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
        "defaultValue": "3",
        "label": "Color theme",
        "options": [
          { "label": "Catppuccin", "value": "0" },
          { "label": "Gruvbox", "value": "1" },
          { "label": "Nord", "value": "2" },
          { "label": "Tokyo Night", "value": "3" },
          { "label": "Kanagawa", "value": "4" },
          { "label": "Dracula", "value": "5" }
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
