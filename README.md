# PS/2 (AT) to PC-9800 Series Keyboard Converter

![convboard](https://user-images.githubusercontent.com/32784787/202309940-71aa7d5c-1b22-4544-8487-274933973721.png)

Protocol converter that allows to use abundant PS/2 and AT keyboards with NEC PC-9800 series of personal computers. Runs on SparkFun Pro Micro 5V/16MHz Arduino boards or equivalents, can be easily adopted for any ATmega32u4-based boards.

## Hardware

Connector pinouts for the default code configuration are provided below. Numbers inside the pin symbols correspond to pin numbers on a Pro Micro. VCC (+5VDC) is provided by the host PC-98 machine and used to power both the Pro Micro and the keyboard.

### PS/2 Female Mini-DIN-6 Connector, looking at the sockets

![ps2conn](https://user-images.githubusercontent.com/32784787/202313107-ca38b250-b138-4b20-9112-8ef252b5cdd0.png)

Cheap chinese DIY female Mini-DIN connectors should be avoided, at least in my experience. Find a factory-made cable to sacrifice, you can get cheap PS/2 splitters.

### PC-98 Male Mini-DIN-8 Connector, looking at the pins

![pc98conn](https://user-images.githubusercontent.com/32784787/202318302-e30bac6f-4704-455f-a904-82fc18dd5afa.png)

Pin 4 on some cheap connectors might need to be bent slightly to fit into the socket if it's right in the center when it should be offset.

## Usage

The converter provides 5 different key maps for a variety of use cases. It starts in full conversion mode. Pause Break or, sometimes, Scroll Lock is used to switch 
