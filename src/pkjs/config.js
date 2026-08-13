module.exports = [
  {
    "type": "heading",
    "defaultValue": "FdF Time"
  },
  {
    "type": "text",
    "defaultValue": "Color options apply to color Pebbles only."
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Look" },
      {
        "type": "select",
        "messageKey": "Theme",
        "defaultValue": "0",
        "label": "Color theme",
        "options": [
          { "label": "Classic FdF", "value": "0" },
          { "label": "Matrix", "value": "1" },
          { "label": "Lava", "value": "2" },
          { "label": "Ice", "value": "3" }
        ]
      },
      {
        "type": "select",
        "messageKey": "Relief",
        "defaultValue": "1",
        "label": "Wall gradient intensity",
        "options": [
          { "label": "Subtle", "value": "0" },
          { "label": "Balanced", "value": "1" },
          { "label": "Vivid", "value": "2" }
        ]
      },
      {
        "type": "select",
        "messageKey": "WaveMode",
        "defaultValue": "0",
        "label": "Ocean animation",
        "options": [
          { "label": "Fluid (updates every second)", "value": "0" },
          { "label": "Eco (drifts once a minute)", "value": "1" },
          { "label": "Frozen", "value": "2" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Behavior" },
      {
        "type": "toggle",
        "messageKey": "Splash42",
        "label": "\"42\" splash on launch",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "ShakeOrbit",
        "label": "Orbit spin on shake",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "BtVibe",
        "label": "Vibrate on Bluetooth loss",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
