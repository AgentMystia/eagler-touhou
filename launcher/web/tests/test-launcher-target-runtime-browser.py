"""Regression for the actual same-origin Runtime document after replacement.

Only in-memory HTML is served. This exercises observation, not a game Runtime.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
from threading import Thread

from playwright.sync_api import Error, sync_playwright

from support.launcher_target import runtime_frame_url


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = (
            b'<!doctype html><iframe id="runtime" src="/initial.html"></iframe>'
            if self.path == "/"
            else b"<!doctype html><title>Runtime observation fixture</title>"
        )
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


def main():
    project = Path(__file__).resolve().parents[1]
    executable = subprocess.run(
        [
            "node", "--input-type=module", "--eval",
            "import {findChromiumExecutable} from './lib/chromium-executable.mjs';"
            "process.stdout.write(await findChromiumExecutable());",
        ],
        cwd=project, check=True, capture_output=True, text=True, encoding="utf-8",
    ).stdout
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    origin = f"http://127.0.0.1:{server.server_port}"
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(executable_path=executable, headless=True)
            try:
                page = browser.new_page()
                page.goto(origin + "/", wait_until="load")
                frame = page.locator("#runtime")
                assert runtime_frame_url(frame) == origin + "/initial.html"
                destination = origin + "/runtime/th06/generation/th06.html?runtimeEpoch=42"
                frame.evaluate("(frame, url) => frame.contentWindow.location.replace(url)", destination)
                page.wait_for_function(
                    "url => document.querySelector('#runtime').contentDocument?.URL === url",
                    arg=destination,
                )
                assert frame.get_attribute("src") == "/initial.html"
                assert runtime_frame_url(frame) == destination
                assert runtime_frame_url(frame) == destination
                frame.evaluate("frame => frame.contentWindow.location.replace('about:blank')")
                page.wait_for_function(
                    "() => document.querySelector('#runtime').contentDocument?.URL === 'about:blank'"
                )
                assert runtime_frame_url(frame) == "about:blank"
                foreign = destination.replace("127.0.0.1", "localhost")
                with page.expect_event("framenavigated", predicate=lambda child: child.url == foreign):
                    frame.evaluate("(frame, url) => frame.contentWindow.location.replace(url)", foreign)
                try:
                    runtime_frame_url(frame)
                except Error as error:
                    assert "inaccessible" in str(error)
                else:
                    raise AssertionError("Cross-origin Runtime document must be rejected")
                handle = frame.element_handle()
                frame.evaluate("frame => frame.remove()")
                try:
                    runtime_frame_url(handle)
                except Error as error:
                    assert "missing or detached" in str(error)
                else:
                    raise AssertionError("Detached Runtime iframe must be rejected")
                print("PASS Runtime document observation: replacement, stale src, repeated read, blank, cross-origin, detached")
            finally:
                browser.close()
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()
