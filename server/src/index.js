import os from 'node:os';
import { createApp } from './app.js';
import { createConfig } from './config.js';
import { isSharpAvailable } from './media.js';

function getLanAddresses() {
  const nets = os.networkInterfaces();
  const ips = [];
  for (const list of Object.values(nets)) {
    for (const iface of list || []) {
      if (iface.family === 'IPv4' && !iface.internal) {
        ips.push(iface.address);
      }
    }
  }
  return ips;
}

const config = createConfig();
const server = createApp({ config });

server.listen(config.port, config.host, async () => {
  const sharpOk = await isSharpAvailable();
  console.log(`\n=== VitaGram Companion Proxy ===`);
  console.log(`Mode          : ${config.isLive ? 'LIVE (Instagram sessionid configured)' : 'DEMO (offline mock feed)'}`);
  console.log(`Sharp/WebP    : ${sharpOk ? 'enabled' : 'not installed (demo JPEGs work; run "npm install" for live WebP conversion)'}`);
  console.log(`Vita3K URL    : http://127.0.0.1:${config.port}`);
  for (const ip of getLanAddresses()) {
    console.log(`PS Vita LAN   : http://${ip}:${config.port}`);
  }
  console.log(`Health check  : http://127.0.0.1:${config.port}/api/health\n`);
});

