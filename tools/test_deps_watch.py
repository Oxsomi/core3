# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

# python tools/test_deps_watch.py
# The retry behavior of deps_watch.fetch, with the network and the clock replaced.

import email.message
import io
import os
import sys
import unittest
import urllib.error
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import deps_watch

class Response(io.BytesIO):
	def __enter__(self): return self
	def __exit__(self, *args): return False

def httpError(code, **headers):

	msg = email.message.Message()

	for k, v in headers.items():
		msg[k.replace("_", "-")] = v

	return urllib.error.HTTPError("https://api.github.com/x", code, "err", msg, io.BytesIO(b"{}"))

def run(outcomes):

	calls = []
	sleeps = []

	def urlopen(request, timeout = None):

		calls.append(request)
		outcome = outcomes[len(calls) - 1]

		if isinstance(outcome, Exception):
			raise outcome

		return Response(outcome)

	with mock.patch("urllib.request.urlopen", urlopen), mock.patch("time.sleep", sleeps.append):
		try:
			result = deps_watch.fetch("https://api.github.com/x")

		except urllib.error.HTTPError as e:
			result = e.code

	return result, len(calls), sleeps

class FetchRetries(unittest.TestCase):

	def test_server_error_waits_between_attempts(self):
		result, calls, sleeps = run([httpError(502), httpError(502), b'{"ok": 1}'])
		self.assertEqual((result, calls), ({ "ok": 1 }, 3))
		self.assertEqual(len(sleeps), 2)
		self.assertTrue(all(s > 0 for s in sleeps))

	def test_retry_after_is_honored_and_clamped(self):
		_, _, sleeps = run([httpError(429, Retry_After = "7"), httpError(429, Retry_After = "9999"), b"{}"])
		self.assertEqual(sleeps, [7, deps_watch.MAX_WAIT])

	def test_exhausted_rate_limit_403_retries(self):
		result, calls, sleeps = run([httpError(403, X_RateLimit_Remaining = "0"), b'{"ok": 1}'])
		self.assertEqual((result, calls, len(sleeps)), ({ "ok": 1 }, 2, 1))

	def test_plain_403_is_an_answer(self):
		result, calls, sleeps = run([httpError(403)])
		self.assertEqual((result, calls, sleeps), (403, 1, []))

	def test_404_is_an_answer(self):
		result, calls, sleeps = run([httpError(404)])
		self.assertEqual((result, calls, sleeps), (404, 1, []))

	def test_gives_up_after_three_attempts(self):
		result, calls, sleeps = run([httpError(503), httpError(503), httpError(503)])
		self.assertEqual((result, calls, len(sleeps)), (503, 3, 2))

if __name__ == "__main__":
	unittest.main()
