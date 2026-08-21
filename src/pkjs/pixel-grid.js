/* Pixel-grid Clay component: a 22x25 cell editor for the user-drawn splash.
 * The grid IS the watch model's inner region, so editing is WYSIWYG — no
 * downsampling ever happens (downsampling fine art is what made every
 * pre-rendered logo attempt painful). A stamp field rasterizes any emoji or
 * short text the phone can render into the grid as a starting point; the
 * result is then hand-editable cell by cell.
 * Serialized value: 6 lowercase hex chars per row (bit 1<<col = filled,
 * col 0 leftmost — the same convention as the C splashes), rows
 * concatenated, 150 chars.
 *
 * CRITICAL CONSTRAINT: Clay embeds this object into the config page with
 * toSource() — each function is serialized WITHOUT the module closure, so
 * nothing here may reference module-level helpers or constants (that
 * shipped once and died with "emptyGrid is not defined" on the page).
 * Everything lives inside initialize(); get/set called before initialize
 * only stash/return the raw serialized string. */

'use strict';

module.exports = {
  name: 'pixelGrid',
  template:
    '<div class="component component-pixel-grid">' +
      '<input data-manipulator-target type="hidden">' +
      '<button type="button" class="pg-head">' +
        '<span class="pg-head-label">{{{label}}}</span>' +
        '<canvas class="pg-mini" width="22" height="25"></canvas>' +
        '<span class="pg-chev">&#9656;</span>' +
      '</button>' +
      '<div class="pg-body">' +
        '<canvas class="pg-canvas" width="352" height="400"></canvas>' +
        '<div class="pg-hint">Tap or drag on the grid to sculpt cells.' +
        '</div>' +
        '<div class="pg-histrow">' +
          '<button type="button" class="pg-undo">&#8630; Undo</button>' +
          '<button type="button" class="pg-redo">&#8631; Redo</button>' +
        '</div>' +
        '<input class="pg-glyph" type="text"' +
          ' placeholder="👽 Or type an emoji / short text…">' +
        '<button type="button" class="pg-stamp">Stamp it</button>' +
        '<button type="button" class="pg-clear">Clear</button>' +
      '</div>' +
    '</div>',
  style:
    '.component-pixel-grid { padding: 0 0.375rem; }' +
    '.component-pixel-grid .pg-head { display: flex; width: 100%;' +
      ' align-items: center; background: none; border: none;' +
      ' color: inherit; font-size: inherit; text-align: left;' +
      ' padding: 0.35rem 0; min-height: 2.1rem; }' +
    '.component-pixel-grid .pg-head-label { flex: 1; }' +
    '.component-pixel-grid .pg-mini { width: 1.8rem; height: 2.05rem;' +
      ' image-rendering: pixelated; image-rendering: crisp-edges;' +
      ' border: 1px solid #555; border-radius: 2px;' +
      ' margin-right: 0.5rem; }' +
    '.component-pixel-grid .pg-chev { color: #ff4700; }' +
    '.component-pixel-grid .pg-body { display: none;' +
      ' padding-bottom: 0.35rem; }' +
    '.component-pixel-grid.pg-open .pg-body { display: block; }' +
    '.component-pixel-grid .pg-canvas { display: block; width: 100%;' +
      ' max-width: 352px; margin: 0 auto; touch-action: none;' +
      ' border: 1px solid #555; border-radius: 0.25rem; }' +
    '.component-pixel-grid .pg-hint { color: #858585; font-size: 0.85em;' +
      ' padding: 0.35rem 0 0.7rem; text-align: center; }' +
    '.component-pixel-grid .pg-histrow { display: flex;' +
      ' margin-bottom: 0.5rem; }' +
    '.component-pixel-grid .pg-undo, .component-pixel-grid .pg-redo {' +
      ' flex: 1; background: #484848; color: #ffffff; border: none;' +
      ' border-radius: 0.25rem; padding: 0.5rem 0; font-size: inherit;' +
      ' min-height: 2.1rem; }' +
    '.component-pixel-grid .pg-redo { margin-left: 0.5rem; }' +
    '.component-pixel-grid .pg-undo:disabled,' +
    ' .component-pixel-grid .pg-redo:disabled { opacity: 0.35; }' +
    '.component-pixel-grid .pg-glyph { display: block; width: 100%;' +
      ' box-sizing: border-box; background: #333333; color: #ffffff;' +
      ' border: none; border-radius: 0.25rem; padding: 0.5rem 0.375rem;' +
      ' font-size: inherit; min-height: 2.1rem;' +
      ' margin-bottom: 0.5rem; -webkit-appearance: none;' +
      ' appearance: none; }' +
    '.component-pixel-grid .pg-glyph::-webkit-input-placeholder' +
      ' { color: #858585; }' +
    '.component-pixel-grid .pg-stamp { display: block; width: 100%;' +
      ' box-sizing: border-box; background: #ff4700; color: #ffffff;' +
      ' border: none; border-radius: 0.25rem; padding: 0.5rem 0;' +
      ' font-size: inherit; min-height: 2.1rem;' +
      ' margin-bottom: 0.5rem; }' +
    '.component-pixel-grid .pg-clear { display: block; width: 100%;' +
      ' box-sizing: border-box; background: #666666; color: #ffffff;' +
      ' border: none; border-radius: 0.25rem; padding: 0.5rem 0;' +
      ' font-size: inherit; min-height: 2.1rem; }',
  defaults: {
    label: 'Your drawing'
  },
  manipulator: {
    get: function () {
      if (this._serializeNow) {
        return this._serializeNow();
      }
      return typeof this._value === 'string' ? this._value : '';
    },
    set: function (value) {
      this._value = typeof value === 'string' ? value : '';
      if (this._applyValue) {
        this._applyValue(this._value);
      }
      return this;
    },
    hide: function () {
      this.$element[0].style.display = 'none';
      return this;
    },
    show: function () {
      this.$element[0].style.display = '';
      return this;
    }
  },
  initialize: function () {
    var self = this;
    var COLS = 22, ROWS = 25;
    var root = self.$element[0];
    var canvas = root.querySelector('.pg-canvas');
    var ctx = canvas.getContext('2d');
    var mini = root.querySelector('.pg-mini');
    var mctx = mini.getContext('2d');
    var head = root.querySelector('.pg-head');
    var chev = root.querySelector('.pg-chev');

    function emptyGrid() {
      var g = [];
      for (var r = 0; r < ROWS; r++) {
        var row = [];
        for (var c = 0; c < COLS; c++) { row.push(0); }
        g.push(row);
      }
      return g;
    }

    function serialize(grid) {
      var out = '';
      for (var r = 0; r < ROWS; r++) {
        var v = 0;
        for (var c = 0; c < COLS; c++) {
          if (grid[r][c]) { v |= (1 << c); }
        }
        out += ('00000' + v.toString(16)).slice(-6);
      }
      return out;
    }

    function parse(str) {
      var grid = emptyGrid();
      if (typeof str !== 'string' || str.length !== ROWS * 6) {
        return grid;
      }
      for (var r = 0; r < ROWS; r++) {
        var v = parseInt(str.substr(r * 6, 6), 16);
        if (isNaN(v)) { return emptyGrid(); }
        for (var c = 0; c < COLS; c++) {
          grid[r][c] = (v >> c) & 1;
        }
      }
      return grid;
    }

    function redraw() {
      var cell = canvas.width / COLS;
      ctx.fillStyle = '#1a1a1a';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.fillStyle = '#ff4700';
      for (var r = 0; r < ROWS; r++) {
        for (var c = 0; c < COLS; c++) {
          if (grid[r][c]) {
            ctx.fillRect(c * cell + 1, r * cell + 1, cell - 2, cell - 2);
          }
        }
      }
      ctx.strokeStyle = 'rgba(255,255,255,0.08)';
      ctx.lineWidth = 1;
      for (var i = 0; i <= COLS; i++) {
        ctx.beginPath();
        ctx.moveTo(i * cell, 0);
        ctx.lineTo(i * cell, canvas.height);
        ctx.stroke();
      }
      for (var j = 0; j <= ROWS; j++) {
        ctx.beginPath();
        ctx.moveTo(0, j * cell);
        ctx.lineTo(canvas.width, j * cell);
        ctx.stroke();
      }
      // Collapsed-row thumbnail: 1 px per cell, scaled up by CSS.
      mctx.fillStyle = '#1a1a1a';
      mctx.fillRect(0, 0, COLS, ROWS);
      mctx.fillStyle = '#ff4700';
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          if (grid[r][c]) { mctx.fillRect(c, r, 1, 1); }
        }
      }
    }

    // The editor lives folded behind a normal-looking settings row (label +
    // live thumbnail + chevron) so the drawing doesn't dominate the page.
    head.addEventListener('click', function () {
      var open = root.className.indexOf('pg-open') === -1;
      root.className = open
        ? root.className + ' pg-open'
        : root.className.replace(/ ?pg-open/, '');
      chev.innerHTML = open ? '&#9662;' : '&#9656;';
    });

    /* ---- binary-shape helpers, shared by the rasterizer ----
     * All of these must stay INSIDE initialize(): Clay serializes the
     * component with toSource() and a module-level helper would compile
     * fine here and die on the config page with a ReferenceError. */

    /* 4-connected components of cells equal to `val`. 4-connected, not 8:
     * the watch draws an edge only between ADJACENT vertices, so corner
     * contact is not connection there either. */
    function componentsOf(g, val) {
      var seen = [], r, c;
      for (r = 0; r < ROWS; r++) {
        seen.push([]);
        for (c = 0; c < COLS; c++) { seen[r].push(false); }
      }
      var out = [];
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          if (g[r][c] !== val || seen[r][c]) { continue; }
          var stack = [[r, c]], comp = [];
          seen[r][c] = true;
          while (stack.length) {
            var p = stack.pop(), y = p[0], x = p[1];
            comp.push(p);
            var nb = [[y - 1, x], [y + 1, x], [y, x - 1], [y, x + 1]];
            for (var k = 0; k < 4; k++) {
              var ny = nb[k][0], nx = nb[k][1];
              if (ny < 0 || nx < 0 || ny >= ROWS || nx >= COLS) { continue; }
              if (!seen[ny][nx] && g[ny][nx] === val) {
                seen[ny][nx] = true;
                stack.push([ny, nx]);
              }
            }
          }
          out.push(comp);
        }
      }
      return out;
    }

    /* Weld corner-only links. On the watch two cells touching at a corner
     * share no edge and render as disconnected floating blocks — the same
     * rule that gives digits.h its straight-legged 7. Fill whichever EMPTY
     * cell of the opposite diagonal carries more ink; note the two
     * orientations have different gap cells. */
    function weldDiagonals(g, cov) {
      var changed = 0;
      for (var r = 0; r < ROWS - 1; r++) {
        for (var c = 0; c < COLS - 1; c++) {
          var a = g[r][c], b = g[r + 1][c + 1];
          var d = g[r][c + 1], e = g[r + 1][c];
          if (a && b && !d && !e) {
            if (cov[r][c + 1] >= cov[r + 1][c]) { g[r][c + 1] = 1; }
            else { g[r + 1][c] = 1; }
            changed++;
          } else if (d && e && !a && !b) {
            if (cov[r][c] >= cov[r + 1][c + 1]) { g[r][c] = 1; }
            else { g[r + 1][c + 1] = 1; }
            changed++;
          }
        }
      }
      return changed;
    }

    /* Drop specks, always keeping the largest component. */
    function pruneComponents(g, minSize) {
      var comps = componentsOf(g, 1);
      if (!comps.length) { return; }
      comps.sort(function (x, y) { return y.length - x.length; });
      for (var i = 1; i < comps.length; i++) {
        if (comps[i].length >= minSize) { continue; }
        for (var j = 0; j < comps[i].length; j++) {
          g[comps[i][j][0]][comps[i][j][1]] = 0;
        }
      }
    }

    /* Fill enclosed background pockets under maxSize: at this scale they
     * read as noise, not as counters. */
    function fillSmallHoles(g, maxSize) {
      var holes = componentsOf(g, 0);
      for (var i = 0; i < holes.length; i++) {
        var comp = holes[i], edge = false, j;
        for (j = 0; j < comp.length; j++) {
          var r = comp[j][0], c = comp[j][1];
          if (r === 0 || c === 0 || r === ROWS - 1 || c === COLS - 1) {
            edge = true;
            break;
          }
        }
        if (edge || comp.length > maxSize) { continue; }
        for (j = 0; j < comp.length; j++) { g[comp[j][0]][comp[j][1]] = 1; }
      }
    }

    /* A cell is "thin" when no 2x2 block of ink contains it. Below two
     * cells a stroke is not legible as a plateau — the project's own digit
     * font is 2-cell for exactly this reason. Two outcomes: a thin cell
     * with >=2 filled neighbours is part of a STROKE and gets thickened
     * (this is what saves text stamps); one with <=1 is a STUB left by a
     * detail carve and gets dropped, because growing stubs grows noise. */
    function repairThin(g, cov) {
      for (var pass = 0; pass < 2; pass++) {
        var grow = [], drop = [], r, c;
        for (r = 0; r < ROWS; r++) {
          for (c = 0; c < COLS; c++) {
            if (!g[r][c]) { continue; }
            var inBlock = false, dr, dc;
            for (dr = -1; dr <= 0; dr++) {
              for (dc = -1; dc <= 0; dc++) {
                var rr = r + dr, cc = c + dc;
                if (rr < 0 || cc < 0 || rr + 1 >= ROWS || cc + 1 >= COLS) {
                  continue;
                }
                if (g[rr][cc] && g[rr + 1][cc] && g[rr][cc + 1] &&
                    g[rr + 1][cc + 1]) {
                  inBlock = true;
                }
              }
            }
            if (inBlock) { continue; }
            var nb = [[r - 1, c], [r + 1, c], [r, c - 1], [r, c + 1]];
            var deg = 0, best = null, bv = -1, k;
            for (k = 0; k < 4; k++) {
              var ny = nb[k][0], nx = nb[k][1];
              if (ny < 0 || nx < 0 || ny >= ROWS || nx >= COLS) { continue; }
              if (g[ny][nx]) { deg++; }
              else if (cov[ny][nx] > bv) { bv = cov[ny][nx]; best = [ny, nx]; }
            }
            if (deg <= 1) { drop.push([r, c]); }
            else if (best) { grow.push(best); }
          }
        }
        if (!grow.length && !drop.length) { break; }
        var i;
        for (i = 0; i < drop.length; i++) { g[drop[i][0]][drop[i][1]] = 0; }
        for (i = 0; i < grow.length; i++) { g[grow[i][0]][grow[i][1]] = 1; }
      }
    }

    /* Rasterize a glyph (emoji or short text) into the grid.
     *
     * Pipeline, in order, each step earning its place on a 16-glyph bench
     * (🙂❤️⭐🌙👍😎🎱🆘🌍👌😉🔥☕🐱, "A", "42") measured offline against the
     * watch's two hard rules — no corner-only links, no sub-2-cell features:
     *   1. supersample the glyph 8x and reduce to per-cell coverage+luminance
     *   2. Otsu threshold (clamped) instead of a fixed coverage cut
     *   3. left-right symmetry snap on near-symmetric silhouettes
     *   4. detail carve by REGION, never per cell
     *   5. fill pinhole gaps, prune specks
     *   6. thicken thin strokes / drop stubs
     *   7. weld corner-only links, iterating with prune to a fixed point
     * Before this, six of the sixteen produced floating blocks and "42"
     * came out as 26 illegible 1-cell stroke cells; after, both counts are
     * zero across the whole battery. */
    function rasterizeGlyph(ch) {
      var S = 8;
      var W = COLS * S, H = ROWS * S;
      var cv = document.createElement('canvas');
      cv.width = W; cv.height = H;
      var gctx = cv.getContext('2d', { willReadFrequently: true });
      if (!gctx) { return null; }
      gctx.clearRect(0, 0, W, H);
      gctx.textAlign = 'center';
      gctx.textBaseline = 'alphabetic';
      var ref = 100;
      // BOLD matters for text stamps and is free for emoji (colour bitmap
      // glyphs ignore weight): at 22 cells a regular weight lands 1-cell
      // stems, which the watch cannot render as plateaus at all — "42" came
      // out as 26 illegible thin cells before this.
      gctx.font = 'bold ' + ref + 'px sans-serif';
      var m = gctx.measureText(ch);
      var asc = m.actualBoundingBoxAscent || ref * 0.8;
      var desc = m.actualBoundingBoxDescent || ref * 0.2;
      var gw = (m.actualBoundingBoxLeft || 0) +
               (m.actualBoundingBoxRight || m.width);
      var gh = asc + desc;
      if (!gw || !gh) { return null; }
      var size = ref * Math.min(W / gw, H / gh) * 0.98;
      gctx.font = 'bold ' + size + 'px sans-serif';
      m = gctx.measureText(ch);
      asc = m.actualBoundingBoxAscent || size * 0.8;
      desc = m.actualBoundingBoxDescent || size * 0.2;
      // Center the INK box, not the advance width: many emoji carry their
      // ink off-center in the advance, and a sub-cell offset is what makes
      // a symmetric face come out lopsided after thresholding.
      var dx = W / 2;
      if (m.actualBoundingBoxLeft !== undefined) {
        dx = W / 2 - ((m.actualBoundingBoxRight || 0) -
                      (m.actualBoundingBoxLeft || 0)) / 2;
      }
      gctx.fillStyle = '#000';
      gctx.fillText(ch, dx, H / 2 + (asc - desc) / 2);
      var img;
      try {
        img = gctx.getImageData(0, 0, W, H).data;
      } catch (e) {
        return null;
      }
      var cov = [], lum = [], r, c, x, y, i;
      for (r = 0; r < ROWS; r++) {
        cov[r] = []; lum[r] = [];
        for (c = 0; c < COLS; c++) {
          var aSum = 0, lSum = 0, lN = 0;
          for (y = r * S; y < (r + 1) * S; y++) {
            for (x = c * S; x < (c + 1) * S; x++) {
              i = (y * W + x) * 4;
              var a = img[i + 3];
              aSum += a;
              if (a > 64) {
                lSum += 0.299 * img[i] + 0.587 * img[i + 1] +
                        0.114 * img[i + 2];
                lN++;
              }
            }
          }
          cov[r][c] = aSum / (S * S * 255);
          lum[r][c] = lN ? lSum / lN : 255;
        }
      }
      // Fixed coverage cut, tuned on a glyph battery. AUTOMATIC THRESHOLDS
      // WERE TRIED AND REVERTED (2026-08-21, user hit it on 👾): Otsu picks
      // 0.62-0.68 on this kind of coverage map — always above 0.45 — because
      // its class-imbalance drift thins a sparse glyph further. That erodes
      // thin limbs until they DETACH: the space invader lost its antennae
      // and came apart into seven pieces. In this renderer under-filling is
      // fatal (features vanish, shapes disconnect) while over-filling is
      // merely a little heavy, so the threshold must never rise above the
      // tuned value. Ink-conservation and hysteresis variants were measured
      // too and neither beat this constant.
      var thr = 0.45;
      var out = emptyGrid();
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          out[r][c] = cov[r][c] >= thr ? 1 : 0;
        }
      }
      // Symmetry snap (the NixOS C2 lesson, applied left-right): on a
      // near-symmetric silhouette (faces, hearts), edge columns hover
      // around the fill threshold and antialiasing flips one side only.
      // Averaging mirrored coverages makes the outline exactly symmetric.
      // Clearly asymmetric glyphs (letters, moons, hands) are left alone.
      // The carve gets the SAME treatment further down, behind its own
      // second test — see the carve-symmetry block.
      var pairs = 0, mism = 0;
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < (COLS >> 1); c++) {
          if (out[r][c] || out[r][COLS - 1 - c]) {
            pairs++;
            if (out[r][c] !== out[r][COLS - 1 - c]) { mism++; }
          }
        }
      }
      var snapped = pairs && mism / pairs <= 0.15;
      if (snapped) {
        for (r = 0; r < ROWS; r++) {
          for (c = 0; c < (COLS >> 1); c++) {
            var v2 = (cov[r][c] + cov[r][COLS - 1 - c]) / 2 >= thr ? 1 : 0;
            out[r][c] = v2;
            out[r][COLS - 1 - c] = v2;
          }
        }
      }
      var bodyLs = [];
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          if (out[r][c]) { bodyLs.push(lum[r][c]); }
        }
      }
      if (!bodyLs.length) { return null; }
      bodyLs.sort(function (a, b) { return a - b; });
      var bodyL = bodyLs[bodyLs.length >> 1];
      // Detail carve, as REGIONS rather than per cell. Dark details (eyes,
      // mouths) on a bright body, and the mirror case of BRIGHT details on
      // a darker body (🆘, boxed arrows, 🎱 — self-guarding, since bodyL+80
      // is unreachable on a bright body and gloss stays under the margin).
      // A carve only survives if it is a connected blob of >=3 cells: that
      // is what an eye or a mouth looks like here, and carving cell by cell
      // shredded glyphs into pepper noise (the coffee cup lost 5 cells to
      // floating blocks that way, and the smiley's mouth came out broken).
      var carve = emptyGrid();
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          if (!out[r][c]) { continue; }
          if (bodyL > 90 && lum[r][c] < bodyL * 0.55) { carve[r][c] = 1; }
          else if (lum[r][c] > bodyL + 80) { carve[r][c] = 1; }
        }
      }
      // Mirror the CARVE too, but only when the silhouette itself snapped
      // AND the carve is already mostly symmetric (<=30% of mirrored pairs
      // disagree). A symmetric face whose eyes come out lopsided is the
      // most visible failure left — 👽 had 7 asymmetric cells, 😎 six — and
      // this drives them to zero. The double guard is what protects a wink:
      // 😉's carve disagrees on 40% of its pairs, well clear of the bound,
      // so its one closed eye survives untouched. Union, not average: an
      // eye found on one side only should be carved on BOTH.
      var cpairs = 0, cmism = 0;
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < (COLS >> 1); c++) {
          if (carve[r][c] || carve[r][COLS - 1 - c]) {
            cpairs++;
            if (carve[r][c] !== carve[r][COLS - 1 - c]) { cmism++; }
          }
        }
      }
      if (snapped && cpairs && cmism / cpairs <= 0.30) {
        for (r = 0; r < ROWS; r++) {
          for (c = 0; c < (COLS >> 1); c++) {
            var cv = (carve[r][c] || carve[r][COLS - 1 - c]) ? 1 : 0;
            carve[r][c] = cv;
            carve[r][COLS - 1 - c] = cv;
          }
        }
      }
      var blobs = componentsOf(carve, 1);
      for (var bi = 0; bi < blobs.length; bi++) {
        if (blobs[bi].length < 3) { continue; }
        for (var bj = 0; bj < blobs[bi].length; bj++) {
          out[blobs[bi][bj][0]][blobs[bi][bj][1]] = 0;
        }
      }

      fillSmallHoles(out, 2);
      pruneComponents(out, 4);
      repairThin(out, cov);
      // Weld and prune interact — pruning a speck can expose a new
      // corner-only link and welding can re-attach one — so iterate to a
      // fixed point instead of running each once.
      for (var it = 0; it < 8; it++) {
        pruneComponents(out, 4);
        if (!weldDiagonals(out, cov)) { break; }
      }
      return out;
    }

    var saved = typeof self._value === 'string' ? self._value : '';
    var grid = parse(saved);
    if (saved.length !== ROWS * 6) {
      // Nothing ever saved: greet the user with a stamped 👽 instead of a
      // blank grid (it only reaches the watch if they hit Save).
      var starter = rasterizeGlyph('👽');
      if (starter) { grid = starter; }
    }

    // Undo/redo: one entry per finished gesture (a full drag stroke, a
    // stamp, a clear) — not per cell.
    var undoBtn = root.querySelector('.pg-undo');
    var redoBtn = root.querySelector('.pg-redo');
    var hist = [serialize(grid)];
    var hi = 0;

    function updateHistBtns() {
      undoBtn.disabled = hi <= 0;
      redoBtn.disabled = hi >= hist.length - 1;
    }

    function pushHist() {
      var s = serialize(grid);
      if (s === hist[hi]) { return; }
      hist = hist.slice(0, hi + 1);
      hist.push(s);
      if (hist.length > 60) { hist.shift(); }
      hi = hist.length - 1;
      updateHistBtns();
    }

    undoBtn.addEventListener('click', function () {
      if (hi > 0) {
        hi--;
        grid = parse(hist[hi]);
        redraw();
        updateHistBtns();
      }
    });

    redoBtn.addEventListener('click', function () {
      if (hi < hist.length - 1) {
        hi++;
        grid = parse(hist[hi]);
        redraw();
        updateHistBtns();
      }
    });

    // Post-init entry points for the manipulator (which cannot share this
    // closure — see the header comment).
    self._serializeNow = function () { return serialize(grid); };
    self._applyValue = function (value) {
      grid = parse(value);
      redraw();
      pushHist();
    };

    var paintValue = 1;
    var painting = false;

    function cellAt(clientX, clientY) {
      var rect = canvas.getBoundingClientRect();
      var c = Math.floor((clientX - rect.left) / rect.width * COLS);
      var r = Math.floor((clientY - rect.top) / rect.height * ROWS);
      if (r < 0 || r >= ROWS || c < 0 || c >= COLS) { return null; }
      return { r: r, c: c };
    }

    function paintStart(clientX, clientY) {
      var cc = cellAt(clientX, clientY);
      if (!cc) { return; }
      painting = true;
      paintValue = grid[cc.r][cc.c] ? 0 : 1;
      grid[cc.r][cc.c] = paintValue;
      redraw();
    }

    function paintMove(clientX, clientY) {
      if (!painting) { return; }
      var cc = cellAt(clientX, clientY);
      if (!cc || grid[cc.r][cc.c] === paintValue) { return; }
      grid[cc.r][cc.c] = paintValue;
      redraw();
    }

    if (window.PointerEvent) {
      canvas.addEventListener('pointerdown', function (e) {
        e.preventDefault();
        paintStart(e.clientX, e.clientY);
      });
      canvas.addEventListener('pointermove', function (e) {
        e.preventDefault();
        paintMove(e.clientX, e.clientY);
      });
      window.addEventListener('pointerup', function () {
        if (painting) { painting = false; pushHist(); }
      });
    } else {
      canvas.addEventListener('touchstart', function (e) {
        e.preventDefault();
        var t = e.changedTouches[0];
        paintStart(t.clientX, t.clientY);
      });
      canvas.addEventListener('touchmove', function (e) {
        e.preventDefault();
        var t = e.changedTouches[0];
        paintMove(t.clientX, t.clientY);
      });
      canvas.addEventListener('touchend', function () {
        if (painting) { painting = false; pushHist(); }
      });
      canvas.addEventListener('mousedown', function (e) {
        e.preventDefault();
        paintStart(e.clientX, e.clientY);
      });
      canvas.addEventListener('mousemove', function (e) {
        paintMove(e.clientX, e.clientY);
      });
      window.addEventListener('mouseup', function () {
        if (painting) { painting = false; pushHist(); }
      });
    }

    root.querySelector('.pg-stamp').addEventListener('click', function () {
      var text = root.querySelector('.pg-glyph').value;
      text = text.replace(/^\s+|\s+$/g, '').slice(0, 8);
      if (!text) { return; }
      var stamped = rasterizeGlyph(text);
      if (stamped) {
        grid = stamped;
        redraw();
        pushHist();
      }
    });

    root.querySelector('.pg-clear').addEventListener('click', function () {
      grid = emptyGrid();
      redraw();
      pushHist();
    });

    redraw();
    updateHistBtns();
  }
};
