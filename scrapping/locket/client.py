import httpx
from .auth import LocketAuth

BASE_URL = "https://api.locketcamera.com"


class LocketClient:
    def __init__(self, auth: LocketAuth):
        self.auth = auth

    def _call(self, endpoint: str, data: dict) -> dict:
        r = httpx.post(
            f"{BASE_URL}/{endpoint}",
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.auth.token()}",
            },
            json={"data": data},
            timeout=30.0,
        )
        r.raise_for_status()
        return r.json()["result"]

    def get_latest_moment(self, last_fetch: int = 1) -> dict:
        """
        last_fetch is a Unix timestamp in seconds.
        Returns the latest moment(s) created after that timestamp.
        Pass 1 to get the absolute latest.
        """
        return self._call("getLatestMomentV2", {
            "last_fetch": last_fetch,
            "should_count_missed_moments": True,
        })

    def fetch_user(self, uid: str) -> dict:
        return self._call("fetchUserV2", {"user_uid": uid})