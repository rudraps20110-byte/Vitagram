import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { Readable } from 'node:stream';
import test from 'node:test';
import { createRequestHandler } from '../src/app.js';
import { createConfig } from '../src/config.js';
import { createMediaService } from '../src/media.js';

function createTestHarness(options = {}) {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'vitagram-test-'));
  const config = createConfig({ dataDir: tmpDir, port: 8787, host: '127.0.0.1' });
  const handler = createRequestHandler({ config, ...options });

  async function request(method, urlPath, bodyObj = null) {
    const payload = bodyObj ? Buffer.from(JSON.stringify(bodyObj), 'utf8') : null;
    const req = Readable.from(payload ? [payload] : []);
    req.method = method;
    req.url = urlPath;
    req.headers = {
      host: '127.0.0.1:8787',
      ...(payload ? { 'content-type': 'application/json' } : {}),
    };

    const chunks = [];
    let statusCode = 200;
    let headers = {};
    const res = {
      writeHead(code, hdrs = {}) {
        statusCode = code;
        for (const [k, v] of Object.entries(hdrs)) {
          headers[k.toLowerCase()] = v;
        }
      },
      end(data) {
        if (data) chunks.push(Buffer.isBuffer(data) ? data : Buffer.from(data));
      },
    };

    await handler(req, res);
    const body = Buffer.concat(chunks);
    return {
      status: statusCode,
      headers,
      body,
      json() {
        return JSON.parse(body.toString('utf8'));
      },
    };
  }

  return {
    config,
    request,
    close() {
      fs.rmSync(tmpDir, { recursive: true, force: true });
    },
  };
}

test('health endpoint reports demo mode by default', async () => {
  const srv = createTestHarness();
  try {
    const res = await srv.request('GET', '/api/health');
    assert.equal(res.status, 200);
    const data = res.json();
    assert.equal(data.ok, true);
    assert.equal(data.mode, 'demo');
  } finally {
    srv.close();
  }
});

test('feed pagination returns demo posts and next_max_id', async () => {
  const srv = createTestHarness();
  try {
    const page1 = (await srv.request('GET', '/api/feed?limit=5')).json();
    assert.equal(page1.posts.length, 5);
    assert.equal(page1.has_more, true);
    assert.equal(page1.next_max_id, '5');

    const page2 = (
      await srv.request('GET', `/api/feed?limit=5&max_id=${encodeURIComponent(page1.next_max_id)}`)
    ).json();
    assert.equal(page2.posts.length, 3);
    assert.equal(page2.has_more, false);
    assert.equal(page2.next_max_id, '');
  } finally {
    srv.close();
  }
});

test('search and profile endpoints return matching demo users and posts', async () => {
  const srv = createTestHarness();
  try {
    const search = (await srv.request('GET', '/api/search?q=vita')).json();
    assert.ok(search.users.length >= 1);
    assert.equal(search.users[0].username, 'vita_explorer');

    const profileRes = await srv.request('GET', '/api/profile/vita_explorer');
    assert.equal(profileRes.status, 200);
    const profile = profileRes.json();
    assert.equal(profile.username, 'vita_explorer');
    assert.ok(profile.posts.length >= 3);

    const missing = await srv.request('GET', '/api/profile/nonexistent_user_xyz');
    assert.equal(missing.status, 404);
  } finally {
    srv.close();
  }
});

test('media endpoint renders valid baseline JPEGs for demo scenes and avatars', async () => {
  const srv = createTestHarness();
  try {
    const sceneRes = await srv.request('GET', '/api/media?demo_scene=vita&w=320');
    assert.equal(sceneRes.status, 200);
    assert.equal(sceneRes.headers['content-type'], 'image/jpeg');
    const sceneBuf = sceneRes.body;
    assert.equal(sceneBuf[0], 0xff);
    assert.equal(sceneBuf[1], 0xd8);
    assert.equal(sceneBuf[sceneBuf.length - 2], 0xff);
    assert.equal(sceneBuf[sceneBuf.length - 1], 0xd9);

    const avatarRes = await srv.request('GET', '/api/media?demo_avatar=2');
    assert.equal(avatarRes.status, 200);
    const avatarBuf = avatarRes.body;
    assert.equal(avatarBuf[0], 0xff);
    assert.equal(avatarBuf[1], 0xd8);
  } finally {
    srv.close();
  }
});

test('session endpoint switches between demo and live mode', async () => {
  let calledLive = false;
  const mockLiveApi = {
    async fetchLiveFeed(session) {
      calledLive = session.sessionid === 'abc123';
      return { posts: [], next_max_id: '', has_more: false };
    },
    async searchLiveUsers() {
      return { users: [] };
    },
    async fetchLiveProfile() {
      return null;
    },
  };

  const srv = createTestHarness({ liveApi: mockLiveApi });
  try {
    const authRes = await srv.request('POST', '/api/auth/session', {
      sessionid: 'abc123',
      username: 'my_acct',
    });
    assert.equal(authRes.status, 200);
    assert.equal(authRes.json().mode, 'live');

    await srv.request('GET', '/api/feed');
    assert.equal(calledLive, true);
  } finally {
    srv.close();
  }
});

test('media endpoint rejects invalid URLs and handles upstream JPEG pass-through', async () => {
  const fakeJpeg = Buffer.from([0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0xff, 0xd9]);
  const mediaService = createMediaService({
    fetchImpl: async () =>
      new Response(fakeJpeg, {
        status: 200,
        headers: { 'content-type': 'image/jpeg' },
      }),
  });
  const srv = createTestHarness({ mediaService });
  try {
    const bad = await srv.request('GET', '/api/media?u=not_a_url');
    assert.equal(bad.status, 400);

    const encoded = Buffer.from('https://example.com/photo.jpg', 'utf8').toString('base64url');
    const ok = await srv.request('GET', `/api/media?u=${encoded}&w=480`);
    assert.equal(ok.status, 200);
    assert.equal(ok.headers['content-type'], 'image/jpeg');
    assert.deepEqual(ok.body, fakeJpeg);
  } finally {
    srv.close();
  }
});
