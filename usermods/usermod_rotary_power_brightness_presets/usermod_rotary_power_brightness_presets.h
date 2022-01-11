#pragma once

#include "wled.h"

//v2 usermod that allows to change brightness and color using a rotary encoder, 
//change between modes by pressing a button (many encoders have one included)
class RotaryEncoderPowerBrightnessPresets : public Usermod
{
private:
  //Private class members. You can declare variables and functions only accessible to your usermod here
  // notification mode for colorUpdated()
  const byte NotifyUpdateMode = CALL_MODE_NO_NOTIFY;
  unsigned long lastTime = 0;
  unsigned long currentTime;
  unsigned long loopTime;
  unsigned long statesettime = 0;
  bool buttonIsPressed;
  int16_t preset_no = 1;
  int16_t preset_max = 1;
    // on and off presets
  uint8_t m_onPreset = 0;
  uint8_t m_offPreset = 0;

  // pins
  int8_t SWpin = -1;
  int8_t CLKpin = -1;
  int8_t DTpin = -1;
  



  unsigned char select_state = 0; // 0 = brightness 1 = color
  unsigned char button_state = HIGH;
  unsigned char prev_button_state = HIGH;
  CRGB fastled_col;
  CHSV prim_hsv;
  int16_t new_val;

  unsigned char Enc_A;
  unsigned char Enc_B;
  unsigned char Enc_A_prev = 0;

  // private class members configurable by Usermod Settings (defaults set inside readFromConfig())
  int fadeAmount; // how many points to fade the Neopixel with each step
  unsigned int clicktime; // how many ms is a click less than. longer is ignored.

public:
  //Functions called by WLED

  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
  {
    //Serial.println("Hello from my usermod!");
    pinMode(DTpin, INPUT_PULLUP);
    pinMode(CLKpin, INPUT_PULLUP);
    if(SWpin >= 0) pinMode(SWpin, INPUT_PULLUP);
    currentTime = millis();
    loopTime = currentTime;
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
      bri = briLast;
      colorUpdated(NotifyUpdateMode);
    } else if (!switchOn && bri != 0) {
      briLast = bri;
      bri = 0;
      colorUpdated(NotifyUpdateMode);
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
    currentTime = millis(); // get the current elapsed time

    if (currentTime >= (loopTime + 2)) // 2ms since last check of encoder = 500Hz
    {
      /* we have a button defined
      * single click - cycle power
      * hold and rotate - cycle presets
      * rotate - brightness
      */
      if(SWpin >= 0) {
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
            Serial.println("button is pressed");
            statesettime = currentTime;
          }
          else
          {
            Serial.println("button HIGH");
            // declick action
            if ((currentTime - statesettime) <= clicktime)
            {
              if (select_state == 1)
              {
                select_state = 0;
                switchStrip(false);
              }
              else
              {
                select_state = 1;
                switchStrip(true);
              } 
            }
          }
          prev_button_state = button_state;
        }
      }
      // Read encoder pins
      int Enc_A = digitalRead(DTpin); 
      int Enc_B = digitalRead(CLKpin);
      if ((!Enc_A) && (Enc_A_prev))
      { // A has gone from high to low
        if (Enc_B == HIGH)
        { // B is high so clockwise
          if (!buttonIsPressed)
          {
            if (bri + fadeAmount <= 255)
              bri += fadeAmount; // increase the brightness, dont go over 255
          }
          else
          {
            // bool applyPreset(byte index, bool loadBri)
            preset_no = preset_no + 1;
            
            if (!applyPreset(preset_no))
            {
              preset_no = 1;
              applyPreset(preset_no);
            } else if (preset_no > preset_max)
            { 
              preset_max = preset_no;
            }
          }
        }
        else if (Enc_B == LOW)
        { // B is low so counter-clockwise
          if (!buttonIsPressed)
          {
            if (bri - fadeAmount >= 0)
              bri -= fadeAmount; // decrease the brightness, dont go below 0
          }
          else
          {
            preset_no = preset_no - 1;
            if (preset_no <= 0)
            {
              preset_no = preset_max;
            }
            applyPreset(preset_no);
          }
        }
        //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
        // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
        colorUpdated(CALL_MODE_BUTTON);
        updateInterfaces(CALL_MODE_BUTTON);
      }
      Enc_A_prev = Enc_A;     // Store value of A for next time
      loopTime = currentTime; // Updates loopTime
    }
  }

  void addToConfig(JsonObject& root)
  {
    JsonObject top = root.createNestedObject("RotaryEncoderPowerBrightnessPresets");
    top["fadeAmount"] = fadeAmount;
    top["clickMs"]  = clicktime;
    top["SWpin"] = SWpin;
    top["CLKpin"] = CLKpin;
    top["DTpin"] = DTpin;
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
    clicktime = 250;
    SWpin = -1;
    CLKpin = -1;
    DTpin = -1;

    JsonObject top = root["RotaryEncoderPowerBrightnessPresets"];

    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["fadeAmount"], fadeAmount);
    configComplete &= getJsonValue(top["clickMs"], clicktime);
    configComplete &= getJsonValue(top["SWpin"], SWpin);
    configComplete &= getJsonValue(top["DTpin"], DTpin);
    configComplete &= getJsonValue(top["CLKpin"], CLKpin);

    return configComplete;
  }
};
