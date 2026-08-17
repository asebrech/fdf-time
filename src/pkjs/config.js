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
        "messageKey": "Mode",
        "defaultValue": "0",
        "label": "Display mode",
        "options": [
          { "label": "Classic (HH:MM terrain)", "value": "0" },
          { "label": "Seconds (SS terrain)", "value": "1" }
        ]
      },
      {
        "type": "select",
        "messageKey": "WaveMode",
        "defaultValue": "3",
        "label": "Ocean animation",
        "options": [
          { "label": "Silk (ultra-fluid)", "value": "3" },
          { "label": "Fluid (updates every second)", "value": "0" },
          { "label": "Eco (drifts once a minute)", "value": "1" },
          { "label": "Frozen", "value": "2" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "WaveRest",
        "label": "Rest ocean while backlight is off",
        "defaultValue": true
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
        "defaultValue": ""
      },
      {
        "type": "select",
        "messageKey": "Splash42",
        "defaultValue": "6",
        "label": "Splash on launch",
        "options": [
          { "label": "\"42\"", "value": "1" },
          { "label": "My drawing", "value": "5" },
          { "label": "Today's date", "value": "6" },
          { "label": "Battery", "value": "8" },
          { "label": "NixOS", "value": "2" },
          { "label": "Orbit spin", "value": "7" },
          { "label": "Off", "value": "0" }
        ]
      },
      {
        "type": "select",
        "messageKey": "ShakeOrbit",
        "defaultValue": "9",
        "label": "Shake gesture",
        "options": [
          { "label": "\"42\"", "value": "6" },
          { "label": "My drawing", "value": "5" },
          { "label": "Today's date", "value": "4" },
          { "label": "Seconds", "value": "2" },
          { "label": "Battery", "value": "9" },
          { "label": "NixOS", "value": "7" },
          { "label": "Orbit spin", "value": "1" },
          { "label": "Off", "value": "0" }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "LowBatt",
        "label": "Battery alert under 20%",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "WakeFirst",
        "label": "First shake only wakes the screen",
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
