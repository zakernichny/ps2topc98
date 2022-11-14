////////////////////////////////////////////////////////////////////////////////
// PS/2 (AT) to PC-9800 Series keyboard converter V1.2                        //
// copyleft zake 2022 (look, just don't sell arduinos with this for stupid    //
// money on ebay or yahoo auctions or wherever)                               //
// Discord: zake#0138 (granted they haven't banned me again).                 //
// Inspired by a project from AVX Studios                                     //
// (https://pulsethread.com/pc98/ps2pc98arduino.html), therefore, only runs   //
// on boards with at least two hardware serial interfaces (one is reserved    //
// for programming). Might be possible to use software serial on cheaper      //
// boards. Set up for and tested on a SparkFun Pro Micro clone (ATmega32U4).  //
// PC-98 and PS/2 interfaces are based on PC-9800 Series Technical Data Book  //
// and amazing articles by Adam Chapweske on the topic respectively.          //
////////////////////////////////////////////////////////////////////////////////

#define pc98ser Serial1  //PC-98 keyboard serial interface
//#define usbser Serial  //Uncomment for USB serial debugging, disables PC-98 interface

//PS/2 interface pins
#define DATA 2  //Pin 8 on AVX hardware
#define CLOCK 3  //Pin 2 on AVX hardware, must be an interrupt-capable pin!

//PC-98 keyboard interface pins
#define RST 4  //Reset
#define RDY 5  //Ready
#define RTY 6  //Retransmit

#define pc98tm 0x22  //Typematic delays similar to a 98NOTE keyboard (0.5 s delay, ~24.6 cps)
//#define pc98tm 0x26  //That would be closer to the datasheet (0.5 s delay, 16.(6) cps)

#define numlock  //Enable num lock by default
#define numkeep  //Keep num lock status on reset
//IMPORTANT: when num lock LED is lit or num lock key was pressed nav cluster and arrow keys produce
//           E0 12 and E0 F0 12 ("fake shift") before make and after break scancodes, but don't produce
//           the usual "fake unshifts" when shift key is held down. Should explain some of the comments.

uint8_t ps2clk = 10;  //PS/2 interrupt clock counter
uint8_t ps2data = 0;  //PS/2 interrupt data buffer
uint8_t scancode = 0;  //Input PS/2 and output PC-98 scancode
uint8_t locks = 0;  //Bits 0-3 - lock statuses; bits 4-7 - lock keys pressed; bits 3 and 7 are unused
uint8_t tmrate = pc98tm;  //Typematic delays
uint8_t status = 0b00000100;  //Bit 0 - keybreak;
                              //bit 1 - extend;
                              //bit 2 - num lock (inverted, repeated here for quicker evaluations in converters);
                              //bit 3 - reserved;
                              //bit 4 - predictive conversion;
                              //bits 5-7 - map ID

void setup() {
  pinMode(RST, INPUT);
  pinMode(RDY, INPUT);
  pinMode(RTY, INPUT);  //Init PC-98 interface pins
  pinMode(CLOCK, INPUT_PULLUP);
  pinMode(DATA, INPUT_PULLUP);  //Init PS/2 interface pins
  #ifdef usbser  //USB debugging
  usbser.begin(500000, SERIAL_8N1);
  #else
  pc98ser.begin(19200, SERIAL_8O1);  //Start PC-98 interface
  #endif
  delay(500);  //Wait until self-test is complete
  tmset();  //Set typematic timings, ps2send will attach a receive interrupt after it's done
  #ifdef numlock  //Num Lock on startup
  status &= 0b11111011; 
  locks |= 0b00000010; 
  ledset(0x10);
  #endif
}

void loop() {  //Main loop, waits for a scancode
  do if (scancode) convfull(); while (status >> 5 == 0);  //Run the converter when a scancode arrives
  do if (scancode) convtoho(); while (status >> 5 == 1);  //Don't forget to change last map ID in nextmap() when adding new layouts
  do if (scancode) convyume(); while (status >> 5 == 2);
  do if (scancode) convymsp(); while (status >> 5 != 0);  //Last map condition suggestion
}

void reset() {  //Keyboard reset
  ps2send(0xFF);  //Send reset request
  delay(500);  //Wait until self-test is complete
  tmset();  //Reset typematic delays
  #ifndef numkeep
  #ifdef numlock  //Num Lock on reset
  status &= 0b11111011;
  locks |= 0b00000010;
  #else
  status |= 0b00000100;
  locks &= 0b11111101;
  #endif
  #endif
}

void tmset() {  //Set keyboard typematic delays
  ps2send(0xF3);  //Send typematic setup command
  ps2send(tmrate);  //Send typematic delays
}

void ledset(uint8_t ledstatus) {
  ps2send(0xED);  //Send set LEDs command
  if (ledstatus < 0x10) ps2send(ledstatus);  //Send LED status if valid
  else ps2send(locks & 0b00001111);  //Otherwise show current lock status
}

void pc98send(uint8_t sendcode) {  //Output PC-98 scancode
  static uint8_t lastcode = 0;
  if (sendcode < 0x80) {  //If valid PC-98 MAKE scancode
    if (status & 0b00000001) {  //Check keybreak
      sendcode |= 0x80;  //MAKE -> BREAK scancode
      status &= 0b11111110;  //Unset keybreak
    }
    #ifdef usbser  //USB debugging
    if (status & 0b00010000) usbser.print(ps2clk, DEC);  //PS/2 clock cycles elapsed (for fast conversion)
    usbser.print("= ");
    usbser.println(sendcode, HEX);  //Projected output scancode
    #else
    if (digitalRead(RTY) == LOW) {  //Hanlde a resend request
      while (digitalRead(RDY) == HIGH);  //Wait until host is ready
      pc98ser.write(lastcode);  //Resend last scancode
    }
    if (digitalRead(RST) == LOW) {  //Handle a reset request, would require rewiring and setting up an interrupt to do a proper reset
      while (digitalRead(RST) == LOW);  //Wait until RST is released
      delay(13);  //Pretend we're resetting
      while (digitalRead(RDY) == HIGH);  //RDY should be low at this point, wait in case it isn't
      delayMicroseconds(100);  //Wait before transmitting in compliance with the data book
    } else while (digitalRead(RDY) == HIGH);  //Wait until host is ready
    pc98ser.write(sendcode);  //He npowJIo u roga...
    #endif
    lastcode = sendcode;  //Remember sent scancode in case host requests a resend
  }
  if (sendcode == 0xFF) status &= 0b11111110;  //Unset keybreak, use 0xFE to preserve keybreak and still not output
  scancode = 0;  //Clear main loop condition
}

void ps2send(uint8_t command) {  //Send PS/2 command
  uint32_t exectime = 0;  //Usage of millis() in this function is questionable, really
  while (ps2clk < 10) delay(1);  //Wait for the reception to end
  detachInterrupt(digitalPinToInterrupt(CLOCK));  //Detach receive interrupt, if attached
  ps2data = command;  //Bring command byte outside
  ps2clk = 12;  //Init clock counter for transmit interrupt
  delay(5);  //Some keyboards won't receive properly without this delay, 2 ms might be enough though
  exectime = millis();  //Remember time on transmission start
  pinMode(CLOCK, OUTPUT);
  digitalWrite(CLOCK, LOW);  //Pull clock low for more than 60 us
  delayMicroseconds(100);  //Playing it safe and waiting 100 us just to be sure
  pinMode(DATA, OUTPUT);
  digitalWrite(DATA, LOW);  //Pull data low, start bit
  attachInterrupt(digitalPinToInterrupt(CLOCK), ps2tx, FALLING);  //Attach transmit interrupt
  pinMode(CLOCK, INPUT_PULLUP);  //Release clock
  while (ps2clk != 11) if (millis() - exectime > 17) {  //Wait for transmission to end or fail (stops working if you look at it the wrong way)
    pinMode(DATA, INPUT_PULLUP);  //Set pin mode to input, assuming it's still an output
    ps2clk = 11;  //Get out of the loop
  }
  detachInterrupt(digitalPinToInterrupt(CLOCK));  //Detach transmit interrupt
  ps2clk = 10;  //Reset clock counter
  if (status & 0b00010000) attachInterrupt(digitalPinToInterrupt(CLOCK), ps2rxfast, FALLING);
  else attachInterrupt(digitalPinToInterrupt(CLOCK), ps2rx, FALLING);  //Attach (reattach) an appropriate receive interrupt
}

void locktgl(uint8_t lockid) {  //Toggle lock state properly
  if (status & 0b00000001) {  //Check keybreak
    scancode = 0xFF;  //Tell pc98send not to output
    locks &= ~(0b00010000 << lockid);  //Unset lock pressed
  } else {
    if ((locks >> lockid) & 0b00010000) scancode = 0xFF;
    else {  //If the key was just pressed
      switch (lockid) {
        case 0: scancode = 0x72; break;  //Kana Lock
        case 1: scancode = 0xFF; status ^= 0b00000100; break;  //Num Lock
        case 2: scancode = 0x71; break;  //Caps Lock
        //case 3: scancode = 0xFF; break;  //Reserved
      }
      if ((locks >> lockid) & 0b00000001) status |= 0b00000001;  //Set keybreak
      locks ^= 0b00000001 << lockid;  //Toggle lock
      locks |= 0b00010000 << lockid;  //Set lock pressed
    }
  }
  ledset(0x10);  //Update lock LEDs, move this to cases in converters to avoid accidentally enabling indication (see nextmap)
}

void nextmap() {  //Switch to the next conversion table
  int8_t map;
  detachInterrupt(digitalPinToInterrupt(CLOCK));  //Detach PS/2 interrupt ASAP to ignore the rest of the scancodes for Pause Break
  delay(50);  //Wait until Pause Break is done vomiting scancodes
  map = status >> 5;  //Get map ID from status
  status &= 0b00001100;  //Reset keybreak, extend, predictive conversion and map ID
  if (map < 3) map++; else map = 0;  //Change condition when adding new maps, up to 8 total (0 through 7)
  status |= map << 5;  //Set new map ID
  if (map == 1) status |= 0b00010000;  //Set predictive conversion for map 1
  ps2clk = 10;  //In case interrupt fired
  if (map > 0) {  //Different initialization procedures for maps greater than 0, please consider when adding new maps
    tmrate = 0x7F;  //Slowest typematic, might increase performance in games
    tmset();  //0xF8 disables typematic completely, but requires migrating to Set 3 thus breaking AT keyboard compatibility
    while (map > -1) {  //Blink to indicate mode ID
      ledset(0x00); delay(100);  //Turn off all LEDs for 100 ms
      ledset(0x07);  //Turn on all LEDs
      if (map > 2) {delay(200); map -= 3;}  //Up to 2 long flashes (200 ms)
      else {delay(100); map--;}  //Up to 3 short flashes (100 ms)
    }
    ledset(0x00);  //Turn off all LEDs
  } else {
    tmrate = pc98tm;  //Set PC-98 typematic again
    reset();  //Reset to blink the LEDs and set num lock
    ledset(0x10);  //Show lock statuses again
  }
  if (status & 0b00010000) delay(50);  //Wait until keyboard sends 0xFA, it gets misinterpreted in fast converters
}

//Commented out scancodes are optional, remember to disable what they replace when uncommenting.

void convfull() {  //Full standard 101/102-key layout conversion, unusual mappings are marked with arrows
  #ifdef usbser  //USB debugging
  usbser.print(scancode, HEX);
  usbser.print(" ");
  #endif
  switch (scancode) {                   //PS/2 key (PC-98 KEY, if different)
    case 0x01: scancode = 0x6A; break;  //F9
    case 0x03: scancode = 0x66; break;  //F5
    case 0x04: scancode = 0x64; break;  //F3
    case 0x05: scancode = 0x62; break;  //F1
    case 0x06: scancode = 0x63; break;  //F2
    case 0x07: scancode = 0x53; break;  //F12 (VF2)
    //case 0x08: scancode = 0x54; break;  //F13 (VF3)
    case 0x09: scancode = 0x6B; break;  //F10
    case 0x0A: scancode = 0x69; break;  //F8
    case 0x0B: scancode = 0x67; break;  //F6
    case 0x0C: scancode = 0x65; break;  //F4
    case 0x0D: scancode = 0x0F; break;  //Tab
    case 0x0E: scancode = 0x1A; break;  //~ (@) <-------------------
    //case 0x10: scancode = 0x55; break;  //F14 (VF4)
    case 0x11: scancode = 0x55; status &= 0b11111101; break;  //LAlt, RAlt (GRPH)
    case 0x12: if (status & 0b00000010) {scancode = 0xFF; status &= 0b11111101;} else scancode = 0x70; break;  //LShift (SHIFT), dismiss fake shifts
    case 0x14: if (status & 0b00000010) {scancode = 0x33; status &= 0b11111101;} else scancode = 0x74; break;  //LCtrl (CTRL), RCtrl (--) <-------------------
    case 0x15: scancode = 0x10; break;  //Q
    case 0x16: scancode = 0x01; break;  //1
    //case 0x18: scancode = 0x56; break;  //F15 (VF5)
    case 0x1A: scancode = 0x29; break;  //Z
    case 0x1B: scancode = 0x1E; break;  //S
    case 0x1C: scancode = 0x1D; break;  //A
    case 0x1D: scancode = 0x11; break;  //W
    case 0x1E: scancode = 0x02; break;  //2
    case 0x1F: scancode = 0x51; status &= 0b11111101; break;  //LWin (NFER) <-------------------
    case 0x21: scancode = 0x2B; break;  //C
    case 0x22: scancode = 0x2A; break;  //X
    case 0x23: scancode = 0x1F; break;  //D
    case 0x24: scancode = 0x12; break;  //E
    case 0x25: scancode = 0x04; break;  //4
    case 0x26: scancode = 0x03; break;  //3
    case 0x27: scancode = 0x35; status &= 0b11111101; break;  //RWin (XFER) <-------------------
    case 0x29: scancode = 0x34; break;  //Space
    case 0x2A: scancode = 0x2C; break;  //V
    case 0x2B: scancode = 0x20; break;  //F
    case 0x2C: scancode = 0x14; break;  //T
    case 0x2D: scancode = 0x13; break;  //R
    case 0x2E: scancode = 0x05; break;  //5
    case 0x2F: locktgl(0); status &= 0b11111101; break;  //Menu (KANA) <-------------------
    case 0x31: scancode = 0x2E; break;  //N
    case 0x32: scancode = 0x2D; break;  //B
    case 0x33: scancode = 0x22; break;  //H
    case 0x34: scancode = 0x21; break;  //G
    case 0x35: scancode = 0x15; break;  //Y
    case 0x36: scancode = 0x06; break;  //6
    case 0x37: scancode = 0x56; status &= 0b11111101; break;  //Power (VF5) <-------------------
    case 0x3A: scancode = 0x2F; break;  //M
    case 0x3B: scancode = 0x23; break;  //J
    case 0x3C: scancode = 0x16; break;  //U
    case 0x3D: scancode = 0x07; break;  //7
    case 0x3E: scancode = 0x08; break;  //8
    case 0x3F: scancode = 0x55; status &= 0b11111101; break;  //Sleep (VF4) <-------------------
    case 0x41: scancode = 0x30; break;  //,
    case 0x42: scancode = 0x24; break;  //K
    case 0x43: scancode = 0x17; break;  //I
    case 0x44: scancode = 0x18; break;  //O
    case 0x45: scancode = 0x0A; break;  //0
    case 0x46: scancode = 0x09; break;  //9
    case 0x49: scancode = 0x31; break;  //.
    case 0x4A: if (status & 0b00000010) {scancode = 0x41; status &= 0b11111101;} else scancode = 0x32; break;  // /, Numpad /
    case 0x4B: scancode = 0x25; break;  //L
    case 0x4C: scancode = 0x26; break;  //: (;)
    case 0x4D: scancode = 0x19; break;  //P
    case 0x4E: scancode = 0x0B; break;  //-
    case 0x52: scancode = 0x27; break;  //" (:)
    case 0x54: scancode = 0x1B; break;  //[ <-------------------
    case 0x55: scancode = 0x0C; break;  //= (^)
    case 0x58: locktgl(2); break;  //Caps Lock
    case 0x59: scancode = 0x70; break;  //RShift (SHIFT)
    case 0x5A: if (status & 0b00000010) {scancode = 0x4D; status &= 0b11111101;} else scancode = 0x1C; break;  //Enter, Numpad Enter (NUMPAD =) <-------------------
    case 0x5B: scancode = 0x28; break;  //] <-------------------
    case 0x5D: scancode = 0x0D; break;  //Backslash (YEN)
    case 0x5E: scancode = 0x54; status &= 0b11111101; break;  //Wake (VF3) <-------------------
    case 0x66: scancode = 0x0E; break;  //Backspace
    case 0x69: if (status & 0b00000110) {scancode = 0x3F; status &= 0b11111101;} else scancode = 0x4A; break;  //Numpad 1, End (HELP)
    case 0x6B: if (status & 0b00000110) {scancode = 0x3B; status &= 0b11111101;} else scancode = 0x46; break;  //Numpad 4, Left
    case 0x6C: if (status & 0b00000110) {scancode = 0x3E; status &= 0b11111101;} else scancode = 0x42; break;  //Numpad 7, Home (HOME CLR)
    //case 0x6C: if (status & 0b00000110) {scancode = 0x5E; status &= 0b11111101;} else scancode = 0x42; break;  //Numpad 7, Home (HOME)
    case 0x70: if (status & 0b00000110) {scancode = 0x38; status &= 0b11111101;} else scancode = 0x4E; break;  //Numpad 0, Insert
    //case 0x71: if (status & 0b00000110) {scancode = 0x39; status &= 0b11111101;} else scancode = 0x4F; break;  //Numpad . (,), Delete
    case 0x71: if (status & 0b00000110) {scancode = 0x39; status &= 0b11111101;} else scancode = 0x50; break;  //Numpad . (.), Delete
    case 0x72: if (status & 0b00000110) {scancode = 0x3D; status &= 0b11111101;} else scancode = 0x4B; break;  //Numpad 2, Down
    case 0x73: if (status & 0b00000100) scancode = 0xFF; else scancode = 0x47; break;  //Numpad 5
    case 0x74: if (status & 0b00000110) {scancode = 0x3C; status &= 0b11111101;} else scancode = 0x48; break;  //Numpad 6, Right
    case 0x75: if (status & 0b00000110) {scancode = 0x3A; status &= 0b11111101;} else scancode = 0x43; break;  //Numpad 8, Up
    case 0x76: scancode = 0x00; break;  //Esc
    case 0x77: locktgl(1); break;  //Num Lock
    case 0x78: scancode = 0x52; break;  //F11 (VF1)
    case 0x79: scancode = 0x49; break;  //Numpad +
    case 0x7A: if (status & 0b00000110) {scancode = 0x36; status &= 0b11111101;} else scancode = 0x4C; break;  //Numpad 3, Page Down (ROLL UP)
    case 0x7B: scancode = 0x40; break;  //Numpad -
    case 0x7C: if (status & 0b00000010) {scancode = 0x60; status &= 0b11111101;} else scancode = 0x45; break;  //Numpad *, Print Screen (STOP) <-------------------
    case 0x7D: if (status & 0b00000110) {scancode = 0x37; status &= 0b11111101;} else scancode = 0x44; break;  //Numpad 9, Page Up (ROLL DOWN)
    case 0x7E: scancode = 0x61; break;  //Scroll Lock (COPY) <-------------------
    case 0x83: scancode = 0x68; break;  //F7
    case 0xE0: status |= 0b00000010; break;  //Set extend flag
    case 0xE1: nextmap(); break;  //Pause Break (switch to next map)
    case 0xF0: status |= 0b00000001; break;  //Set keybreak
    default: scancode = 0xFF;  //Invalid scancode (do not output)
  }
  pc98send(scancode);
}

void convtoho() {  //Fast predictive converter for a certain game series, lots of phantom mappings (see if you can find an unintended feature)
  //Can't be debugged like other converters due to tight timings
  switch (scancode) {  //PS2 key (PC98 KEY, if different)
    case 0x02: if (ps2clk == 5) scancode = 0x2A; else scancode = 0xFE; break;  //X
    case 0x03: if (ps2clk == 3) scancode = 0x3B; else scancode = 0xFE; status &= 0b11111101; break;  //Left
    case 0x04: if (ps2clk == 3) scancode = 0x3C; else scancode = 0xFE; status &= 0b11111101; break; break;  //Right
    case 0x09: if (ps2clk == 5) scancode = 0x34; else scancode = 0xFE; break;  //Space
    case 0x0A: scancode = 0x29; status &= 0b11111101; break;  //Z, Enter, Numpad Enter
    //case 0x0A: if (ps2clk == 4) scancode = 0x29; else scancode = 0xFE; status &= 0b11111101; break;  //Z, Enter, Numpad Enter
    case 0x12: if (ps2clk == 6) {if (status & 0b00000010) {scancode = 0xFF; status &= 0b11111101;} else scancode = 0x70;} else scancode = 0xFE; break;  //LShift (SHIFT), dismiss fake shifts
    case 0x15: if (ps2clk == 8) scancode = 0x10; else scancode = 0xFE; break;  //Q
    case 0x19: scancode = 0x70; break;  //RShift (SHIFT)
    case 0x32: scancode = 0x3D; status &= 0b11111101; break;  //Down, will stick if you press B, press and release down to fix, or use the line below (theoretically slower)
    //case 0x32: if (ps2clk == 6) scancode = 0x3D; else scancode = 0xFE; status &= 0b11111101; break;  //Down
    case 0x35: scancode = 0x3A; status &= 0b11111101; break;  //Up, will stick if you press Y, press and release down to fix, or use the line below (theoretically slower)
    //case 0x32: if (ps2clk == 6) scancode = 0x3D; else scancode = 0xFE; status &= 0b11111101; break;  //Down
    case 0x76: if (ps2clk == 8) scancode = 0x00; else scancode = 0xFE; break;  //Esc
    //case 0x77: locktgl(1); break;  //Num Lock, indicators will be broken, see locktgl
    //PRO TIP: press Num Lock odd number of times (once) after switching the map to reduce the amount of ignored scancodes (1 vs 4) generated when arrow keys are pressed with shift down.
    //         This, however, produces more scancodes for unshifted presses (3 vs 1), so not suitable for Reiiden, Fuumaroku, Yumejikuu... Depends on the LED status in normal operation.
    case 0xE0: status |= 0b00000010; break;  //Set extend flag
    case 0xE1: nextmap(); break;  //Pause Break (switch to next map)
    case 0xF0: status |= 0b00000001; break;  //Set keybreak
    default: if (ps2clk == 8) scancode = 0xFF; else scancode = 0xFE;  //Invalid scancode (do not output)
  }
  pc98send(scancode);
}

void convyume() {  //Comfortable layout for multiplayer in Yumejikuu, doesn't parse extend scancodes at all
  #ifdef usbser  //USB debugging
  usbser.print(scancode, HEX);
  usbser.print(" ");
  #endif
  switch (scancode) {                   //PS2 key -------------- (In-game function)
    case 0x14: scancode = 0x3C; break;  //LCtrl, RCtrl --------- (P2 Bomb)
    case 0x15: scancode = 0x10; break;  //Q -------------------- (Quit)
    case 0x1A: scancode = 0x29; break;  //Z -------------------- (P1 Shot)
    case 0x22: scancode = 0x2A; break;  //X -------------------- (P1 Bomb)
    case 0x2F: scancode = 0x3B; break;  //Menu ----------------- (P2 Shot)
    case 0x33: scancode = 0x20; break;  //H -------------------- (P1 Left)
    case 0x3B: scancode = 0x2D; break;  //J -------------------- (P1 Down)
    case 0x3C: scancode = 0x14; break;  //U -------------------- (P1 Up)
    case 0x42: scancode = 0x22; break;  //K -------------------- (P1 Right)
    case 0x69: scancode = 0x46; break;  //Numpad 1, End -------- (P2 Left)
    case 0x72: scancode = 0x4B; break;  //Numpad 2, Down ------- (P2 Down)
    case 0x73: scancode = 0x43; break;  //Numpad 5 ------------- (P2 Up)
    case 0x76: scancode = 0x00; break;  //Esc ------------------ (Pause)
    //case 0x77: locktgl(1); break;  //Num Lock, indicators will be broken, see locktgl
    case 0x7A: scancode = 0x48; break;  //Numpad 3, Page Down -- (P2 Right)
    case 0xE1: nextmap(); break;  //Pause Break (switch to next map)
    case 0xF0: status |= 0b00000001; break;  //Set keybreak
    default: scancode = 0xFF;  //Invalid scancode (do not output)
  }
  pc98send(scancode);
}

void convymsp() {  //Play against yourself in Yumejikuu, if you dare
  #ifdef usbser  //USB debugging
  usbser.print(scancode, HEX);
  usbser.print(" ");
  #endif
  switch (scancode) {                   //PS2 key (In-game function)
    case 0x12: if (status & 0b00000010) {scancode = 0xFF; status &= 0b11111101;} else scancode = 0x2A; break;  //LShift (P1 Bomb), dismiss fake shifts
    case 0x15: scancode = 0x10; break;  //Q (Quit)
    case 0x1B: scancode = 0x2D; break;  //S (P1 Down)
    case 0x1C: scancode = 0x20; break;  //A (P1 Left)
    case 0x1D: scancode = 0x14; break;  //W (P1 Up)
    case 0x23: scancode = 0x22; break;  //D (P1 Right)
    case 0x29: scancode = 0x29; break;  //Space (P1 Shot)
    case 0x5A: scancode = 0x3C; status &= 0b11111101; break;  //Enter, Numpad Enter (P2 Bomb)
    case 0x6B: scancode = 0x46; status &= 0b11111101; break;  //Numpad 4 (P2 Left)
    case 0x70: scancode = 0x3B; status &= 0b11111101; break;  //Numpad 0 (P2 Shot)
    case 0x73: scancode = 0x4B; break;  //Numpad 5 (P2 Down)
    case 0x74: if (status & 0b00000010) {scancode = 0x3B; status &= 0b11111101;} else scancode = 0x48; break;  //Numpad 6 (P2 Right), Left (P2 Shot)
    case 0x75: scancode = 0x43; status &= 0b11111101; break;  //Numpad 8 (P2 Up)
    case 0x76: scancode = 0x00; break;  //Esc (Pause)
    //case 0x77: locktgl(1); break;  //Num Lock, indicators will be broken, see locktgl
    case 0xE0: status |= 0b00000010; break;  //Set extend flag
    case 0xE1: nextmap(); break;  //Pause Break (switch to next map)
    case 0xF0: status |= 0b00000001; break;  //Set keybreak
    default: scancode = 0xFF;  //Invalid scancode (do not output)
  }
  pc98send(scancode);
}

void ps2rx() {  //PS/2 receive interrupt
  static uint32_t lasttime = 0;  //millis() value (time elapsed since MCU startup) during last interrupt
  uint32_t currtime = 0;  //Current millis() value
  ps2data |= (digitalRead(DATA) << ps2clk);  //Everything past ps2clk == 7 gets shifted away
  if (ps2clk == 7) {  //Got something, hurry, no time for the rest of the interrupt
    scancode = ps2data;  //Pass buffer for output
    ps2clk++;  //Next bit
  } else {
    currtime = millis();  //Start bit or check if the transmission was interrupted
    if (ps2clk > 9 || currtime - lasttime > 2) {  //1 ms sometimes produces errors, 2 ms is fine
      ps2clk = 0;  //Next bit is the first data bit
      ps2data = 0;  //Clear buffer
    } else ps2clk++;  //Next bit
    lasttime = currtime;  //Remember current millis()
  }
}

void ps2rxfast() {  //PS/2 receive interrupt for predictive conversion
  static uint32_t lasttime = 0;  //millis() value (time elapsed since MCU startup) during last interrupt
  uint32_t currtime = 0;  //Current millis() value
  ps2data |= (digitalRead(DATA) << ps2clk);  //Everything past ps2clk == 7 gets shifted away
  scancode = ps2data;  //Pass current buffer outside every bit
  currtime = millis();  //Start bit or check if the transmission was interrupted
  if (ps2clk > 9 || currtime - lasttime > 2) {  //1 ms sometimes produces errors, 2 ms is fine
    ps2clk = 0;  //Next bit is the first data bit
    ps2data = 0;  //Clear buffer
  } else ps2clk++;  //Next bit
  lasttime = currtime;  //Remember current millis()
}

void ps2tx() {  //PS/2 transmit interrupt
  static bool parity;  //Parity bit
  bool outgoing = 0;  //Data bit buffer
  switch (ps2clk) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      outgoing = (ps2data >> ps2clk) & 0b00000001;  //Get bit from byte
      digitalWrite(DATA, outgoing);  //Output data bit
      parity ^= outgoing;  //Calculate parity
      break;
    case 8: digitalWrite(DATA, parity); break;  //Output parity bit
    case 9: pinMode(DATA, INPUT_PULLUP); break;  //Set pin mode to input, outputting the stop bit as a result of pull-up
  }
  if (ps2clk > 10) {
    ps2clk = 0;  //Next bit is the first data bit
    parity = 1;  //Odd parity
  } else ps2clk++;  //Next bit
}
