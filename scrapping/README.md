# scrapping

Small Python tool that signs into [Locket](https://locket.camera/) through Firebase, calls its private mobile API to fetch the latest moment(s) from your friends, and downloads the thumbnails locally.

This is the scrapping component of the broader `locket-frame` project — its job is to figure out the auth flow and API surface, and produce a stream of image files that downstream code can consume.

## Requirements

- Python 3.14+
- [uv](https://docs.astral.sh/uv/) for dependency and environment management
- A Locket account (email + password)

## Setup

```sh
# Install dependencies into a local .venv
uv sync

# Copy the env template and fill in your Locket credentials
cp .env.example .env
```

`.env` expects:

| Variable           | Description                                                                 |
| ------------------ | --------------------------------------------------------------------------- |
| `LOCKET_EMAIL`     | Your Locket account email.                                                  |
| `LOCKET_PASSWORD`  | Your Locket account password.                                               |
| `FIREBASE_API_KEY` | Public Firebase Web API key for Locket. The template ships with a default. |

## Running

```sh
uv run main.py
```

On success you'll see something like:

```
Got 1 moment(s), 0 missed
  ✓ 2026-05-25_142233_x10yeCpZmduKPIf4YNBD.webp
```

Files land in `./downloads/`, named `YYYY-MM-DD_HHMMSS_<moment_uid>.<ext>`. Re-running skips moments that have already been downloaded.

## How it works

### 1. Auth — `locket/auth.py`

Locket's Firebase project is locked to its iOS bundle ID, so the request headers spoof the iOS Firebase SDK (`User-Agent`, `X-Ios-Bundle-Identifier`, `X-Firebase-GMPID`, …). Without these the API key is rejected.

- `sign_in()` → `identitytoolkit/v3/relyingparty/verifyPassword` returns an `idToken` + `refreshToken`.
- `refresh()` → `securetoken.googleapis.com/v1/token` rotates the token when it's near expiry.
- `token()` lazily signs in on first use and refreshes 60 s before expiration.

### 2. API client — `locket/client.py`

Thin wrapper around `https://api.locketcamera.com`. All endpoints are POST and follow the Firebase Callable Function convention (`{"data": …}` in, `{"result": …}` out).

- `get_latest_moment(last_fetch=1)` — `getLatestMomentV2`, returns the latest moment(s) newer than the given Unix timestamp.
- `fetch_user(uid)` — `fetchUserV2`, profile lookup by UID.

### 3. Downloader — `locket/downloader.py`

Takes a moment dict, pulls its `thumbnail_url`, streams the bytes to `downloads/` with a date-prefixed filename. Returns the destination path, or `None` if the moment has no thumbnail.

### 4. Entry point — `main.py`

Wires the three pieces together: load env → build auth → call `get_latest_moment` → download each result.

## Project layout

```
scrapping/
├── main.py              # entry point
├── locket/
│   ├── auth.py          # Firebase sign-in / refresh (iOS spoof)
│   ├── client.py        # api.locketcamera.com wrapper
│   └── downloader.py    # thumbnail → ./downloads
├── downloads/           # output images
├── .env.example
├── pyproject.toml
└── uv.lock
```

## Notes

- The Firebase API key in `.env.example` is the public web key shipped with the Locket iOS app; it's only useful in combination with the spoofed bundle headers in `auth.py`.
- This relies on undocumented endpoints. They can change without notice — if `getLatestMomentV2` starts returning errors, the request shape is probably what shifted.
- Use a Locket account you own. Don't scrape friends who haven't agreed to it.
