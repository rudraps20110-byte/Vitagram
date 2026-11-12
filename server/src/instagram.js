import crypto from 'node:crypto';

/**
 * Live Instagram Web API adapter using browser session cookies (sessionid / csrftoken).
 * Includes response caching (to avoid HTTP 429 rate-limits) and DM inbox/thread/send support.
 */

const IG_APP_ID = '936619743392459';
const USER_AGENT =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36';

const apiCache = new Map();
const CACHE_TTL_MS = 45 * 1000;
let autoCsrfToken = '';

function getCached(key, allowStale = false) {
  const entry = apiCache.get(key);
  if (!entry) return null;
  if (!allowStale && Date.now() - entry.ts > CACHE_TTL_MS) return null;
  return entry.data;
}

function setCached(key, data) {
  apiCache.set(key, { ts: Date.now(), data });
  if (apiCache.size > 60) {
    const oldest = apiCache.keys().next().value;
    apiCache.delete(oldest);
  }
}

function mediaProxyUrl(rawUrl, width = 480) {
  if (!rawUrl) return '';
  const encoded = Buffer.from(rawUrl, 'utf8').toString('base64url');
  return `/api/media?u=${encoded}&w=${width}`;
}

function formatRelativeTime(unixSeconds) {
  if (!unixSeconds) return '';
  // Instagram DM timestamps are sometimes in microseconds (16 digits)
  let secs = Number(unixSeconds);
  if (secs > 1e12) secs = Math.floor(secs / 1e6);
  const diff = Math.max(0, Math.floor(Date.now() / 1000) - secs);
  if (diff < 60) return 'just now';
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  if (diff < 604800) return `${Math.floor(diff / 86400)}d ago`;
  return `${Math.floor(diff / 604800)}w ago`;
}

function pickBestCandidate(candidates = []) {
  if (!Array.isArray(candidates) || candidates.length === 0) return '';
  const sorted = [...candidates].sort((a, b) => (a.width || 0) - (b.width || 0));
  const medium = sorted.find((c) => (c.width || 0) >= 480);
  return (medium || sorted[sorted.length - 1])?.url || '';
}

function resolveUserId(session) {
  if (session.ds_user_id) return String(session.ds_user_id).trim();
  const decoded = decodeURIComponent(session.sessionid || '');
  const first = decoded.split(':')[0];
  return /^\d+$/.test(first) ? first : '';
}

async function ensureCsrfToken(session) {
  if (session.csrftoken) return session.csrftoken;
  if (autoCsrfToken) return autoCsrfToken;
  try {
    const res = await fetch('https://www.instagram.com/accounts/login/', {
      headers: {
        'User-Agent': USER_AGENT,
        Cookie: `sessionid=${session.sessionid}`,
      },
    });
    const setCookies = typeof res.headers.getSetCookie === 'function'
      ? res.headers.getSetCookie()
      : [res.headers.get('set-cookie') || ''];
    for (const c of setCookies) {
      const m = /csrftoken=([^;]+)/i.exec(c);
      if (m && m[1]) {
        autoCsrfToken = m[1];
        break;
      }
    }
  } catch {
    // Best-effort only
  }
  return autoCsrfToken;
}

async function buildHeaders(session, extra = {}) {
  const csrf = await ensureCsrfToken(session);
  const dsUserId = resolveUserId(session);
  const cookieParts = [`sessionid=${session.sessionid}`];
  if (csrf) cookieParts.push(`csrftoken=${csrf}`);
  if (dsUserId) cookieParts.push(`ds_user_id=${dsUserId}`);

  return {
    'User-Agent': USER_AGENT,
    Accept: '*/*',
    'Accept-Language': 'en-US,en;q=0.9',
    'X-IG-App-ID': IG_APP_ID,
    'X-ASBD-ID': '129477',
    'X-IG-WWW-Claim': '0',
    'X-Requested-With': 'XMLHttpRequest',
    ...(csrf ? { 'X-CSRFToken': csrf } : {}),
    Cookie: cookieParts.join('; '),
    Referer: 'https://www.instagram.com/',
    Origin: 'https://www.instagram.com',
    ...extra,
  };
}

async function igRequest(url, session, { method = 'GET', formBody = null } = {}) {
  const headers = await buildHeaders(
    session,
    formBody ? { 'Content-Type': 'application/x-www-form-urlencoded' } : {}
  );
  const res = await fetch(url, {
    method,
    headers,
    ...(formBody ? { body: formBody } : {}),
  });

  if (!res.ok) {
    const body = await res.text().catch(() => '');
    const hint =
      res.status === 429
        ? 'Instagram rate-limited (HTTP 429). Wait 2-5 min or add IG_CSRFTOKEN to server/.env.'
        : `Instagram API HTTP ${res.status}: ${body.slice(0, 140)}`;
    const err = new Error(hint);
    err.status = res.status;
    throw err;
  }
  return res.json();
}

export async function fetchLiveFeed(session, maxId = '', limit = 10) {
  const cacheKey = `feed:${session.sessionid.slice(-10)}:${maxId}:${limit}`;
  const cached = getCached(cacheKey);
  if (cached) return cached;

  const count = String(Math.max(1, Math.min(20, Number(limit) || 10)));
  let data;
  try {
    const params = new URLSearchParams({ count });
    if (maxId) params.set('max_id', maxId);
    data = await igRequest(
      `https://www.instagram.com/api/v1/feed/timeline/?${params.toString()}`,
      session
    );
  } catch (err) {
    // Fallback to stale cached feed on 429 if we have one, otherwise try POST once on 400/405
    if (err.status === 429) {
      const stale = getCached(cacheKey, true);
      if (stale) return stale;
      throw err;
    }
    if (err.status === 400 || err.status === 405) {
      const form = new URLSearchParams({
        is_prefetch: '0',
        feed_view_info: '[]',
        seen_posts: '',
        phone_id: crypto.randomUUID(),
        reason: maxId ? 'pagination' : 'cold_start_fetch',
        ...(maxId ? { max_id: maxId } : {}),
      });
      data = await igRequest('https://www.instagram.com/api/v1/feed/timeline/', session, {
        method: 'POST',
        formBody: form.toString(),
      });
    } else {
      throw err;
    }
  }

  const items = Array.isArray(data.feed_items) ? data.feed_items : [];
  const posts = [];

  for (const entry of items) {
    const media = entry.media_or_ad;
    if (!media || media.ad_id) continue;

    let imageRaw = '';
    if (Array.isArray(media.carousel_media) && media.carousel_media.length > 0) {
      imageRaw = pickBestCandidate(media.carousel_media[0]?.image_versions2?.candidates);
    } else {
      imageRaw = pickBestCandidate(media.image_versions2?.candidates);
    }
    if (!imageRaw) continue;

    const user = media.user || {};
    posts.push({
      id: String(media.pk || media.id || ''),
      username: user.username || 'unknown',
      full_name: user.full_name || user.username || '',
      avatar_url: mediaProxyUrl(user.profile_pic_url || '', 96),
      image_url: mediaProxyUrl(imageRaw, 480),
      caption: media.caption?.text || '',
      like_count: Number(media.like_count || 0),
      comment_count: Number(media.comment_count || 0),
      timestamp_text: formatRelativeTime(media.taken_at),
      is_video: media.media_type === 2,
    });
  }

  const result = {
    posts,
    next_max_id: data.next_max_id ? String(data.next_max_id) : '',
    has_more: Boolean(data.more_available && data.next_max_id),
  };
  setCached(cacheKey, result);
  return result;
}

export async function searchLiveUsers(session, query) {
  const q = query.trim();
  if (!q) return { users: [] };
  const cacheKey = `search:${q.toLowerCase()}`;
  const cached = getCached(cacheKey);
  if (cached) return cached;

  const params = new URLSearchParams({
    context: 'blended',
    query: q,
    include_reel: 'false',
  });
  const data = await igRequest(
    `https://www.instagram.com/api/v1/web/search/topsearch/?${params.toString()}`,
    session
  );

  const rawUsers = Array.isArray(data.users) ? data.users : [];
  const result = {
    users: rawUsers.slice(0, 20).map((item) => {
      const u = item.user || item;
      return {
        username: u.username || '',
        full_name: u.full_name || '',
        avatar_url: mediaProxyUrl(u.profile_pic_url || '', 96),
        is_verified: Boolean(u.is_verified),
        follower_count: Number(u.follower_count || 0),
      };
    }),
  };
  setCached(cacheKey, result);
  return result;
}

export async function fetchLiveProfile(session, username) {
  const cacheKey = `profile:${username.toLowerCase()}`;
  const cached = getCached(cacheKey);
  if (cached) return cached;

  const params = new URLSearchParams({ username });
  let data;
  try {
    data = await igRequest(
      `https://www.instagram.com/api/v1/users/web_profile_info/?${params.toString()}`,
      session
    );
  } catch (err) {
    if (err.status === 429) {
      const stale = getCached(cacheKey, true);
      if (stale) return stale;
    }
    throw err;
  }

  const user = data?.data?.user;
  if (!user) return null;

  const edges = user.edge_owner_to_timeline_media?.edges || [];
  const posts = edges.slice(0, 18).map((edge) => {
    const node = edge.node || {};
    const captionEdges = node.edge_media_to_caption?.edges || [];
    return {
      id: String(node.id || ''),
      username: user.username || username,
      full_name: user.full_name || user.username || '',
      avatar_url: mediaProxyUrl(user.profile_pic_url || '', 96),
      image_url: mediaProxyUrl(node.display_url || node.thumbnail_src || '', 480),
      caption: captionEdges[0]?.node?.text || '',
      like_count: Number(node.edge_liked_by?.count ?? node.edge_media_preview_like?.count ?? 0),
      comment_count: Number(node.edge_media_to_comment?.count ?? 0),
      timestamp_text: formatRelativeTime(node.taken_at_timestamp),
      is_video: Boolean(node.is_video),
    };
  });

  const result = {
    username: user.username || username,
    full_name: user.full_name || '',
    bio: user.biography || '',
    avatar_url: mediaProxyUrl(user.profile_pic_url_hd || user.profile_pic_url || '', 96),
    is_verified: Boolean(user.is_verified),
    is_private: Boolean(user.is_private),
    post_count: Number(user.edge_owner_to_timeline_media?.count || posts.length),
    follower_count: Number(user.edge_followed_by?.count || 0),
    following_count: Number(user.edge_follow?.count || 0),
    posts,
  };
  setCached(cacheKey, result);
  return result;
}

function summarizeDmItem(item) {
  if (!item) return '';
  if (item.text) return item.text;
  if (item.item_type === 'media' || item.media) return '[Photo/Video]';
  if (item.item_type === 'clip' || item.clip) return '[Reel shared]';
  if (item.item_type === 'media_share' || item.media_share) return '[Post shared]';
  if (item.item_type === 'voice_media') return '[Voice message]';
  if (item.item_type === 'like') return '<3';
  if (item.item_type === 'link' && item.link?.text) return item.link.text;
  return `[${item.item_type || 'Message'}]`;
}

export async function fetchLiveInbox(session) {
  const cacheKey = `inbox:${session.sessionid.slice(-10)}`;
  const cached = getCached(cacheKey);
  if (cached) return cached;

  const params = new URLSearchParams({
    visual_message_return_type: 'unseen',
    thread_message_limit: '10',
    persistentBadging: 'true',
    limit: '20',
  });
  let data;
  try {
    data = await igRequest(
      `https://www.instagram.com/api/v1/direct_v2/inbox/?${params.toString()}`,
      session
    );
  } catch (err) {
    if (err.status === 429) {
      const stale = getCached(cacheKey, true);
      if (stale) return stale;
    }
    throw err;
  }

  const rawThreads = data?.inbox?.threads || [];
  const threads = rawThreads.map((t) => {
    const users = Array.isArray(t.users) ? t.users : [];
    const title = t.thread_title || users.map((u) => u.username).filter(Boolean).join(', ') || 'Direct Chat';
    const avatarRaw = users[0]?.profile_pic_url || '';
    const lastItem = Array.isArray(t.items) && t.items.length > 0 ? t.items[0] : null;
    return {
      id: String(t.thread_id || ''),
      title,
      avatar_url: mediaProxyUrl(avatarRaw, 96),
      last_message: summarizeDmItem(lastItem),
      timestamp_text: formatRelativeTime(lastItem?.timestamp || t.last_activity_at),
      unread: Number(t.read_state || 0) > 0,
    };
  });

  const result = { threads };
  setCached(cacheKey, result);
  return result;
}

export async function fetchLiveThread(session, threadId) {
  const cacheKey = `thread:${threadId}`;
  const cached = getCached(cacheKey);
  if (cached) return cached;

  const params = new URLSearchParams({
    visual_message_return_type: 'unseen',
    limit: '20',
  });
  let data;
  try {
    data = await igRequest(
      `https://www.instagram.com/api/v1/direct_v2/threads/${encodeURIComponent(threadId)}/?${params.toString()}`,
      session
    );
  } catch (err) {
    if (err.status === 429) {
      const stale = getCached(cacheKey, true);
      if (stale) return stale;
    }
    throw err;
  }

  const t = data?.thread;
  if (!t) return null;

  const myUserId = resolveUserId(session);
  const userMap = new Map();
  for (const u of t.users || []) {
    userMap.set(String(u.pk || u.id || ''), u.username || 'user');
  }
  const users = Array.isArray(t.users) ? t.users : [];
  const title = t.thread_title || users.map((u) => u.username).filter(Boolean).join(', ') || 'Direct Chat';
  const avatarRaw = users[0]?.profile_pic_url || '';

  // Instagram returns newest-first; reverse to chronological (oldest -> newest)
  const items = Array.isArray(t.items) ? [...t.items].reverse() : [];
  const messages = items.map((item) => {
    const uid = String(item.user_id || '');
    const isMe = Boolean(myUserId && uid === myUserId);
    return {
      id: String(item.item_id || ''),
      sender: isMe ? 'you' : (userMap.get(uid) || title),
      is_me: isMe,
      text: summarizeDmItem(item),
      timestamp_text: formatRelativeTime(item.timestamp),
    };
  });

  const result = {
    id: String(t.thread_id || threadId),
    title,
    avatar_url: mediaProxyUrl(avatarRaw, 96),
    messages,
  };
  setCached(cacheKey, result);
  return result;
}

export async function sendLiveMessage(session, threadId, text) {
  const cleanText = String(text || '').trim();
  if (!cleanText) {
    const err = new Error('Message text cannot be empty');
    err.status = 400;
    throw err;
  }
  const csrf = await ensureCsrfToken(session);
  const clientContext = crypto.randomUUID();
  const form = new URLSearchParams({
    action: 'send_item',
    thread_ids: JSON.stringify([String(threadId)]),
    text: cleanText,
    client_context: clientContext,
    mutation_token: clientContext,
    ...(csrf ? { _csrftoken: csrf } : {}),
  });

  const data = await igRequest(
    'https://www.instagram.com/api/v1/direct_v2/threads/broadcast/text/',
    session,
    { method: 'POST', formBody: form.toString() }
  );

  // Invalidate thread & inbox cache so next read shows the sent message
  apiCache.delete(`thread:${threadId}`);
  apiCache.delete(`inbox:${session.sessionid.slice(-10)}`);

  return {
    ok: true,
    message: {
      id: String(data?.payload?.item_id || clientContext),
      sender: 'you',
      is_me: true,
      text: cleanText,
      timestamp_text: 'just now',
    },
  };
}
