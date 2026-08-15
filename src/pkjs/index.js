var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// The seconds peek/toggle gestures only exist in the classic display mode:
// if the display is set to Seconds, such a shake choice snaps back to
// Orbit, so the invalid combination can't be saved.
var customClay = function () {
  var clay = this;
  clay.on(clay.EVENTS.AFTER_BUILD, function () {
    var mode = clay.getItemByMessageKey('Mode');
    var shake = clay.getItemByMessageKey('ShakeOrbit');
    if (!mode || !shake) {
      return;
    }
    function sync() {
      var s = shake.get();
      if (mode.get() === '1' && (s === '2' || s === '3')) {
        shake.set('1');
      }
    }
    mode.on('change', sync);
    shake.on('change', sync);
    sync();
  });
};

var clay = new Clay(clayConfig, customClay);
