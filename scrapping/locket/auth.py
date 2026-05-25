import os
import time
import httpx
from dotenv import load_dotenv

load_dotenv()

API_KEY = os.environ["FIREBASE_API_KEY"]
SIGNIN_URL = f"https://www.googleapis.com/identitytoolkit/v3/relyingparty/verifyPassword?key={API_KEY}"
REFRESH_URL = f"https://securetoken.googleapis.com/v1/token?key={API_KEY}"

# Spoof the iOS app — Locket's Firebase API key is restricted to this bundle ID
FIREBASE_HEADERS = {
    "Content-Type": "application/json",
    "Accept-Language": "en-US",
    "User-Agent": "FirebaseAuth.iOS/10.23.1 com.locket.Locket/1.82.0 iPhone/18.0 hw/iPhone12_1",
    "X-Ios-Bundle-Identifier": "com.locket.Locket",
    "X-Client-Version": "iOS/FirebaseSDK/10.23.1/FirebaseCore-iOS",
    "X-Firebase-GMPID": "1:641029076083:ios:cc8eb46290d69b234fa606",
    "X-Firebase-Client": "H4sIAAAAAAAAAKtWykhNLCpJSk0sKVayio7VUSpLLSrOzM9TslIyUqoFAFyivEQfAAAA",
}


class LocketAuth:
    def __init__(self, email: str, password: str):
        self.email = email
        self.password = password
        self.id_token: str | None = None
        self.refresh_token: str | None = None
        self.expires_at: float = 0

    def sign_in(self) -> None:
        r = httpx.post(
            SIGNIN_URL,
            headers=FIREBASE_HEADERS,
            json={
                "email": self.email,
                "password": self.password,
                "returnSecureToken": True,
                "clientType": "CLIENT_TYPE_IOS",
            },
        )
        if r.status_code != 200:
            print("Sign-in failed:", r.status_code, r.json())
            r.raise_for_status()
        data = r.json()
        self.id_token = data["idToken"]
        self.refresh_token = data["refreshToken"]
        self.expires_at = time.time() + int(data["expiresIn"]) - 60

    def refresh(self) -> None:
        r = httpx.post(
            REFRESH_URL,
            headers=FIREBASE_HEADERS,
            json={
                "grantType": "refresh_token",
                "refreshToken": self.refresh_token,
            },
        )
        r.raise_for_status()
        data = r.json()
        self.id_token = data["id_token"]
        self.refresh_token = data["refresh_token"]
        self.expires_at = time.time() + int(data["expires_in"]) - 60

    def token(self) -> str:
        if self.id_token is None:
            self.sign_in()
        elif time.time() >= self.expires_at:
            self.refresh()
        return self.id_token