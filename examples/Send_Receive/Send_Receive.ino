/* 
 * <one line to give the program's name and a brief idea of what it does.>
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


#include <SPI.h>
#include "nRF.h"


nRF24L01p radio;


const uint8_t _mac[] = { 0xAB, 0xAB, 0xAB, 0xAB, 0xAB}; // sendermac
const uint8_t _connected[] = { 0xBA, 0xBA, 0xBA, 0xBA, 0xBA}; // receivermac

const uint8_t _broadcast[] = { 0x01, 0x01, 0x01, 0x01, 0x01}; // broadcastmac

typedef enum { role_ping_out = 1, role_pong_back } role_e;                 // The various roles supported by this sketch
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};  // The debug-friendly names of those roles

// role_e role = role_pong_back;                                              // The role of the current running sketch
 role_e role = role_ping_out;



void setup() {
  optionalSetup();
  radio.begin();
  if (role == role_ping_out) {
    // myMac = tonRFAddress(&_mac);
    radio.setMyMac(radio.tonRFAddress(&_mac));
    radio.setDestinationMac(radio.tonRFAddress(&_connected));
  } else {
    radio.setMyMac(radio.tonRFAddress(&_connected));
    radio.setDestinationMac(radio.tonRFAddress(&_mac));
  }
  radio.setBroadcastMac(radio.tonRFAddress(&_broadcast));
}

void loop() {
  pingOut();
  pongOut();
}

void optionalSetup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  Serial.println("\n\rnRF/examples/MSRLibrary_ESP");
}

void setUpRadio() {
  radio.begin();
}

void pingOut() {
  if (role == role_ping_out) {

    radio.logStart(4);
    uint8_t smalltime[15];
    for (uint8_t i = 0 ; i < sizeof(smalltime); i++) {
      smalltime[i] = i;
    }

    Serial.println("Sending small payload as broadcast: ");
    for (uint8_t i = 0 ; i < sizeof(smalltime); i++) {
      Serial.print( smalltime[i], HEX);  Serial.print(" ");
    }
    Serial.println();
    if (!radio.writeBroadcast(&smalltime, sizeof(smalltime))) {
      Serial.println("failed");
    } else {
      Serial.println("worked");
    }
    radio.logEnd("Packet completely send", 4);

    Serial.println();
    Serial.println();
    delay(1000);

    radio.logStart(4);
    Serial.println("Sending small payload: ");
    for (uint8_t i = 0 ; i < sizeof(smalltime); i++) {
      Serial.print( smalltime[i], HEX);  Serial.print(" ");
    }
    Serial.println();
    if (!radio.write(&smalltime, sizeof(smalltime))) {
      Serial.println("failed");
    } else {
      Serial.println("worked");
    }
    radio.logEnd("Packet completely send", 4);

    Serial.println();
    Serial.println();
    delay(1000);

    radio.logStart(4);
    uint8_t counting[255];
    for (uint8_t i = 0 ; i < sizeof(counting); i++) {
      counting[i] = i;
    }
    Serial.println("Sending big packet: ");
    for (uint8_t i = 0 ; i < sizeof(counting); i++) {
      Serial.print( counting[i], HEX);  Serial.print(" ");
    }
    Serial.println();
    if (!radio.write(&counting, sizeof(counting))) {
      Serial.println("failed");
    } else {
      Serial.println("worked");
    }
    radio.logEnd("Packet completely send", 4);


    Serial.println();
    Serial.println();
    delay(5000);
  }
}

void pongOut() {
  if ( role == role_pong_back ) {
    for (uint8_t i = 0 ; i < 5 ; i++) {
      radio.logStart(4);
      uint32_t timeout = 10000;
      Serial.print("starting receiveLoop with timout "); Serial.print(timeout); Serial.println();
      radio.receiveLoop(timeout);
       radio.logEnd("timout receiveLoop ", 4);
    }
    radio.receiveLoop(0);
  }
}



