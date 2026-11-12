/**
 * Live Instagram Web API adapter using browser session cookies (sessionid / csrftoken).
 * All responses are normalized into lightweight Vita-friendly JSON objects and
 * image URLs are rewritten to pass through /api/media for baseline JPEG conversion.
 */

const IG_APP_ID = '936619743392459';
const USER_AGENT =
  'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36';

function mediaProxyUrl(rawUrl, width = 480) {
  if (!rawUrl) return '';
  const encoded = Buffer.from(rawUrl, 'utf8').toString('base64url');
  return `/api/media?u=${encoded}&w=${width}`;
}

function formatRelativeTime(unixSeconds) {
  if (!unixSeconds) return '';
  const diff = Math.max(0, Math.floor(Date.now() / 1000) - Number(unixSeconds));
  if (diff < 60) return 'just now';
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  if (diff < 604800) return `${Math.floor(diff / 86400)}d ago`;
  return `${Math.floor(diff / 604800)}w ago`;
}

function pickBestCandidate(candidates = []) {
  if (!Array.isArray(candidates) || candidates.length === 0) return '';
  // Pick a medium candidate (around 480-720px wide) to avoid downloading huge 1440px files
  const sorted = [...candidates].sort((a, b) => (a.width || 0) - (b.width || 0));
  const medium = sorted.find((c) => (c.width || 0) >= 480);
  return (medium || sorted[sorted.length - 1])?.url || '';
}

function buildCookieHeader(session) {
  const parts = [`sessionid=${session.sessionid}`];
  if (session.csrftoken) parts.push(`csrftoken=${session.csrftoken}`);
  if (session.ds_user_id) parts.push(`ds_user_id=${session.ds_user_id}`);
  return parts.join('; ');
}

async function igFetchJson(url, session) {
  const res = await fetch(url, {
    headers: {
      'User-Agent': USER_AGENT,
      Accept: '*/*',
      'Accept-Language': 'en-US,en;q=0.9',
      'X-IG-App-ID': IG_APP_ID,
      'X-Requested-With': 'XMLHttpRequest',
      ...(session.csrftoken ? { 'X-CSRFToken': session.csrftoken } : {}),
      Cookie: buildCookieHeader(session),
      Referer: 'https://www.instagram.com/',
    },
  });

  if (!res.ok) {
    const body = await res.text().catch(() => '');
    const err = new Error(`Instagram API HTTP ${res.status}: ${body.slice(0, 160)}`);
    err.status = res.status;
    throw err;
  }
  return res.json();
}

export async function fetchLiveFeed(session, maxId = '', limit = 10) {
  const params = new URLSearchParams();
  if (maxId) params.set('max_id', maxId);
  params.set('count', String(Math.max(1, Math.min(20, Number(limit) || 10))));

  const data = await igFetchJson(
    `https://www.instagram.com/api/v1/feed/timeline/?${params.toString()}`,
    session
  );

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

  return {
    posts,
    next_max_id: data.next_max_id ? String(data.next_max_id) : '',
    has_more: Boolean(data.more_available && data.next_max_id),
  };
}

export async function searchLiveUsers(session, query) {
  const q = query.trim();
  if (!q) return { users: [] };
  const params = new URLSearchParams({
    context: 'blended',
    query: q,
    include_reel: 'false',
  });
  const data = await igFetchJson(
    `https://www.instagram.com/api/v1/web/search/topsearch/?${params.toString()}`,
    session
  );

  const rawUsers = Array.isArray(data.users) ? data.users : [];
  return {
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
}

export async function fetchLiveProfile(session, username) {
  const params = new URLSearchParams({ username });
  const data = await igFetchJson(
    `https://www.instagram.com/api/v1/users/web_profile_info/?${params.toString()}`,
    session
  );

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

  return {
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
}

