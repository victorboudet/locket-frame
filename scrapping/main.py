import os
from pathlib import Path
from dotenv import load_dotenv
from locket.auth import LocketAuth
from locket.client import LocketClient
from locket.downloader import download_moment

load_dotenv()

DOWNLOADS = Path("downloads")

email = os.environ.get("LOCKET_EMAIL")
password = os.environ.get("LOCKET_PASSWORD")
if not email or not password:
    raise SystemExit(
        "LOCKET_EMAIL / LOCKET_PASSWORD are not set — copy .env.example to .env and fill them in"
    )

auth = LocketAuth(email, password)
client = LocketClient(auth)

result = client.get_latest_moment()
moments = result.get("data", [])

print(f"Got {len(moments)} moment(s), {result.get('missed_moments_count', 0)} missed")

for m in moments:
    path = download_moment(m, DOWNLOADS)
    if path:
        print(f"  ✓ {path.name}")