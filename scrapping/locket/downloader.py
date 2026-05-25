from pathlib import Path
from datetime import datetime, timezone
import httpx


def download_moment(moment: dict, out_dir: Path) -> Path | None:
    """Download a single moment's thumbnail. Returns the saved path, or None if skipped."""
    url = moment.get("thumbnail_url")
    if not url:
        return None

    # Build a meaningful filename: 2025-11-20_x10yeCpZmduKPIf4YNBD.webp
    ts = moment["date"]["_seconds"]
    date_str = datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%Y-%m-%d_%H%M%S")
    uid = moment["canonical_uid"]
    ext = ".webp" if ".webp" in url else ".jpg"
    dest = out_dir / f"{date_str}_{uid}{ext}"

    out_dir.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        return dest  # skip duplicates

    with httpx.stream("GET", url, timeout=60.0) as r:
        r.raise_for_status()
        with open(dest, "wb") as f:
            for chunk in r.iter_bytes():
                f.write(chunk)

    return dest