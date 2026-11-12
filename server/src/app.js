import http from 'node:http';
import {
  getDemoFeed,
  getDemoInbox,
  getDemoProfile,
  getDemoThread,
  searchDemoUsers,
  sendDemoMessage,
} from './demo_data.js';
import {
  fetchLiveFeed,
  fetchLiveInbox,
  fetchLiveProfile,
  fetchLiveThread,
  searchLiveUsers,
  sendLiveMessage,
} from './instagram.js';
import { createMediaService, isSharpAvailable } from './media.js';

function sendJson(res, status, payload) {
  const body = Buffer.from(JSON.stringify(payload), 'utf8');
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': body.length,
    'Access-Control-Allow-Origin': '*',
    'Cache-Control': 'no-store',
  });
  res.end(body);
}

function sendJpeg(res, buf) {
  res.writeHead(200, {
    'Content-Type': 'image/jpeg',
    'Content-Length': buf.length,
    'Cache-Control': 'public, max-age=3600',
    'Access-Control-Allow-Origin': '*',
  });
  res.end(buf);
}

async function readJsonBody(req, maxBytes = 64 * 1024) {
  const chunks = [];
  let total = 0;
  for await (const chunk of req) {
    total += chunk.length;
    if (total > maxBytes) {
      const err = new Error('Payload too large');
      err.status = 413;
      throw err;
    }
    chunks.push(chunk);
  }
  const raw = Buffer.concat(chunks).toString('utf8').trim();
  if (!raw) return {};
  return JSON.parse(raw);
}

export function createRequestHandler({ config, mediaService = createMediaService(), liveApi = null } = {}) {
  const ig = liveApi || {
    fetchLiveFeed,
    searchLiveUsers,
    fetchLiveProfile,
    fetchLiveInbox,
    fetchLiveThread,
    sendLiveMessage,
  };

  return async (req, res) => {
    try {
      const reqUrl = new URL(req.url || '/', `http://${req.headers.host || 'localhost'}`);
      const pathname = reqUrl.pathname;

      if (req.method === 'OPTIONS') {
        res.writeHead(204, {
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
          'Access-Control-Allow-Headers': 'Content-Type',
        });
        res.end();
        return;
      }

      if (req.method === 'GET' && pathname === '/api/health') {
        sendJson(res, 200, {
          ok: true,
          app: 'VitaGram Companion Proxy',
          mode: config.isLive ? 'live' : 'demo',
          username: config.session.username || (config.isLive ? 'instagram_user' : 'demo_user'),
          sharp_enabled: await isSharpAvailable(),
        });
        return;
      }

      if (req.method === 'POST' && pathname === '/api/auth/session') {
        const body = await readJsonBody(req);
        config.saveSession({
          sessionid: body.sessionid || '',
          csrftoken: body.csrftoken || '',
          ds_user_id: body.ds_user_id || '',
          username: body.username || '',
        });
        sendJson(res, 200, {
          ok: true,
          mode: config.isLive ? 'live' : 'demo',
        });
        return;
      }

      if (req.method === 'GET' && pathname === '/api/feed') {
        const maxId = reqUrl.searchParams.get('max_id') || '';
        const limit = reqUrl.searchParams.get('limit') || '5';
        const data = config.isLive
          ? await ig.fetchLiveFeed(config.session, maxId, limit)
          : getDemoFeed(maxId, limit);
        sendJson(res, 200, data);
        return;
      }

      if (req.method === 'GET' && pathname === '/api/search') {
        const q = reqUrl.searchParams.get('q') || '';
        const data = config.isLive
          ? await ig.searchLiveUsers(config.session, q)
          : searchDemoUsers(q);
        sendJson(res, 200, data);
        return;
      }

      if (req.method === 'GET' && pathname.startsWith('/api/profile/')) {
        const username = decodeURIComponent(pathname.slice('/api/profile/'.length)).replace(/^@/, '').trim();
        if (!username) {
          sendJson(res, 400, { error: 'Username is required' });
          return;
        }
        const profile = config.isLive
          ? await ig.fetchLiveProfile(config.session, username)
          : getDemoProfile(username);
        if (!profile) {
          sendJson(res, 404, { error: 'User not found' });
          return;
        }
        sendJson(res, 200, profile);
        return;
      }

      if (req.method === 'GET' && pathname === '/api/media') {
        const jpeg = await mediaService.getMediaJpeg(reqUrl.searchParams);
        sendJpeg(res, jpeg);
        return;
      }

      if (req.method === 'GET' && pathname === '/api/messages/inbox') {
        const data = config.isLive
          ? await ig.fetchLiveInbox(config.session)
          : getDemoInbox();
        sendJson(res, 200, data);
        return;
      }

      if (req.method === 'POST' && pathname.startsWith('/api/messages/thread/') && pathname.endsWith('/send')) {
        const rawId = pathname.slice('/api/messages/thread/'.length, -'/send'.length);
        const threadId = decodeURIComponent(rawId).trim();
        if (!threadId) {
          sendJson(res, 400, { error: 'Thread ID is required' });
          return;
        }
        const body = await readJsonBody(req);
        const text = String(body.text || '').trim();
        if (!text) {
          sendJson(res, 400, { error: 'Message text is required' });
          return;
        }
        const result = config.isLive
          ? await ig.sendLiveMessage(config.session, threadId, text)
          : sendDemoMessage(threadId, text);
        if (!result) {
          sendJson(res, 404, { error: 'Conversation not found' });
          return;
        }
        sendJson(res, 200, result);
        return;
      }

      if (req.method === 'GET' && pathname.startsWith('/api/messages/thread/')) {
        const threadId = decodeURIComponent(pathname.slice('/api/messages/thread/'.length)).trim();
        if (!threadId) {
          sendJson(res, 400, { error: 'Thread ID is required' });
          return;
        }
        const thread = config.isLive
          ? await ig.fetchLiveThread(config.session, threadId)
          : getDemoThread(threadId);
        if (!thread) {
          sendJson(res, 404, { error: 'Conversation not found' });
          return;
        }
        sendJson(res, 200, thread);
        return;
      }

      sendJson(res, 404, { error: 'Not found' });
    } catch (err) {
      const status = Number(err?.status) || 500;
      sendJson(res, status, { error: err?.message || 'Internal server error' });
    }
  };
}

export function createApp(options = {}) {
  return http.createServer(createRequestHandler(options));
}
