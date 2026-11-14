import contextlib
import io
import json
import os
import unittest
from unittest.mock import Mock, patch

from curl_cffi.requests import Response
from curl_cffi.requests.exceptions import HTTPError

with patch.dict(os.environ, {"FIREBASE_API_KEY": "test-key"}):
    from locket.client import LocketClient


def response(body, status=200):
    result = Response()
    result.status_code = status
    result.ok = status < 400
    result.content = json.dumps(body).encode()
    return result


class LocketClientTests(unittest.TestCase):
    def setUp(self):
        self.auth = Mock()
        self.auth.token.return_value = "test-token"
        self.client = LocketClient(self.auth)

    def test_returns_successful_feed(self):
        feed = {"status": 200, "data": [{"canonical_uid": "moment-1"}]}
        with patch("locket.client.requests.post", return_value=response({"result": feed})):
            self.assertEqual(self.client.get_latest_moment(), feed)

    def test_http_200_application_errors_are_not_empty_feeds(self):
        for result in (
            {"errors": ["Please sign in"], "status": 401},
            {"status": 403},
            {"errors": ["Permission denied"]},
        ):
            with self.subTest(result=result):
                with patch("locket.client.requests.post", return_value=response({"result": result})):
                    with self.assertRaisesRegex(RuntimeError, "getLatestMomentV2: API error"):
                        self.client.get_latest_moment()

    def test_html_403_preserves_http_error(self):
        forbidden = response(None, 403)
        forbidden.content = b"<!doctype html><title>403</title>403 Forbidden"
        with patch("locket.client.requests.post", return_value=forbidden):
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(HTTPError):
                    self.client.get_latest_moment()

    def test_invalid_response_shapes_raise_clear_error(self):
        for body in ({}, {"result": None}, {"result": []}, []):
            with self.subTest(body=body):
                with patch("locket.client.requests.post", return_value=response(body)):
                    with self.assertRaisesRegex(RuntimeError, "unexpected response shape"):
                        self.client.get_latest_moment()


if __name__ == "__main__":
    unittest.main()
