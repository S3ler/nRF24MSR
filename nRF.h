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


#ifndef MQTT_SNRF24L01P_NRF24L01P_H
#define MQTT_SNRF24L01P_NRF24L01P_H

#include "RF24.h"
#include "nRF24Messages.h"
#include "nRF_config.h"

#define OK    true;
#define Error false;

class nRF24L01p {
  private:
    RF24 _rf24;
    const uint64_t _pipes[2]; // TODO remove member

    nRF_address myMac;
    nRF_address destinationMac;
    nRF_address broadcastMac;

    uint16_t payloadSize;
    uint8_t payloadBuffer[maxPayloadLength];
    volatile boolean breakReceiveLoop = false;
  public:


    nRF24L01p();
    void begin();
    bool setMyMac(nRF_address mac);
    bool setDestinationMac(nRF_address destination);
    bool resetDestinationMac();
    bool setBroadcastMac(nRF_address destination);

    void stopReceiveLoop();
    bool receiveLoop(uint32_t timout);
    void receiveForMeCallback(nRF_address source, const void* buf, uint16_t len );
    void receiveBroadcastCallback(nRF_address source, const void* buf, uint16_t len );

    bool write( const void* buf, uint8_t len );
    bool writeBroadcast( const void* buf, uint8_t len );

    nRF_address tonRFAddress(const void* mac) ;

  private:
    // building packets
    nRF_stream_request buildSingleRequestPacket(uint8_t streamLength);
    nRF_stream_request_data buildSingleRequestDataPacket(const void* buf, uint8_t len, uint8_t streamLength);
    nRF_stream_ack buildStreamAckPacket(nRF_address address, uint8_t streamFlags);
    nRF_stream_data createStreamDataPacket(const void* buf, uint8_t size, uint8_t  packetCount, uint8_t packetCounter);

    // sender - send
    bool sendAsStream(const void* buf, uint8_t len);
    bool sendAsBroadcast(const void* buf, uint8_t len);
    bool sendStreamRequestPacket(const void* buf, uint8_t len);
    bool sendStreamRequestDataPacket(const void* buf, uint8_t len);
    bool sendStreamDataPacket(const void* buf, uint8_t len) ;


    // sender - receive ack
    bool waitForStreamAckPacket(uint8_t streamFlags);
    // sender - helper
    uint8_t calculateStreamLength(uint8_t size);

    // receiver - send back
    bool writeAck(const void* buf, uint8_t len);

    // checking methods
    bool isStreamDataPacket(uint8_t type);
    bool isStreamAckPacket(uint8_t type);
    bool isStreamRequestPacket(uint8_t type);
    bool isFromDestination(nRF_address address);
    bool isForMe(nRF_address address);
    bool isBroadcast(nRF_address address);
    bool areEqual(nRF_address address1, nRF_address address2);

    // timout methods
    bool isTimeout(uint64_t startTimeout, uint64_t maxTimoutValue);

    // print packets
    void printStreamAckPacket(nRF_stream_ack packet) ;
    void printStreamDataPacket(nRF_stream_data_rec packet);
    void printStreamDataPacket(nRF_stream_data packet);
    void printStreamRequestPacket(nRF_stream_request packet);
    void printStreamRequestDataPacket(nRF_stream_request_data packet);
    void printStreamRequestDataPacket(nRF_stream_request_data_rec packet);

#if defined(_startup_profiling) || defined(_send_profiling) || defined(_receiving_profiling)
  public:
    volatile uint64_t start[5] = { 0, 0, 0, 0, 0 };
    void logStart( uint8_t timer);
    void logStop( uint8_t timer);
    void logEnd(const char* profile_message, uint8_t timer);
#endif


};


#endif //MQTT_SNRF24L01P_NRF24L01P_H
