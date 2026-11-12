# VitaGram — Instagram Client for PlayStation Vita

VitaGram is a lightweight Instagram client for PS Vita (and **Vita3K**) built with **VitaSDK (`vita2d` + `SceHttp`)** and a **Node.js Companion Proxy Server**.

## Why a Companion Proxy?
Instagram does not offer a public API for personal feeds, blocks older TLS stacks with anti-bot fingerprinting, and serves WebP/AVIF images that the PS Vita cannot easily decode natively. The companion proxy solves this cleanly:
- Handles Instagram session authentication & modern HTTPS/TLS on your PC
- Transcodes and resizes images to Vita-friendly **baseline JPEGs** (`<= 480px` wide, cached in memory)
- Includes an **offline Demo Mode** out of the box so you can test the entire UI in Vita3K immediately without logging in

---

## 1. Run the Companion Proxy Server (`server/`)

Requires **Node.js 20+** (already installed if you are on Node 24).

```powershell
cd server
npm start
```

- Runs on `http://0.0.0.0:8787`
- **Vita3K URL**: `http://127.0.0.1:8787` (default in the Vita app)
- **Real PS Vita URL**: `http://<YOUR_PC_LAN_IP>:8787` (printed at startup)

### Optional: Enable Live Instagram Mode
1. Copy `server/.env.example` to `server/.env`.
2. In your desktop browser (logged into `instagram.com`), open DevTools -> Application -> Cookies -> `https://www.instagram.com` and copy `sessionid` (and optionally `csrftoken` and `ds_user_id`) into `server/.env`:
   ```env
   IG_SESSIONID=your_sessionid_cookie
   IG_CSRFTOKEN=your_csrftoken_cookie
   IG_DS_USER_ID=your_ds_user_id
   ```
3. Install `sharp` in `server/` so WebP/AVIF photos from Instagram CDN are converted to baseline JPEG for the PS Vita:
   ```powershell
   cd server
   npm install
   npm start
   ```

> **Note:** Never share your `sessionid` cookie—it grants access to your Instagram session.

### Run Server Tests
```powershell
cd server
npm test
```

---

## 2. Build the PS Vita Homebrew App (`vita/`)

Because VitaSDK targets ARMv7 (`arm-vita-eabi`), you can build `VitaGram.vpk` using any of these options:

### Option A: Docker (Windows / macOS / Linux)
```powershell
cd vita
.\build-docker.ps1
```
Output: `vita/build/VitaGram.vpk`

### Option B: WSL2 (Ubuntu) or Linux with VitaSDK installed
```bash
cd vita
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
make -j$(nproc)
```

### Option C: GitHub Actions (Zero local SDK setup)
Push this repository to GitHub and run the **Build VitaGram VPK** workflow (`.github/workflows/build-vpk.yml`), then download `VitaGram.vpk` from the workflow artifacts.

---

## 3. Controls (Vita3K & PS Vita)

| Control | Action |
|---|---|
| `L` / `R` or tap bottom bar | Switch tabs (`Feed`, `Search`, `Profile`, `Settings`) |
| `D-Pad Up/Down` | Browse feed posts / move in Search or Settings |
| `Cross (X)` | Open author profile / select item / open keyboard |
| `Triangle (/\)` | Zoom photo (Feed) / New search query (Search) |
| `Square ([])` | Refresh feed / Backspace in keyboard |
| `Circle (O)` | Close photo zoom / Back from profile / Cancel keyboard |
| `Select` | Exit VitaGram |

