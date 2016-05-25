# nRF24MSR (nRF24L01+ MultiSendReceive Library)
version 0.0.1a

This library is written for the nRF24L01+ modules overcoming the two main limitations in these modules:
 - Limited address range (255 maximum)
 - limited payload size (32 bytes maximum).

The nRF24L01 modules are very cheap (e.g. compared to Bluetooth or ZigBee Modules for Arduino) and so we can use these cheap modules for greater payloads.
The whole library is based on RF24 from TMRh20 (see Installation).
This library implements a protocol providing 5 bytes long adress (nRF_address) and a maximum payload size (maxPayloadLength) which can be configured (depend on how much RAM you can provide for the receive buffer).
The default value for the maximum payload is set to 57 bytes.

## Example Sketch.
ATTENTION: install the RF24 library first, it will not compile (cannot find RF24.h) without it.
An example Sketch Send_Receive.ino is provided for the Arduino IDE.
To change the roles switch the commend of lines 16 and 17:
commend out one:
 role_e role = role_ping_out;
 or 
 role_e role = role_pong_back;)
You need one sender (role_ping_out) and one receiver (role_pong_back).
The default Pins for the nRF24L01+ modules are Pins 7 and 8 (like in the default PINS of the RF24 library). These Pins can be changes in the nRF.h file.

## Installation:

Install THMRh20/RF24 library:

	cd ~/Arduino/libraries
	git clone https://github.com/TMRh20/RF24.git

For using the library with ESP8266 Arduino IDE use exactly following configurations:
At least python 2.7.6 installed (maybe another python version of 2.7.X works too. Not tested)

TMRh20 (https://github.com/TMRh20/RF24) (see Known Errors)

ESP8266 (https://github.com/esp8266/Arduino) commit hash: 33723a9b52a40ed10cdced4507dde63b42fa38ed

Arduino IDE v. 1.6.7 (1.6.8 it is not working!)

install ESP8266 support using git version:
(from: https://github.com/esp8266/Arduino)

	// cd to extracted arduino-1.6.7
	cd hardware
	mkdir esp8266com
	cd esp8266com
	git clone https://github.com/esp8266/Arduino.git esp826

	cd esp8266/tools
	python get.py

	// restart arduino ide if open

##Kown Errors
Compilation error for ESP8266:
~Arduino/libraries/RF24/RF24_config.h:133:27: fatal error: avr/pgmspace.h: No such file or directory
  #include <avr/pgmspace.h>
Solution:
cd ~/Arduino/libraries/RF24/
open RF24_config.h
	go to line 133
	change line 133 from: 	#include \<avr/pgmspace.h\> 
	to:#ifdef ESP8266 #include \<pgmspace.h\> #else #include \<avr/pgmspace.h\> #endif
	save file.
This uses pgmspace.h for ESP8266 compilatins.
	
## Implementations and Configurations Details:

The first limitation of the nRF24L01+ modules exist because the adressing is made by so called pipes. The pipes are 5 bytes long but only the last byte between the pipes can be different. This means we can only adress 255 devices.
The second limitations exist because the modules' maximum payload length is 32 bytes. When we want so send more then 32 bytes we need an additional protocol like this.

This library implements a protocol providing 5 bytes long adress (nRF_address) and a maximum payload size (maxPayloadLength) which can be configured (depend on how much RAM you can provide), per default the maximum payload size is set to 57 bytes.

nRF_address (5 bytes):
| bytes 0 | bytes 1 | bytes 2 | bytes 3 | bytes 4 |

As a withdraw we communicate over a single pipes (used_pipes) and our protocol header (nRF_message_header) consumes 13 byte (length(1)+type(1)+destination(5)+source(5)+streamLength(1)|streamFlags(1)) leaving 19 byte (32 - 13) payload for a single packet. The protocol needs to establish something like a connection for payloads greater then 19 bytes.

Example for single packet with header in bytes 0-12 (13 bytes) and in bytes 13-31 (19 bytes) payload:<br \>
| byte 0 | byte 1 | bytes 2 - 6		    | bytes 7 - 11	 | byte 12	| byte 13-31	| <br \>
| length | type   | nRF_address destination | nRF_address source | streamLength | payloa	| <br \>

A connection is established by a so called StreamRequest packet providing no payload but the count (streamLength) of packets in the stream.
The StreamRequest packet is acknowledged by a StreamAck packet with a streamFlags instead of a streamLength, accepting the stream (the StreamFlag_streamAccepted is set) if he can received it or denied it (StreamFlag_streamAccepted=StreamFlag_streamDenied is not set) if the streamLength does not fit in the receive buffer.
When the StreamRequest is accepted, the sender sends StreamData packets with a streamPacketCounter instead of the streamLength (the streamPacketCounter goes from streamLength-1 to 0, to order the StreamData packets). When the StreamData packet with the streamPacketCounter of 0 is received and every packet between in the right order is received by the receiver the whole stream is acknowledged by a StreamAck packet with the StreamFlag_streamComplete flag set. If not every packet is received or a timout occurs (TODO timout detection here is not implemented) the receiver sends a StreamAck packet without the StreamFlag_streamComplet=StreamFlag_streamCorrupted flag set. No automatic resend is implemented.

For a detail look at the messages and flags see file nRF24Messages.h.
For a change of the configuration like: Pins where the nRF24L01+ module is connected, maximum payload length and used pipes take a look at file nRF.h.
For enabling debug outputs and profiling take a look at the top of file nRF.h, but be carefull they consume a lot of RAM.
For configurating sleep durations and retries take a look at the top of file nRF.h, too.
For implementation details see file nRF.cpp.


