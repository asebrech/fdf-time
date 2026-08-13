"""Minimal Chrome DevTools Protocol driver for the Brave session on :9222.

Usage:
  cdp.py tabs
  cdp.py screenshot <out.png>
  cdp.py navigate <url>
  cdp.py eval <js>                 (returns JSON of the result value)
  cdp.py upload <css_selector> <file> [file...]
"""
import base64
import json
import sys
import urllib.request

from websocket import create_connection


def http(path):
    with urllib.request.urlopen("http://localhost:9222" + path) as r:
        return json.loads(r.read())


def page_ws():
    pages = [t for t in http("/json") if t.get("type") == "page"]
    if not pages:
        raise SystemExit("no page target")
    return create_connection(pages[0]["webSocketDebuggerUrl"], timeout=30), pages[0]


class Tab:
    def __init__(self):
        self.ws, self.info = page_ws()
        self._id = 0

    def send(self, method, **params):
        self._id += 1
        self.ws.send(json.dumps({"id": self._id, "method": method, "params": params}))
        while True:
            msg = json.loads(self.ws.recv())
            if msg.get("id") == self._id:
                if "error" in msg:
                    raise RuntimeError(json.dumps(msg["error"]))
                return msg.get("result", {})

    def evaluate(self, js):
        r = self.send("Runtime.evaluate", expression=js, returnByValue=True,
                      awaitPromise=True)
        return r.get("result", {}).get("value")


cmd = sys.argv[1]
tab = Tab()

if cmd == "tabs":
    print(json.dumps([{"url": t["url"], "title": t["title"]}
                      for t in http("/json") if t.get("type") == "page"], indent=1))
elif cmd == "screenshot":
    r = tab.send("Page.captureScreenshot", format="png")
    with open(sys.argv[2], "wb") as f:
        f.write(base64.b64decode(r["data"]))
    print("saved", sys.argv[2])
elif cmd == "navigate":
    tab.send("Page.navigate", url=sys.argv[2])
    print("ok")
elif cmd == "eval":
    print(json.dumps(tab.evaluate(sys.argv[2])))
elif cmd == "eval-file":
    with open(sys.argv[2]) as f:
        print(json.dumps(tab.evaluate(f.read())))
elif cmd == "click":
    # Coordinates are given in SCREENSHOT pixels; convert to CSS pixels.
    ix, iy = float(sys.argv[2]), float(sys.argv[3])
    metrics = tab.send("Page.getLayoutMetrics")
    css_w = metrics["cssLayoutViewport"]["clientWidth"]
    shot_w = float(sys.argv[4]) if len(sys.argv) > 4 else css_w
    scale = css_w / shot_w
    x, y = ix * scale, iy * scale
    for t, extra in (("mousePressed", {"clickCount": 1}),
                     ("mouseReleased", {"clickCount": 1})):
        tab.send("Input.dispatchMouseEvent", type=t, x=x, y=y,
                 button="left", **extra)
    print("clicked css", round(x), round(y))
elif cmd == "upload":
    selector, files = sys.argv[2], sys.argv[3:]
    doc = tab.send("DOM.getDocument")
    node = tab.send("DOM.querySelector", nodeId=doc["root"]["nodeId"],
                    selector=selector)
    if not node.get("nodeId"):
        raise SystemExit("selector not found: " + selector)
    tab.send("DOM.setFileInputFiles", files=files, nodeId=node["nodeId"])
    print("uploaded", len(files), "file(s) into", selector)
else:
    raise SystemExit("unknown command")
