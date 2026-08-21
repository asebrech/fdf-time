/* Replace the Rebble dev-portal screenshot set, from the browser console.
 *
 * WHY THIS EXISTS: driving the portal's UI to swap screenshots is a trap.
 * Every one of these bit us on 2026-08-21 before this script was written:
 *
 *  - "EDIT STORE LISTING" is a TOGGLE. Two clicks close it again, so a
 *    "click it to be safe" reflex silently leaves you with no form.
 *  - Platform panels are LAZY. #e-scr-<platform> has no content until its
 *    tab (#e-scr-<platform>-tab) has been clicked, so counting screenshots
 *    on a freshly opened form reports 0 for every platform but the active
 *    one.
 *  - The DOM does NOT refresh after a delete. button[data-uuid] elements
 *    stay behind, so a delete that WORKED looks like it failed and you
 *    "retry" against a uuid the server no longer has ("Screenshot not
 *    found"). Always re-read via getEditScreenshotsForPlatform() and count
 *    the <img> srcs, skipping the placeholder ones.
 *  - A platform must keep AT LEAST ONE screenshot and may hold at most 5.
 *    So a full swap is: delete down to 1, upload the new ones into the free
 *    slots, delete the last old one, then upload the final new one.
 *  - Locating the hidden file inputs by their accessibility labels gives
 *    refs that belong to ANOTHER platform. That is how a 180x180 chalk GIF
 *    ended up rejected by a 144x168 slot, with the portal reporting only a
 *    dimensions error. Call the page's own upload function instead, with an
 *    explicit slot id: newScreenshotForUpload('e-screenshot-<letter>-<n>-i',
 *    file, platform), letters a..g = aplite basalt chalk diorite emery
 *    flint gabbro.
 *
 * HOW TO USE
 *  1. Serve the GIF directory over HTTP from this machine. The server MUST
 *     be threaded (a single-threaded one deadlocks the moment the browser
 *     holds a connection open, which freezes the whole tab) and MUST send
 *     Access-Control-Allow-Origin plus Access-Control-Allow-Private-Network,
 *     or Chrome blocks the HTTPS page from reading localhost and the fetch
 *     hangs forever with no error at all. See the header block in
 *     tools/serve_gifs.py.
 *  2. Open https://dev-portal.rebble.io/, click EDIT STORE LISTING ONCE.
 *  3. Paste this file, then: await swapAll('http://127.0.0.1:8733')
 */

const LETTER = {aplite: 'a', basalt: 'b', chalk: 'c', diorite: 'd',
                emery: 'e', flint: 'f', gabbro: 'g'};

// Which clips each platform ships. 1-bit boards have no colour themes, and
// aplite has no health service so it shows the battery scene instead.
const SET = {
  aplite:  ['boot', 'rollover', 'orbit', 'battery'],
  basalt:  ['boot', 'rollover', 'orbit', 'heart', 'themes'],
  chalk:   ['boot', 'rollover', 'orbit', 'heart', 'themes'],
  diorite: ['boot', 'rollover', 'orbit', 'heart'],
  emery:   ['boot', 'rollover', 'orbit', 'heart', 'themes'],
  flint:   ['boot', 'rollover', 'orbit', 'heart'],
  gabbro:  ['boot', 'rollover', 'orbit', 'heart', 'themes'],
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/* The only trustworthy count: reload the panel from the server, then look at
 * the image sources and drop the placeholders. */
async function real(pl) {
  getEditScreenshotsForPlatform(pl);
  await sleep(1800);
  return [...document.getElementById('e-scr-' + pl).querySelectorAll('img')]
    .map((i) => i.src.split('/').pop())
    .filter((s) => !/screenshot|placeholder/i.test(s));
}

async function delOne(pl) {
  const b = document.getElementById('e-scr-' + pl)
    .querySelector('button[data-uuid]');
  if (!b) return false;
  b.click();
  await sleep(2000);
  return true;
}

async function put(pl, kind, slot, base) {
  const c = new AbortController();
  setTimeout(() => c.abort(), 8000);  // never let a hung fetch freeze the tab
  const blob = await (await fetch(base + '/' + pl + '_' + kind + '.gif',
                                  {cache: 'no-store', signal: c.signal})).blob();
  newScreenshotForUpload('e-screenshot-' + LETTER[pl] + '-' + slot + '-i',
    new File([blob], pl + '_' + kind + '.gif', {type: 'image/gif'}), pl);
  await sleep(2300);
}

async function swap(pl, base) {
  document.getElementById('e-scr-' + pl + '-tab').click();
  await sleep(1200);
  for (let i = 0; i < 6 && (await real(pl)).length > 1; i++) await delOne(pl);
  const kinds = SET[pl];
  // Slot 1 is squatted by the last old screenshot, which cannot be deleted
  // while it is the only one; fill 2..N first.
  for (let i = 0; i < kinds.length - 1; i++) await put(pl, kinds[i], i + 2, base);
  await delOne(pl);                                   // now it can go
  await put(pl, kinds[kinds.length - 1], kinds.length, base);
  const got = await real(pl);
  console.log(pl, got.length + '/' + kinds.length);
  return got.length === kinds.length;
}

async function swapAll(base) {
  const bad = [];
  for (const pl of Object.keys(SET)) if (!(await swap(pl, base))) bad.push(pl);
  console.log(bad.length ? 'INCOMPLETE: ' + bad : 'all platforms complete');
  return bad;
}
