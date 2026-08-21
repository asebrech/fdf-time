"""Upload a release to the Pebble appstore with full control over the
screenshot set: replaceScreenshots=true wipes the previous set, and the
multipart field order per platform IS the order the store shows them in.

Order (2026-08-21): the drawing rising into the time, the minute rollover,
the orbit, the heart-rate strip, then the theme cycle. Whatever is missing
for a platform is skipped rather than faked — 1-bit boards have no themes
and aplite has no health, so they ship a shorter set (aplite substitutes
the battery scene). The old seconds clip and the static shot were dropped:
in a list of still screenshots, every slot being animated is the point."""
import os
import sys
import requests
from pebble_tool.account import get_default_account

APP_ID = "0e2670c1adae469783030d49"
API = "https://appstore-api.repebble.com/api/dashboard/apps/{}/releases".format(APP_ID)
PLATFORMS = ["aplite", "basalt", "chalk", "diorite", "emery", "flint", "gabbro"]

project = sys.argv[1]
version = sys.argv[2]
notes = sys.argv[3]
gifs_dir = sys.argv[4]

pbw = os.path.join(project, "build", "fdf-time.pbw")

files_payload = [("pbwFile", ("fdf-time.pbw", open(pbw, "rb"), "application/octet-stream"))]
ORDER = ["boot", "rollover", "orbit", "heart", "themes", "battery"]
for p in PLATFORMS:
    field = "screenshots_" + p
    found = 0
    for kind in ORDER:
        name = "{}_{}.gif".format(p, kind)
        path = os.path.join(gifs_dir, name)
        if not os.path.exists(path):
            continue
        files_payload.append((field, (name, open(path, "rb"), "image/gif")))
        found += 1
    assert found, "no screenshots at all for " + p
    print("  {:8s} {} screenshots".format(p, found))

tok = get_default_account().get_access_token()
r = requests.post(
    API,
    headers={"Authorization": "Bearer " + tok},
    data={
        "version": version,
        "releaseNotes": notes,
        "isPublished": "true",
        "replaceScreenshots": "true",
    },
    files=files_payload,
    timeout=300,
)
print(r.status_code)
print(r.text[:400])
