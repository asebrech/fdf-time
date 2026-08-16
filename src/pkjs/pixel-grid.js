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
          ' placeholder="😎 Or type an emoji / short text…">' +
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

    /* Rasterize a glyph (emoji or short text) into the grid. Thresholds
     * were tuned on a glyph battery: alpha coverage >= 0.45 builds the
     * silhouette; on bright-bodied glyphs (median L > 90) cells darker
     * than 0.55x the body's median luminance are carved out (eyes,
     * mouths); 1-cell orphans are dropped (they can't render as
     * plateaus on the watch). */
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
      gctx.font = ref + 'px sans-serif';
      var m = gctx.measureText(ch);
      var asc = m.actualBoundingBoxAscent || ref * 0.8;
      var desc = m.actualBoundingBoxDescent || ref * 0.2;
      var gw = (m.actualBoundingBoxLeft || 0) +
               (m.actualBoundingBoxRight || m.width);
      var gh = asc + desc;
      if (!gw || !gh) { return null; }
      var size = ref * Math.min(W / gw, H / gh) * 0.98;
      gctx.font = size + 'px sans-serif';
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
      var out = emptyGrid();
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          out[r][c] = cov[r][c] >= 0.45 ? 1 : 0;
        }
      }
      // Symmetry snap (the NixOS C2 lesson, applied left-right): on a
      // near-symmetric silhouette (faces, hearts), edge columns hover
      // around the fill threshold and antialiasing flips one side only.
      // Averaging mirrored coverages makes the outline exactly symmetric.
      // Clearly asymmetric glyphs (letters, moons, hands) are left alone —
      // and only the SILHOUETTE is snapped, never the dark-detail carve,
      // so a wink 😉 keeps its one closed eye.
      var pairs = 0, mism = 0;
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < (COLS >> 1); c++) {
          if (out[r][c] || out[r][COLS - 1 - c]) {
            pairs++;
            if (out[r][c] !== out[r][COLS - 1 - c]) { mism++; }
          }
        }
      }
      if (pairs && mism / pairs <= 0.15) {
        for (r = 0; r < ROWS; r++) {
          for (c = 0; c < (COLS >> 1); c++) {
            var v2 = (cov[r][c] + cov[r][COLS - 1 - c]) / 2 >= 0.45 ? 1 : 0;
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
      if (bodyL > 90) {
        for (r = 0; r < ROWS; r++) {
          for (c = 0; c < COLS; c++) {
            if (out[r][c] && lum[r][c] < bodyL * 0.55) { out[r][c] = 0; }
          }
        }
      }
      // The mirror case: BRIGHT details on a darker body (🆘, boxed
      // arrows, 🎱) — engrave cells clearly brighter than the body's
      // median. Self-guarding: on bright bodies (🙂, 👻) bodyL + 80 is
      // unreachable, and mild gloss/shading stays well under the margin.
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          if (out[r][c] && lum[r][c] > bodyL + 80) { out[r][c] = 0; }
        }
      }
      for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
          if (!out[r][c]) { continue; }
          var n = 0;
          if (r > 0 && out[r - 1][c]) { n++; }
          if (r < ROWS - 1 && out[r + 1][c]) { n++; }
          if (c > 0 && out[r][c - 1]) { n++; }
          if (c < COLS - 1 && out[r][c + 1]) { n++; }
          if (!n) { out[r][c] = 0; }
        }
      }
      return out;
    }

    var saved = typeof self._value === 'string' ? self._value : '';
    var grid = parse(saved);
    if (saved.length !== ROWS * 6) {
      // Nothing ever saved: greet the user with a stamped 😎 instead of a
      // blank grid (it only reaches the watch if they hit Save).
      var starter = rasterizeGlyph('😎');
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
