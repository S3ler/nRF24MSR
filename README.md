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
You have to notice that we do not use dynamic payload sizes our message are always 32 byte long. The real length is shown in the length field (byte 0).

nRF_address (5 bytes):

     | byte 0 | byte 1 | byte 2 | byte 3 | byte 4 |

As a withdraw we communicate over a single pipes (used_pipes) and our protocol header (nRF_message_header) consumes 13 byte (length(1)+type(1)+destination(5)+source(5)+streamLength(1)|streamFlags(1)) leaving 19 byte (32 - 13) payload for a single packet. The protocol needs to establish something like a connection for payloads greater then 19 bytes.

Example for single nRF_stream_data packet with header in bytes 0-12 (13 bytes) and in bytes 13-31 (19 bytes) payload:

     |byte 0 | byte 1 | bytes 2 - 6             | bytes 7 - 11       | byte 12	    | byte 13-31 |
     |length | type   | nRF_address destination | nRF_address source | streamLength | payload    |
     
Example for a nRF_stream_ack packet:

     |byte 0 | byte 1 | bytes 2 - 6             | bytes 7 - 11       | byte 12	   | 
     |length | type   | nRF_address destination | nRF_address source | streamFlags | 

A connection is established by a so called StreamRequest packet providing no payload but the count (streamLength) of packets in the stream.
The StreamRequest packet is acknowledged by a StreamAck packet with a streamFlags instead of a streamLength, accepting the stream (the StreamFlag_streamAccepted is set) if he can received it or denied it (StreamFlag_streamAccepted=StreamFlag_streamDenied is not set) if the streamLength does not fit in the receive buffer.
When the StreamRequest is accepted, the sender sends StreamData packets with a streamPacketCounter instead of the streamLength (the streamPacketCounter goes from streamLength-1 to 0, to order the StreamData packets). When the StreamData packet with the streamPacketCounter of 0 is received and every packet between in the right order is received by the receiver the whole stream is acknowledged by a StreamAck packet with the StreamFlag_streamComplete flag set. If not every packet is received or a timout occurs (TODO timout detection here is not implemented) the receiver sends a StreamAck packet without the StreamFlag_streamComplet=StreamFlag_streamCorrupted flag set. No automatic resend is implemented.

For payload length < 20 bytes:

                 Sender                             Receiver
      [set address] |                                   |
                    |                                   |
         write() -->| --- nRF_stream_request_data ----> | 
               [OK] | <------ nRF_stream_ack  --------- | 
                    |                                   | [exec receiveForMeCallback]
                    |                                   |


For payload length >= 20 bytes:

                 Sender                             Receiver
      [set address] |                                   |
                    |                                   |
         write() -->| ------- nRF_stream_request -----> | (connection requested for n data packets)
                    | <------ nRF_stream_ack  --------- | (connection established)
                    |                                   |
                    | --- nRF_stream_request_data ----> | ( 0-th data packet send)
                    | <------ nRF_stream_ack  --------- | ( 0-th data packet acknowledged)
                    //                                  //
                    | --- nRF_stream_request_data ----> | ( (n-1)-th data packet send)
               [OK] | <------ nRF_stream_ack  --------- | ( (n-1)-th data packet acknowledged)
                    |                                   | [exec receiveForMeCallback]
                    |                                   |


For broadcasts (only for payload length < 20 bytes):

                 Sender                             Receiver
                    |                                   |
         write() -->| --- nRF_stream_request_data ----> | 
               [OK] |                                   | [exec receiveBroadcastCallback]
                    |                                   |


For a detail look at the messages and flags see file nRF24Messages.h.
For a change of the configuration like: Pins where the nRF24L01+ module is connected, maximum payload length and used pipes take a look at file nRF.h.
For enabling debug outputs and profiling take a look at the top of file nRF.h, but be carefull they consume a lot of RAM.
For configurating sleep durations and retries take a look at the top of file nRF.h, too.
For implementation details see file nRF.cpp.

### Idea for a lite protocol
Now we go down and think about each bit, to shrink down the header content.
THe reasons are:
The nRF24L01+ modules have a range which is depend on: the length of the send bytes and the Mbps.
Best range values are seen with 8 bytes maximum sending.

First we change the nRF_address to 1-n bytes where n is between 1 and 5.
So we save here a lot of bytes.
For not altering the API we can add leading zeros to unused bytes.
E.g. a nRF_adddress with 2 bytes: 0x25, 0x22 becomes in the receiveForMeCallback method a nRF_address with 5 bytes with leading zeros: 0x00, 0x00, 0x00, 0x25, 0x22. Of course as always: The builder has to take care that 2 addresses are not given twice.

nRF_address (1-n bytes):

     | byte 0 | ... | byte n | 

Second we change the nRF_stream_data packet.
We need only 3 different data types, and then we only need 2 bits (theoretically).
But to stay outside of the MQTT-SN message type ranges 0x00 to 0x1D 00011101 and 0xFE (0b1111 1110) we need to change bit-combination which do not use the MQTT-SN message type bitcombinations.

  StreamRequest           = 0x80 // 0b1000 0000
  StreamAck               = 0x90 // 0b1001 0000
  StreamData              = 0xA0 // 0b1010 0000
  reserved		  = 0xB0, 0xC0, 0xD0, 0xF0 (here is a reserved bit here with a 1: 0b0100 0000
  by this way 0xFE (0b1111 1110) is still available for Encapsulated message and we only need 4 bits for message types.

Furthermore we use the lower 4 bits for the streamFlags and streamLength.
For the streamFlags are 4 bytes enough, so we only move them from the end of the header into the lower 4 bits in the type field (compared to the prior protocol).
For the streamLength this means we can only have a maximum of 15 streamPackets. The question is now if this is enough, if we assume we have a the minimum values: 1 bytes length, 1 byte type+streamLength/Flags, 1 byte destination, 1 byte source meaning a 4 byte header, and with a maximum bytes to send of 8 bytes, we have 4 byte payload. So we have 4*15 == 60 bytes payload maximum. I assume this i not enough. Theoretically we can get some bit from the length byte. For a maximum bytes to send of 32 bytes we only need 6 bits ( 32 == 0b00100000).
Further Ideas:
Of course now we can do tricks like only telling the payload length then we need only 32-4 (we assume 4 byte header) = 28 ( == 0b00011100 ) 5 bits. Doing a different tricks like assuming that we know when we receive a message, we can use the length of 31 as 32 and 0 is 1 (length in packet to reald length in bytes is length+1) and showing still the whole byte cound. Last but not least, less then 5 bits for the length field is not possible.
Taking these 3 bits and add them to the the streamLength field we have a maximum of 127 streamPackets and 4*127 == 508 bytes.
This is a lot more. We still have a reserved bit in the message type (the 6th bit).

So i suggest to rearrange to use only the first 3 bits (first bit for the non overlapping with MQTT message types and 2 bits for the protocol message types) the message types like following:
  StreamRequest           = 0xE0 // 0b1000 0000
  StreamAck               = 0xA0 // 0b1010 0000
  StreamData              = 0xC0 // 0b1100 0000
This ensures that in the higher 3 bits for the message type (1 bit for non-overlapping with mqtt message types and 2 bytes for protocol message type but without 0b011) that we never have  0xFE (0b1111 1110) == Encapsulated message as protocol combination).
Now we have 5 bits for protocol stuff left or we use it for the streamLength like thought about above.
Leaving aside no/unicast available (never used yet) we need only 3 bits in the prior protocol, namely dataComplete(Corrupted, streamComplete/Corrupted, streamAccepted/Denied. These flags appear in a certain context with the message types. 
We can shrink this down by using the message type as context:
In a StreamData message we do not need any flags, in a StreamRequest message we dont need any flags, too.
Only in a StreamAck message we need flags and fortunately we do not need any streamLength information here.
So we can use the lower 5 bits in StreamAck packets for Flags, and in StreamRequest/StreamData for the StreamLength.
In a StreamAck message the 0b0001 0000 bit is always 0.
In StreamRequest/StreamData the 0b0001 0000 bit is used for the StreamLength, additionally the 0b0000 1111 bits, and then we can think about the bits out of the length field. we have there 3 bits left (using all compression stuff) so we have a total of 3+5 = 8 bits for the streamlength, providing a maximum streamLength of 255 packets and 4*255 == 1020 bytes maximum payload. This should be enough space for MQTT-SN messages and Topic Names and Messages.
By using fixed payload sizes, we can use the RF24-libraries functionality and can match depend on the length of received payload in the module the different protocols: 32 byte are the normal protocol and everything below is the lite protocol (especially 8 byte).

Example for single nRF_comprehensive_stream_packet:

     | byte 0			  | byte 1 				       | bytes 2     | bytes 3 	  | bytes 4 - 7	| 
     | bit 0-2 		 |bit 3-8 | bit 0    | bytes 1 - 2  | bytes 3 - 8      | byte 0 - 8  | byte 0 - 8 | byte 0 - 8 	|
     | msb_streamLengt	 | length | always 1 | message_type | lsb_streamLength | destination | source     | payload	|
     
// TODO: add a section how these two protocols are not overlapping and how we can use them both with prior code and what has to be exchanged/modified
