import crypto from 'node:crypto';
import { renderAvatarJpeg, renderDemoPhotoJpeg } from './jpeg_gen.js';

let sharpLoader = null;

async function getSharp() {
  if (sharpLoader !== null) return sharpLoader;
  try {
    const mod = await import('sharp');
    sharpLoader = mod.default || mod;
  } catch {
    sharpLoader = false;
  }
  return sharpLoader;
}

export async function isSharpAvailable() {
  return Boolean(await getSharp());
}

/**
 * Bounded LRU cache for transcoded baseline JPEGs so Vita feed scrolling stays snappy.
 */
export function createMediaService({ maxCacheItems = 80, fetchImpl = globalThis.fetch } = {}) {
  const cache = new Map();

  function getCached(key) {
    if (!cache.has(key)) return null;
    const val = cache.get(key);
    cache.delete(key);
    cache.set(key, val);
    return val;
  }

  function setCached(key, buf) {
    if (cache.has(key)) cache.delete(key);
    cache.set(key, buf);
    while (cache.size > maxCacheItems) {
      const oldestKey = cache.keys().next().value;
      cache.delete(oldestKey);
    }
  }

  async function getMediaJpeg(searchParams) {
    const demoScene = searchParams.get('demo_scene');
    const demoAvatar = searchParams.get('demo_avatar');
    const targetWidth = Math.max(64, Math.min(960, Number.parseInt(searchParams.get('w') || '480', 10) || 480));

    if (demoScene) {
      const key = `demo_scene:${demoScene}:${targetWidth}`;
      const cached = getCached(key);
      if (cached) return cached;
      const height = Math.round((targetWidth * 3) / 4);
      const jpeg = renderDemoPhotoJpeg(demoScene, targetWidth, height);
      setCached(key, jpeg);
      return jpeg;
    }

    if (demoAvatar !== null) {
      const seed = Number.parseInt(demoAvatar, 10) || 0;
      const key = `demo_avatar:${seed}`;
      const cached = getCached(key);
      if (cached) return cached;
      const jpeg = renderAvatarJpeg(seed, 96);
      setCached(key, jpeg);
      return jpeg;
    }

    const encodedUrl = searchParams.get('u');
    if (!encodedUrl) {
      const err = new Error('Missing media parameter (u, demo_scene, or demo_avatar)');
      err.status = 400;
      throw err;
    }

    let remoteUrl = '';
    try {
      remoteUrl = Buffer.from(encodedUrl, 'base64url').toString('utf8');
      const parsed = new URL(remoteUrl);
      if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
        throw new Error('Unsupported protocol');
      }
    } catch {
      const err = new Error('Invalid media URL');
      err.status = 400;
      throw err;
    }

    const cacheKey = crypto.createHash('sha1').update(`${remoteUrl}|${targetWidth}`).digest('hex');
    const cached = getCached(cacheKey);
    if (cached) return cached;

    const res = await fetchImpl(remoteUrl, {
      headers: {
        'User-Agent':
          'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36',
        Referer: 'https://www.instagram.com/',
      },
    });
    if (!res.ok) {
      const err = new Error(`Upstream media HTTP ${res.status}`);
      err.status = 502;
      throw err;
    }

    const rawBuf = Buffer.from(await res.arrayBuffer());
    const contentType = (res.headers.get('content-type') || '').toLowerCase();
    const sharp = await getSharp();

    let outputBuf;
    if (sharp) {
      outputBuf = await sharp(rawBuf)
        .rotate()
        .resize({ width: targetWidth, height: targetWidth, fit: 'inside', withoutEnlargement: true })
        .jpeg({ quality: 82, progressive: false, mozjpeg: false })
        .toBuffer();
    } else if (contentType.includes('jpeg') || contentType.includes('jpg') || (rawBuf[0] === 0xff && rawBuf[1] === 0xd8)) {
      outputBuf = rawBuf;
    } else {
      // Fallback baseline JPEG notice if sharp is unavailable for WebP/AVIF transcoding
      outputBuf = renderDemoPhotoJpeg('vita', Math.min(targetWidth, 480), 360);
    }

    setCached(cacheKey, outputBuf);
    return outputBuf;
  }

  return { getMediaJpeg };
}

