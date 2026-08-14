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
          { "label": "Tokyo Night", "value": "0" },
          { "label": "Catppuccin", "value": "1" },
          { "label": "Dracula", "value": "2" },
          { "label": "Gruvbox", "value": "3" },
          { "label": "Kanagawa", "value": "4" },
          { "label": "Nord", "value": "5" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "Gradient",
        "label": "Wall gradients",
        "defaultValue": true
      },
      {
        "type": "select",
        "messageKey": "Mode",
        "defaultValue": "0",
        "label": "Display mode",
        "options": [
          { "label": "Classic (HH:MM terrain)", "value": "0" },
          { "label": "Seconds (experimental, drains battery)", "value": "1" }
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
        "type": "select",
        "messageKey": "ShakeOrbit",
        "defaultValue": "1",
        "label": "Shake gesture",
        "options": [
          { "label": "Orbit spin", "value": "1" },
          { "label": "Peek at seconds (until the minute ends)", "value": "2" },
          { "label": "Toggle seconds (until the next shake)", "value": "3" },
          { "label": "Off", "value": "0" }
        ]
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
