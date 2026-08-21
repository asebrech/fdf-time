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
        "defaultValue": "1",
        "label": "Color theme",
        "options": [
          { "label": "Catppuccin", "value": "1" },
          { "label": "Dracula", "value": "2" },
          { "label": "Gruvbox", "value": "3" },
          { "label": "Kanagawa", "value": "4" },
          { "label": "Matrix", "value": "6" },
          { "label": "Nord", "value": "5" },
          { "label": "Tokyo Night", "value": "0" }
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
        "messageKey": "TimeFormat",
        "defaultValue": "0",
        "label": "Time format",
        "options": [
          { "label": "Watch setting", "value": "0" },
          { "label": "12h (AM/PM)", "value": "1" },
          { "label": "24h", "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "ShowAmPm",
        "label": "AM/PM indicator (12h)",
        "defaultValue": true
      },
      {
        "type": "select",
        "messageKey": "WaveMode",
        "defaultValue": "4",
        "label": "Ocean animation",
        "options": [
          { "label": "Pulse (beats with your heart)", "value": "4" },
          { "label": "Silk (ultra-fluid)", "value": "3" },
          { "label": "Fluid (updates every second)", "value": "0" },
          { "label": "Eco (drifts once a minute)", "value": "1" },
          { "label": "Frozen", "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "WaveRest",
        "label": "Pause ocean to save battery (freezes it outdoors)",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Behavior" },
      {
        "type": "pixelGrid",
        "messageKey": "CustomMap",
        "label": "Your drawing",
        "defaultValue": "000000003f0000ffc003fff007fff80ffffc0ffffc1ffffe1ffffe387f87303f03301e03181e06180c061c0c0e0e3f1c07fff807def803c0f001f3e000ffc0007f80003f00000c00000000"
      },
      {
        "type": "select",
        "messageKey": "Splash42",
        "defaultValue": "5",
        "label": "Splash on launch",
        "options": [
          { "label": "My drawing", "value": "5" },
          { "label": "Orbit spin", "value": "7" },
          { "label": "Today's date", "value": "6" },
          { "label": "Battery", "value": "8" },
          { "label": "Heart rate", "value": "9" },
          { "label": "Off", "value": "0" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ShakeOrbit",
        "defaultValue": "1",
        "label": "Shake gesture",
        "options": [
          { "label": "Orbit spin", "value": "1" },
          { "label": "My drawing", "value": "5" },
          { "label": "Seconds", "value": "2" },
          { "label": "Today's date", "value": "4" },
          { "label": "Battery", "value": "9" },
          { "label": "Heart rate", "value": "10" },
          { "label": "Off", "value": "0" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "LowBatt",
        "label": "Battery alert under 12%",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "WakeFirst",
        "label": "First shake only wakes the screen",
        "defaultValue": false
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
