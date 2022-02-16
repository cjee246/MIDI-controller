
/* -------------------- HEADER --------------------
   MIDI controller
    3D printed, using capacitive touch_curr sensing
    Outputs through MIDI
    Not yet compatible with USB
*/
// Calvin Jee, 2019-06-21

/* -------------------- INIT -------------------- */
//include libraries
#include <MIDI.h>
#include <MIDIUSB.h>
#include <Adafruit_NeoPixel.h>  //rgb flora LED
#include <Adafruit_MPR121.h>    //capacitive sensor

//if USB plugged in
#if defined(USBCON)
const int MIDI_USB = 1;
#else
const int MIDI_USB = 0;
#endif

//setup MIDI
MIDI_CREATE_DEFAULT_INSTANCE();
const uint8_t ch = 1;
const uint8_t MIDI_note[3] = {0, 1, 2};
const uint8_t MIDI_btn[3] = {14, 15, 16}; //cc values
const uint8_t MIDI_cc[6] = {110, 111, 112, 113, 114, 115};
const int _delay = 20;

//setup capacitive sensors
Adafruit_MPR121 cap = Adafruit_MPR121();
const uint8_t ic2addr = 0x5A;

//setup buttons
/*
    0   mod button
    1-3 main buttons
    4-9 +/- buttons
*/
const int BTN_COUNT = 10;

//setup LED
const int LED_COUNT = 1;
const int LED_PIN = 7;
const int LED_PWR = 8;
Adafruit_NeoPixel LED = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
const uint8_t BRIGHT_MIN = 25;
const uint8_t BRIGHT_MAX = 75;

/* -------------------- SETUP -------------------- */
void setup()
{
  //setup LED
  digitalWrite(LED_PWR, HIGH);
  LED.begin();
  LED.setBrightness(BRIGHT_MIN);
  LED.setPixelColor(0, LED.Color(25, 25, 25));
  LED.show();

  //setup MIDI and buttons
  while (!cap.begin(ic2addr));
  Serial.begin(115200);
  delay(100);
  MIDI.begin(1);

}

/* -------------------- PRE-LOOP -------------------- */
//MIDI
uint8_t MIDI_ccVal[6] = {127, 127, 127, 127, 127, 127};
uint8_t MIDI_btnVal[3] = {0, 0, 0};
uint8_t mod = 0;
uint8_t _cc = 0;
uint8_t _btn = 0;
uint8_t _note = 0;

//capacitive sensors
uint16_t touch_curr = 0;
uint16_t touch_last = 0;
uint16_t _pressed = 0;
uint16_t _held = 0;
uint16_t _released = 0;

//LEDs
uint8_t _bright = BRIGHT_MIN;
uint8_t _color[3] = {25, 25, 25}; // {r, g, b}

/* -------------------- LOOP -------------------- */
void loop()
{
topOfLoop:

  //update buttons
  touch_last = touch_curr;
  touch_curr = cap.touched();
  _pressed = touch_curr & ~touch_last;
  _held = touch_curr & touch_last;
  _released = ~touch_curr & touch_last;

  //update LEDs
  if (touch_curr) {
    _bright = BRIGHT_MAX;
  }
  else {
    _bright = BRIGHT_MIN;
  }

  /*---------------------------------------------*/

  //check modifier button
  mod = 0;
  if (touch_curr & bit(0)) {
    mod = 3;
  }

  //check +/- buttons
  _cc = 0;
  for (uint8_t i = 4; i < 10; i++) {

    //minus sign pressed
    if ((touch_curr & bit(i)) && !(touch_curr & bit(i + 1))) {
      MIDI_ccVal[_cc + mod] = MIDI_ccVal[_cc + mod] - 1;
      controlChange(MIDI_cc[_cc + mod], MIDI_ccVal[_cc + mod], ch);
    }

    //plus sign pressed
    else if (!(touch_curr & bit(i)) && (touch_curr & bit(i + 1))) {
      MIDI_ccVal[_cc + mod] = MIDI_ccVal[_cc + mod] + 1;
      controlChange(MIDI_cc[_cc + mod], MIDI_ccVal[_cc + mod], ch);
    }

    //next control
    _cc++;
    i++;
  }

  //if no change, go to top
  if (touch_last == touch_curr) {
    delay(_delay);
    goto topOfLoop;
  }

  /*---------------------------------------------*/

  //check cc buttons -- press on / press off
  if (!mod) {
    _btn = 0;
    for (uint8_t i = 1; i < 4; i++) {
      if (_pressed & bit(i)) {

        //reset other button ccs
        for (int j = 0; j < 3; j++) {
          if (j != _btn) {
            MIDI_btnVal[j] = 0;
            controlChange(MIDI_btn[j], MIDI_btnVal[j], ch);
            _color[j] = 0;
          }
        }

        //change cc button state
        MIDI_btnVal[_btn] = 127 - MIDI_btnVal[_btn];
        controlChange(MIDI_btn[_btn], MIDI_btnVal[_btn], ch);
        _color[_btn] = MIDI_btnVal[_btn];
      }
      _btn++;
    }
  }

  //check notes (mod + button) -- press on / release off
  else if (mod) {
    _note = 0;
    for (uint8_t i = 1; i < 4; i++)
    {
      if (_pressed & bit(i)) {
        noteOn(MIDI_note[_note], 127, ch);
      }
      else if (_released & bit(i)) {
        noteOff(MIDI_note[_note], 0, ch);
      }
      _note++;
    }
  }

  //update LED
  LED.setBrightness(_bright);
  LED.setPixelColor(0 , LED.Color(_color[0] + 25, _color[1] + 25, _color[2] + 25));
  LED.show();
  delay(_delay);

}

/* -------------------- FUNCTIONS -------------------- */
void noteOff(byte pitch, byte velocity, byte channel) {
  MIDI.sendNoteOff(pitch, velocity, channel);
  if (MIDI_USB) {
    midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOff);
    MidiUSB.flush();
  }
}

void noteOn(byte pitch, byte velocity, byte channel) {
  MIDI.sendNoteOn(pitch, velocity, channel);
  if (MIDI_USB) {
    midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
    MidiUSB.sendMIDI(noteOn);
    MidiUSB.flush();
  }
}

void controlChange(byte control, byte value, byte channel) {
  MIDI.sendControlChange(control, value, channel);
  if (MIDI_USB) {
    midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
    MidiUSB.sendMIDI(event);
    MidiUSB.flush();
  }
}
/* ---------- FUTURE ----------
  //lock button - hold main button and press mod button
    if ((touch_curr & bit(i)) & (touch_last & bit(i))
      & (touch_curr & bit(0)) & !(touch_last & bit(0)))
*/
