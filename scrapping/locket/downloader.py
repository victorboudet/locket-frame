from pathlib import Path
from datetime import datetime, timezone
from urllib.parse import urlparse
import httpx

KNOWN_EXTENSIONS = {".webp", ".jpg", ".jpeg", ".png"}


def download_moment(moment: dict, out_dir: Path) -> Path | None:
    """Download a single moment's thumbnail. Returns the saved path, or None if skipped."""
    url = moment.get("thumbnail_url")
    if not url:
        return None

    # Build a meaningful filename: 2025-11-20_x10yeCpZmduKPIf4YNBD.webp
    ts = moment["date"]["_seconds"]
    date_str = datetime.fromtimestamp(ts, tz=timezone.utc).strftime("%Y-%m-%d_%H%M%S")
    uid = moment["canonical_uid"]
    ext = Path(urlparse(url).path).suffix.lower()
    if ext not in KNOWN_EXTENSIONS:
        ext = ".jpg"
    dest = out_dir / f"{date_str}_{uid}{ext}"

    out_dir.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        return dest  # skip duplicates

    # Stream to a .part file and rename on success, so an interrupted transfer
    # never leaves a truncated file that the exists() check would skip forever.
    tmp = dest.with_suffix(dest.suffix + ".part")
    try:
        with httpx.stream("GET", url, timeout=60.0) as r:
            r.raise_for_status()
            with open(tmp, "wb") as f:
                for chunk in r.iter_bytes():
                    f.write(chunk)
        tmp.replace(dest)
    except BaseException:
        tmp.unlink(missing_ok=True)
        raise

    return dest