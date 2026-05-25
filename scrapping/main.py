import os
from pathlib import Path
from dotenv import load_dotenv
from locket.auth import LocketAuth
from locket.client import LocketClient
from locket.downloader import download_moment

load_dotenv()

DOWNLOADS = Path("downloads")

auth = LocketAuth(os.environ["LOCKET_EMAIL"], os.environ["LOCKET_PASSWORD"])
client = LocketClient(auth)

result = client.get_latest_moment()
moments = result.get("data", [])

print(f"Got {len(moments)} moment(s), {result.get('missed_moments_count', 0)} missed")

for m in moments:
    path = download_moment(m, DOWNLOADS)
    if path:
        print(f"  ✓ {path.name}")