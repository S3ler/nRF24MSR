/* 
 *  nRF24MSG Library - enhancing nRF24L01+ module by increasing payload sizes, adding 5 byte addresses-
 *  Copyright (C) 2016  Gabriel Nikol
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/


#ifndef MQTT_SNRF24L01P_NRF24MESSAGES_H
#define MQTT_SNRF24L01P_NRF24MESSAGES_H

#include <stdint.h>


#define packetLength                    32
#define streamDataLength                19
#define streamHeaderLength              13

#define StreamFlag_reserved             0b11110000
#define StreamFlag_dataComplete         0b00001000
#define StreamFlag_dataCorrupted        0b00000000
#define StreamFlag_noUnicastAvailable   0b00000100
#define StreamFlag_unicastAvailable     0b00000000
#define StreamFlag_streamComplete       0b00000010
#define StreamFlag_streamCorrupted      0b00000000
#define StreamFlag_streamAccepted       0b00000001
#define StreamFlag_streamDenied         0b00000000

enum nRF_message_type {
  StreamRequest           = 0xb3,
  StreamAck               = 0xb4,
  StreamData              = 0xb5,
};

#ifndef __MQTTSN_MESSAGES_H__
struct message_header {
  uint8_t length;
  uint8_t type;
};
#endif

struct nRF_address {
  uint8_t bytes[5];  // mac
};

struct nRF_message_header : public message_header {
  nRF_address destination;
  nRF_address source;
};

struct nRF_stream_request : public nRF_message_header {
  uint8_t streamLength;
};

struct nRF_stream_request_data : public nRF_message_header {
  uint8_t streamLength;
  uint8_t payload0; uint8_t payload1; uint8_t payload2; uint8_t payload3; uint8_t payload4;
  uint8_t payload5; uint8_t payload6; uint8_t payload7; uint8_t payload8; uint8_t payload9;
  uint8_t payload10; uint8_t payload11; uint8_t payload12; uint8_t payload13; uint8_t payload14;
  uint8_t payload15; uint8_t payload16; uint8_t payload17; uint8_t payload18;
};

struct nRF_stream_request_data_rec : public nRF_message_header {
  uint8_t streamLength;
  uint8_t payload[19];
};


struct nRF_stream_ack : public nRF_message_header {
  uint8_t streamFlags;
};

struct nRF_stream_data : public nRF_message_header {
  uint8_t streamPacketCounter;
  uint8_t payload0; uint8_t payload1; uint8_t payload2; uint8_t payload3; uint8_t payload4;
  uint8_t payload5; uint8_t payload6; uint8_t payload7; uint8_t payload8; uint8_t payload9;
  uint8_t payload10; uint8_t payload11; uint8_t payload12; uint8_t payload13; uint8_t payload14;
  uint8_t payload15; uint8_t payload16; uint8_t payload17; uint8_t payload18;
};

struct nRF_stream_data_rec : public nRF_message_header {
  uint8_t streamPacketCounter;
  uint8_t payload[19];
};


#endif //MQTT_SNRF24L01P_NRF24MESSAGES_H
