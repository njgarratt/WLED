#pragma once

#include "wled.h"

//v2 usermod that allows to change brightness and color using a rotary encoder,
//change between modes by pressing a button (many encoders have one included)
class RotaryEncoderPowerBrightnessPresets : public Usermod
{
private:
  //Private class members. You can declare variables and functions only accessible to your usermod here
  unsigned long currentTime;
  unsigned long loopTime;
  unsigned long statesettime = 0;
  bool buttonIsPressed = false;
  int16_t preset_no = 1;
  int16_t preset_max = 1;
  // on and off presets
  uint8_t m_onPreset = 0;
  uint8_t m_offPreset = 0;

  // pins
  int8_t SWpin = -1;
  int8_t CLKpin = -1;
  int8_t DTpin = -1;
  bool bInvert = false;

  // reading; Enc_A/Enc_B are loop-local (no state needed between calls), only Enc_A_prev persists
  int Enc_A_prev = 0;

  // scratch buffer for getPresetName() calls; kept as a class member so the String's
  // internal heap buffer is allocated once and reused, avoiding repeated alloc/free
  // churn on every encoder pulse while spinning through presets.
  String tmpname;

  unsigned char button_state = HIGH;
  unsigned char prev_button_state = HIGH;

  // private class members configurable by Usermod Settings (defaults set inside readFromConfig())
  int fadeAmount;      // how many points to fade the Neopixel with each step
  uint8_t minBri;      // minimum brightness when rotating down (0 = allow full off)
  unsigned long clicktime; // how many ms is a click less than. longer is ignored.

public:
  //Functions called by WLED

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
  {
    if (CLKpin < 0 || DTpin < 0) return;  // encoder pins mandatory; bail if unconfigured
    pinMode(DTpin,  INPUT_PULLUP);
    pinMode(CLKpin, INPUT_PULLUP);
    if (SWpin >= 0) pinMode(SWpin, INPUT_PULLUP);
    currentTime = millis();
    loopTime    = currentTime;
  }

  /**
   * switch strip on/off
   */
  void switchStrip(bool switchOn)
  {
    if (switchOn && m_onPreset) {
      applyPreset(m_onPreset);
    } else if (!switchOn && m_offPreset) {
      applyPreset(m_offPreset);
    } else if (switchOn && bri == 0) {
      // clamp briLast: if it's 0 (e.g. first boot before ever being on) use a safe default
      bri = briLast > 0 ? briLast : 128;
      colorUpdated(CALL_MODE_BUTTON);  // notify all clients (web UI, MQTT, etc.)
    } else if (!switchOn && bri != 0) {
      briLast = bri;
      bri = 0;
      colorUpdated(CALL_MODE_BUTTON);  // notify all clients (web UI, MQTT, etc.)
    }
  }

  /*
   * loop() is called continuously. Here you can check for events, read sensors, etc.
   *
   * Tips:
   * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
   *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
   *
   * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
   *    Instead, use a timer check as shown here.
   */
  void loop()
  {
    if (CLKpin < 0 || DTpin < 0) return;  // skip if encoder pins not configured

    currentTime = millis(); // get the current elapsed time

    if ((currentTime - loopTime) >= 2) // 2ms since last check of encoder = 500Hz
    {
      /* we have a button defined
      * single click - cycle power
      * hold down and rotate - cycle presets
      * rotate - brightness
      */
      if (SWpin >= 0) {
        button_state = digitalRead(SWpin);
        if (button_state == LOW)
        {
          buttonIsPressed = true;
        } else
        {
          buttonIsPressed = false;
        }
        // has button state changed since last time?
        if (prev_button_state != button_state)
        {
          // toggle power state
          if (buttonIsPressed)
          {
            DEBUG_PRINTLN(F("button is pressed"));
            statesettime = currentTime;
          }
          else
          {
            DEBUG_PRINTLN(F("button HIGH"));
            /*
            * declick action
            * toggle power for a click, but not for a hold
            */
            if ((currentTime - statesettime) <= clicktime)
            {
              // derive on/off state directly from bri to stay in sync
              // with changes made via MQTT, HTTP API, web UI, etc.
              if (bri > 0)
              {
                switchStrip(false);
              }
              else
              {
                switchStrip(true);
              }
            }
          }
          prev_button_state = button_state;
        }
      }
      // Read encoder pins; declared local as no state is needed between loop() calls
      int Enc_A, Enc_B;
      if (!bInvert)
      {
        Enc_A = digitalRead(DTpin);
        Enc_B = digitalRead(CLKpin);
      } else
      {
        Enc_A = digitalRead(CLKpin);
        Enc_B = digitalRead(DTpin);
      }
      if ((!Enc_A) && (Enc_A_prev))
      { // A has gone from high to low
        bool presetChanged = false;

        if (Enc_B == HIGH)
        { // B is high so clockwise
          if (!buttonIsPressed)
          {
            // increase the brightness, dont go over 255!
            if (bri + fadeAmount <= 255)
            {
              bri += fadeAmount;
            } else
            {
              // account for increments which are not factors of 255
              bri = 255;
            }
          }
          else
          {
            presetChanged = true;
            preset_no++;

            // skip gaps in preset numbering; if we go past preset_max wrap to 1.
            // bounded by preset_max iterations to prevent an infinite loop if no presets exist.
            for (int i = 0; i < preset_max && !getPresetName(preset_no, tmpname); i++)
            {
              preset_no++;
              if (preset_no > preset_max) { preset_no = 1; break; }
            }
            applyPreset(preset_no);
            if (preset_no > preset_max)
            {
              // capture the largest preset we have successfully applied
              preset_max = preset_no;
            }
          }
        }
        else if (Enc_B == LOW)
        { // B is low so counter-clockwise
          if (!buttonIsPressed)
          {
            // decrease the brightness, dont go below minBri!
            if (bri - fadeAmount >= (int)minBri)
            {
              bri -= fadeAmount;
            } else
            {
              // account for increments which are not factors of minBri
              bri = minBri;
            }
          }
          else
          {
            presetChanged = true;
            preset_no = preset_no - 1;
            if (preset_no <= 0)
            {
              // loop around to the highest preset number we have previously seen
              preset_no = preset_max;
            }
            // skip gaps in preset numbering; loop to handle multiple consecutive missing presets
            for (int i = 0; i < preset_max && !getPresetName(preset_no, tmpname); i++)
            {
              preset_no--;
              if (preset_no <= 0) { preset_no = preset_max; break; }
            }
            applyPreset(preset_no);
          }
        }
        // only call colorUpdated() for brightness changes; preset application is async
        // and handlePresets() will call stateUpdated() + updateInterfaces() itself
        if (!presetChanged)
        {
          //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
          // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
          colorUpdated(CALL_MODE_BUTTON);
          updateInterfaces(CALL_MODE_BUTTON);
        }
      }
      Enc_A_prev = Enc_A;     // Store value of A for next time
      loopTime = currentTime; // Updates loopTime
    }
  }

  void addToJsonInfo(JsonObject& root)
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray presetArr = user.createNestedArray(F("Rotary Preset"));
    presetArr.add(preset_no);

    JsonArray stateArr = user.createNestedArray(F("Rotary Power"));
    stateArr.add(bri > 0 ? F("on") : F("off"));
  }

  void addToConfig(JsonObject& root)
  {
    JsonObject top = root.createNestedObject("RotaryEncoder Power Brightness Presets");
    top["fadeAmount"] = fadeAmount;
    top["minBri"]     = minBri;
    top["clickMs"]    = clicktime;
    top["SWpin"]      = SWpin;
    top["CLKpin"]     = CLKpin;
    top["DTpin"]      = DTpin;
    top["Invert"]     = bInvert;
    top["onPreset"]   = m_onPreset;
    top["offPreset"]  = m_offPreset;
  }

  /*
   * This example uses a more robust method of checking for missing values in the config, and setting back to defaults:
   * - The getJsonValue() function copies the value to the variable only if the key requested is present, returning false with no copy if the value isn't present
   * - configComplete is used to return false if any value is missing, not just if the main object is missing
   * - The defaults are loaded every time readFromConfig() is run, not just once after boot
   *
   * This ensures that missing values are added to the config, with their default values, in the rare but plauible cases of:
   * - a single value being missing at boot, e.g. if the Usermod was upgraded and a new setting was added
   * - a single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)
   *
   * If configComplete is false, the default values are already set, and by returning false, WLED now knows it needs to save the defaults by calling addToConfig()
   */
  bool readFromConfig(JsonObject& root)
  {
    // set defaults here, they will be set before setup() is called, and if any values parsed from ArduinoJson below are missing, the default will be used instead
    fadeAmount = 5;
    minBri     = 0;
    clicktime  = 250;
    SWpin      = -1;
    CLKpin     = -1;
    DTpin      = -1;
    bInvert    = false;
    m_onPreset  = 0;
    m_offPreset = 0;

    JsonObject top = root["RotaryEncoder Power Brightness Presets"];

    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["fadeAmount"], fadeAmount);
    configComplete &= getJsonValue(top["minBri"],     minBri);
    configComplete &= getJsonValue(top["clickMs"],    clicktime);
    configComplete &= getJsonValue(top["SWpin"],      SWpin);
    configComplete &= getJsonValue(top["DTpin"],      DTpin);
    configComplete &= getJsonValue(top["CLKpin"],     CLKpin);
    configComplete &= getJsonValue(top["Invert"],     bInvert);
    configComplete &= getJsonValue(top["onPreset"],   m_onPreset);
    configComplete &= getJsonValue(top["offPreset"],  m_offPreset);

    return configComplete;
  }
};
