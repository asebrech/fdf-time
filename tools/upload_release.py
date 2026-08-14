"""Upload a release to the Pebble appstore with full control over the
screenshot set: replaceScreenshots=true wipes the previous set, and the
multipart field order per platform is: boot GIF (42 -> time), orbit GIF,
then a crisp static of the steady face."""
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
for p in PLATFORMS:
    field = "screenshots_" + p
    for name, mime in (
        (p + "_boot.gif", "image/gif"),
        (p + "_rollover.gif", "image/gif"),
        (p + "_orbit.gif", "image/gif"),
        (p + "_seconds.gif", "image/gif"),
        (p + "_steady.png", "image/png"),
    ):
        path = os.path.join(gifs_dir, name)
        assert os.path.exists(path), "missing " + path
        files_payload.append((field, (name, open(path, "rb"), mime)))

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
