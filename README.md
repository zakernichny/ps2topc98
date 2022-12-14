# *PS/2 (AT) to PC-9800 Series Keyboard Converter*

![convboard](https://user-images.githubusercontent.com/32784787/202309940-71aa7d5c-1b22-4544-8487-274933973721.png)

A protocol converter that allows to use abundant PS/2 and AT keyboards with NEC PC-9800 series of personal computers. Runs on 5V AVR Arduino boards. Full operation is confirmed for Arduino Nano and Pro Micro.

## Hardware

Connector pinouts for the default code configuration are shown below. Labels inside the pin symbols correspond to pins on an Arduino. TX is the transmit pin of the board's hardware serial interface. VCC (+5VDC) is provided by the host PC-98 machine and used to power both the converter and the keyboard. ***Make sure the code is compatible with your hardware before programming***, defines for some common configurations are included.

### PS/2 Female Mini-DIN-6 Connector, looking at the sockets

![ps2conn](https://user-images.githubusercontent.com/32784787/202313107-ca38b250-b138-4b20-9112-8ef252b5cdd0.png)

Cheap chinese DIY female Mini-DIN connectors should be avoided, at least in my experience, due to poor contact (contacts don't seem to reach far enough into the plastic). Find a factory-made cable to sacrifice, you can get cheap PS/2 splitters.

### PC-98 Male Mini-DIN-8 Connector, looking at the pins

![pc98conn](https://user-images.githubusercontent.com/32784787/203018261-7aaf2aed-6e23-4cac-aa4c-19f40bf7b23e.png)

Pin 4 on some cheap connectors might need to be bent slightly to fit into the socket if it's right in the center when it should be offset.

### PC-9801 Male DIN-8 Connector, looking at the pins

![pc9801conn](https://user-images.githubusercontent.com/32784787/203018311-a8b89311-845f-401f-8fa0-c33f760efbc6.png)

## Usage

The converter provides different key maps for a variety of use cases. Press Pause Break to switch to the next map. Second map is unique as it can use Scan Code Set 3, in which case switching from it will require pressing Scroll Lock instead, due to conflicting scan codes. Existing maps can be easily customized and new maps for specific applications can be easily added, up to 8x2 maps, see the sources for more details. Included key maps and their features are provided in a table below.

Map ID | Scan Code Set | Name | Indication (LED flashes) | Description | Switch Key | Next Map
:---: | :---: | --- | --- | --- | :---: | ---
0 | 2 | Full 101/102-key Converter | 1 long, all LEDs (Reset) | General purpose converter utilizing all the keys on a standard keyboard. Slow but sure, still doesn't wait for the parity bit, so somewhat fast. Has some basic macro functionality on Alt + Print Screen, see the code. Upon switching to the next map, if the keyboard successfully swithes to Scan Code Set 3, map ID 1 will use it's Set 3 version, otherwise Set 2 version will be used. Hold down Ctrl before switching to override into Set 2 version of ID 1. | Pause Break, Ctrl + Pause Break | ID 1 (Set 2 or 3)
1 | 2 | Fast Converter for Games | 2 short, all LEDs | My first attempt at making a predictive converter. Some keys use 3-bit predictive conversion, which in theory allows for conversion delays as low as 380 ??s (from the beginning of keyboard transmission to the beginning of converter transmission), however, the performance is significantly degraded due to additional extend and fake shift scan codes, which delay everything for milliseconds. Has some interesting unintended features though. | Pause Break | ID 2
1 | 3 | Faster Set 3 Converter for Games | 2 short, all LEDs + 1 long, Scroll Lock | Scan Code Set 3 has a benefit of only outputting a single-byte make scan code and two-byte break scan code for any key. Combined with uniform 5-bit predictive conversion, this map provides conversion delays for most of the common keys in games as low as 420 ??s! Using this map where Set 3 is available is strongly recommended over the Set 2 version. If switching to the next map fails with a cool LED light show a bit too often on your particular hardware, try adjusting the delay in the codeset funcion. | Scroll Lock | ID 2
2 | 2 | ??????????????? Multiplayer Layout | 3 short, all LEDs | A subjectively more comfortable layout for multiplayer in Phantasmagoria of Dim.Dream. | Pause Break | ID 3
3 | 2 | ??????????????? Solo Layout | 1 long and 1 short, all LEDs | Use both hands to play against yourself in Phantasmagoria of Dim.Dream. I can't seem to be able to process both sides of the screen at the same time, how about you? | Pause Break | ID 0

### Default layout for ANSI keyboards

![layout](https://user-images.githubusercontent.com/32784787/203142249-dfa41f14-f1df-4e7c-91e9-6877c2cefb79.png)

JSON file for [Keyboard Layout Editor](http://www.keyboard-layout-editor.com/) is provided [here](layout.json). Green and orange legends represent ID 2 and ID 3 mappings respectively.
