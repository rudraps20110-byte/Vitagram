import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SERVER_ROOT = path.resolve(__dirname, '..');
const DATA_DIR = path.join(SERVER_ROOT, 'data');
const SESSION_FILE = path.join(DATA_DIR, 'session.json');

/**
 * Load simple key=value entries from a .env file if present.
 */
function loadDotEnv(envPath) {
  if (!fs.existsSync(envPath)) return;
  const lines = fs.readFileSync(envPath, 'utf8').split(/\r?\n/);
  for (const raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const eq = line.indexOf('=');
    if (eq <= 0) continue;
    const key = line.slice(0, eq).trim();
    let val = line.slice(eq + 1).trim();
    if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
      val = val.slice(1, -1);
    }
    if (!(key in process.env)) {
      process.env[key] = val;
    }
  }
}

loadDotEnv(path.join(SERVER_ROOT, '.env'));

export function createConfig(overrides = {}) {
  const port = Number(overrides.port ?? process.env.PORT ?? 8787);
  const host = String(overrides.host ?? process.env.HOST ?? '0.0.0.0');
  const dataDir = overrides.dataDir ?? DATA_DIR;
  const sessionFile = path.join(dataDir, 'session.json');

  let session = overrides.session
    ? {
        sessionid: overrides.session.sessionid || '',
        csrftoken: overrides.session.csrftoken || '',
        ds_user_id: overrides.session.ds_user_id || '',
        username: overrides.session.username || '',
      }
    : {
        sessionid: process.env.IG_SESSIONID || '',
        csrftoken: process.env.IG_CSRFTOKEN || '',
        ds_user_id: process.env.IG_DS_USER_ID || '',
        username: '',
      };

  if (!overrides.session && fs.existsSync(sessionFile)) {
    try {
      const saved = JSON.parse(fs.readFileSync(sessionFile, 'utf8'));
      session = {
        sessionid: saved.sessionid ?? session.sessionid,
        csrftoken: saved.csrftoken ?? session.csrftoken,
        ds_user_id: saved.ds_user_id ?? session.ds_user_id,
        username: saved.username ?? session.username,
      };
    } catch {
      // Ignore corrupt session file and fall back to env/default.
    }
  }

  return {
    port,
    host,
    dataDir,
    cacheDir: path.join(dataDir, 'cache'),
    sessionFile,
    session,
    get isLive() {
      return Boolean(this.session.sessionid && this.session.sessionid.trim().length > 0);
    },
    saveSession(nextSession) {
      this.session = {
        sessionid: (nextSession.sessionid || '').trim(),
        csrftoken: (nextSession.csrftoken || '').trim(),
        ds_user_id: (nextSession.ds_user_id || '').trim(),
        username: (nextSession.username || '').trim(),
      };
      fs.mkdirSync(this.dataDir, { recursive: true });
      fs.writeFileSync(this.sessionFile, JSON.stringify(this.session, null, 2), 'utf8');
    },
  };
}

export { DATA_DIR, SESSION_FILE };

