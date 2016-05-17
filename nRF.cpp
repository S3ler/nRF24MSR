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


#include "nRF.h"

nRF24L01p::nRF24L01p() :
  payloadSize(0),
  _pipes( { used_pipes, used_pipes }),
  _rf24(RF24Pin1, RF24Pin2) {
  memset(&myMac, 0, sizeof( nRF_address ));
  memset(&destinationMac, 0, sizeof( nRF_address ));
}

void nRF24L01p::begin() {
#if _startup_profiling
  logStart(1);
#endif
  _rf24.begin();
  delay(switchingDelay);
  _rf24.setAutoAck(1);                    // Ensure autoACK is enabled
  _rf24.setRetries(15, 15);                // Smallest time between retries, max no. of retries
  _rf24.setPayloadSize(32);                // Here we are sending 1-byte payloads to test the call-response speed
  _rf24.openWritingPipe(_pipes[1]);        // Both radios listen on the same pipes by default, and switch when writing
  _rf24.openReadingPipe(1, _pipes[0]);
  _rf24.startListening();                 // Start listening
  delay(switchingDelay);
#if _startup_profiling
  logEnd("nRF24L01p::begin", 1);
#endif
}

bool nRF24L01p::setMyMac(nRF_address mac) {
  myMac = mac;
}

bool nRF24L01p::setDestinationMac(nRF_address destination) {
  destinationMac = destination;
}

bool nRF24L01p::setBroadcastMac(nRF_address broadcast) {
  broadcastMac = broadcast;
}

extern void MQTTSN_receive_for_me_callback(nRF_address source, const void* buf, uint16_t len ); // TODO move this later to method references

void nRF24L01p::receiveForMeCallback(nRF_address source,  const void* buf, uint16_t len ) {

#if enable_payload_printout
  Serial.println("=== Received For Me ===");
  const uint8_t* payload = reinterpret_cast<const uint8_t*>(buf);
  Serial.print("bytes: "); Serial.println(len);
  Serial.print("payload: ");
  for (uint8_t i = 0; i <  len; i++) {
    Serial.print(*payload++, HEX); Serial.print(" ");
  }
  Serial.println();
  Serial.println();
#endif

  MQTTSN_receive_for_me_callback(source, buf, len);

}

extern void MQTTSN_receive_broadcast_callback(nRF_address source, const void* buf, uint16_t len ); // TODO move this later to method references

void nRF24L01p::receiveBroadcastCallback(nRF_address source, const void* buf, uint16_t len ) {

#if enable_payload_printout
  Serial.println("=== Received Broadcast===");
  const uint8_t* payload = reinterpret_cast<const uint8_t*>(buf);
  Serial.print("From: ");
  for (uint8_t i = 0; i < sizeof(source); i++) {
    Serial.print(source.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("bytes: "); Serial.println(len);
  Serial.print("payload: ");
  for (uint8_t i = 0; i <  len; i++) {
    Serial.print(*payload++, HEX); Serial.print(" ");
  }
  Serial.println();
  Serial.println();
#endif

  MQTTSN_receive_broadcast_callback(source,  buf, len);

}


void nRF24L01p::stopReceiveLoop() {
  breakReceiveLoop = true;
}

bool nRF24L01p::receiveLoop(uint32_t timout) {
  uint32_t startTimer = millis();
  bool result = false;
  breakReceiveLoop = false;
  while (true) {
    yield();
    _rf24.startListening();

    byte pipeNo;
    while (_rf24.available(&pipeNo)) { // or if(?)
      if (pipeNo == 1) { // single stuff
#if _receiving_profiling
        logStart(0);
#endif
        yield();
        uint8_t buffer[32];
        _rf24.read( &buffer, sizeof(buffer) );
        message_header* header = (message_header*) buffer;
        if (isStreamRequestPacket(header->type)) {
#if _receiving_profiling
          logEnd("Identified StreamReq", 0);
          logStart(0);
#endif
          nRF_message_header* nRF_header = (nRF_message_header*) buffer;
          if (isForMe(nRF_header->destination)) {
            nRF_stream_request* first_nRF_stream_request_packet = (nRF_stream_request*) buffer;
#if enable_debug_output
            printStreamRequestPacket(*first_nRF_stream_request_packet);
#endif
#if _receiving_profiling
            logEnd("Identified StreamReq for Me", 0);
            logStart(0);
#endif
            if (first_nRF_stream_request_packet->streamLength * streamDataLength > maxPayloadLength) {
              nRF_stream_ack ack = buildStreamAckPacket(first_nRF_stream_request_packet->source, StreamFlag_streamDenied);
#if enable_debug_output
              printStreamAckPacket(ack);
#endif
              writeAck(&ack, sizeof(ack) );
#if _receiving_profiling
              logEnd("StreamReq -> StreamAck Denied", 0);
#endif
              yield();
            } else {
              nRF_stream_ack ack = buildStreamAckPacket(first_nRF_stream_request_packet->source, StreamFlag_streamAccepted);
#if enable_debug_output
              printStreamAckPacket(ack);
#endif
              delay(stream_request_ack_switching_delay);
              writeAck(&ack, sizeof(ack) );

#if _receiving_profiling
              logEnd("StreamReq -> StreamAck Accepted", 0);
#endif
              yield();
              bool noError = true;
              for (int16_t i = first_nRF_stream_request_packet->streamLength - 1 ; i >= 0 ; i--) { // count down for each packet in stream

#if enable_debug_output
                Serial.print("Waiting for StreamData "); Serial.println(i);
#endif
#if _receiving_profiling
                logStart(0);
#endif

		uint64_t start = millis();
                while (!_rf24.available(&pipeNo)) {
                  yield();
                  uint64_t end = millis();
                  uint64_t duration = 0;
                  if(end < start){
                    // millis overflow
                    duration = UINT64_MAX - start;
                    duration +=  end;
                  }else {
                    duration =  end - start;
                  }
		  if(duration > stream_data_timeout) {
                    // timout
                    noError = false;
                    break;
                  }
                }
		if(!noError){
		  break;
		}

#if _receiving_profiling
                logEnd("Waited for Data", 0);
#endif
                if (pipeNo == 1) {
#if _receiving_profiling
                  logStart(0);
#endif
                  _rf24.read( &buffer, sizeof(buffer) );
                  message_header* header = (message_header*) buffer;
                  if (isStreamDataPacket(header->type)) {
                    nRF_stream_data_rec* nRF_stream_data_packet = (nRF_stream_data_rec*) buffer;
#if enable_debug_output
                    printStreamDataPacket(*nRF_stream_data_packet);
#endif
#if _receiving_profiling
                    logEnd("Identified StreamData", 0);
                    logStart(0);
#endif
                    if (isForMe(nRF_header->destination) && areEqual(first_nRF_stream_request_packet->source, nRF_stream_data_packet->source ) ) {
                      if ( nRF_stream_data_packet->streamPacketCounter == i ) {
                        uint8_t packet_data_length = nRF_stream_data_packet->length - streamHeaderLength;
#if enable_debug_output
                        Serial.print("packet_data_length "); Serial.println(packet_data_length);
#endif
                        for (uint8_t j = 0; j < packet_data_length; j++) {
                          payloadBuffer[payloadSize + j] = nRF_stream_data_packet->payload[j]; // TODO check if this works!
                        }
                        payloadSize += packet_data_length;
#if enable_debug_output
                        Serial.print("payloadSize "); Serial.println(payloadSize);
#endif
                        nRF_stream_ack ack = buildStreamAckPacket(first_nRF_stream_request_packet->source, StreamFlag_dataComplete);
                        writeAck( &ack, sizeof(ack) );
#if _receiving_profiling
                        logEnd("Handled StreamData ", 0);
#endif
                        yield();
                      } else {
                        nRF_stream_ack ack = buildStreamAckPacket(first_nRF_stream_request_packet->source, StreamFlag_streamCorrupted);
                        writeAck( &ack, sizeof(ack) );
#if _receiving_profiling
                        logEnd("Handled StreamData with not fitting packetCounter", 0);
#endif
                        break; // leave surrounding loop
                      }
                    }
                  } else {
                    // Errors
                    // need to identify if other side tries to restart Stream
                    if (isStreamRequestPacket(header->type)) {
                      nRF_stream_request* nRF_stream_request_packet = (nRF_stream_request*) buffer;
                      if (isForMe(nRF_stream_request_packet->destination) && areEqual(first_nRF_stream_request_packet->source, nRF_stream_request_packet->source ) ) {
#if enable_debug_output
                        printStreamRequestPacket(*nRF_stream_request_packet);
#endif
                        noError = false;
#if _receiving_profiling
                        logEnd("Handled StreamReq => restart Stream", 0);
#endif
                        break;
                      } else {
                        // not for me => ignored
                        i++;
                      }
                    } else {
                      // any other data ignored
                      i++;
                    }
                  }
                } else if (pipeNo == 2) {
                  //ignore
                  i++;
                }
              }
              yield();
              if (noError) {
                receiveForMeCallback(nRF_header->source, &payloadBuffer, payloadSize);
              }
              payloadSize = 0;
              yield();

            }
          } else if (isBroadcast(nRF_header->destination)) {
#if _receiving_profiling
            logEnd("Identified StreamReq Broadcast", 0);
            logStart(0);
#endif
            yield();
            nRF_stream_request_data_rec* nRF_stream_request_data_packet = (nRF_stream_request_data_rec*) buffer;
#if enable_debug_output
            printStreamRequestDataPacket(*nRF_stream_request_data_packet);
#endif

            uint8_t packet_data_length = nRF_stream_request_data_packet->length - streamHeaderLength;
            for (uint8_t i = 0; i < packet_data_length; i++) {
              payloadBuffer[i] = nRF_stream_request_data_packet->payload[i]; // TODO check if this works!
            }
            payloadSize += packet_data_length;
#if _receiving_profiling
            logEnd("Dispatched StreamReq Broadcast", 0);
            logStart(0);
#endif
            yield();
            receiveBroadcastCallback(nRF_stream_request_data_packet->source, &payloadBuffer, payloadSize);
            payloadSize = 0;
            yield();
#if _receiving_profiling
            logEnd("Handled Broadcast Callback", 0);
#endif
          }

        }
#if _receiving_profiling
        logEnd("Identified non StreamReq", 0);
#endif

      } else if (pipeNo == 2) {

      }
    }

    if (breakReceiveLoop) {
#if enable_debug_output
      Serial.println("leaving receiveloop: breakReceiveLoop");
#endif
      yield();
      result = true;
      break;
    }
    if (timout != 0) {
      if (isTimeout(startTimer, timout)) {
#if enable_debug_output
        Serial.println("leaving receiveloop: timeout");
#endif
        yield();
	result = false;
        break;
      }
    }
  }
  _rf24.startListening();
  yield();
  return result;
}

bool nRF24L01p::isBroadcast(nRF_address address) {
  if (address.bytes[0] == broadcastMac.bytes[0] &&
      address.bytes[1] == broadcastMac.bytes[1] &&
      address.bytes[2] == broadcastMac.bytes[2] &&
      address.bytes[3] == broadcastMac.bytes[3] &&
      address.bytes[4] == broadcastMac.bytes[4]) {
    return true;
  }
  return false;
}



bool nRF24L01p::write( const void* buf, uint8_t len ) {
#if _send_profiling
  logStart(0);
#endif
  if (len <= maxPayloadLength) {

    bool result = sendAsStream(buf, len);
#if _send_profiling
    logEnd("nRF24L01p::write", 0);
#endif
    _rf24.startListening();
    return result;
  } else {
#if enable_debug_output
    Serial.println("Message to long");
#endif
    return false;// message is too long
#if _send_profiling
    logEnd("nRF24L01p::write", 0);
#endif
  }
}

bool nRF24L01p::writeBroadcast( const void* buf, uint8_t len ) {
#if _send_profiling
  logStart(0);
#endif
  if (len <= streamDataLength) {
    bool result = sendAsBroadcast(buf, len);
#if _send_profiling
    logEnd("nRF24L01p::write", 0);
#endif
    return result;
  } else {
#if enable_debug_output
    Serial.println("Message to long");
#endif
#if _send_profiling
    logEnd("nRF24L01p::write", 0);
#endif
    return false;// message is too long
  }
}

bool  nRF24L01p::sendAsBroadcast(const void* buf, uint8_t len) {
#if _send_profiling
  logStart(1);
#endif

  uint8_t streamLength = 0; // always 0
  nRF_stream_request_data streamRequest = buildSingleRequestDataPacket(buf, len, streamLength);
#if enable_debug_output
  printStreamRequestDataPacket(streamRequest);
#endif
  if (sendStreamRequestDataPacket(&streamRequest, sizeof(streamRequest))) {
#if _send_profiling
    logEnd("nRF24L01p::sendAsBroadcast", 1);
#endif
    return OK;
  } else {
#if enable_debug_output
    Serial.println("No StreamAck");
#endif
#if _send_profiling
    logEnd("nRF24L01p::sendAsBroadcast", 1);
#endif
    return Error;
  }
}


nRF_stream_request_data nRF24L01p::buildSingleRequestDataPacket(const void* buf, uint8_t len, uint8_t streamLength) {
#if _send_profiling
  logStart(2);
#endif
  nRF_stream_request_data packet;
  packet.length = ((uint8_t) streamHeaderLength + len);
  packet.type = (uint8_t) StreamRequest;
  packet.destination = broadcastMac;
  packet.source = myMac;
  packet.streamLength = streamLength;


  uint8_t end = len;
  uint8_t start = 0;
  uint8_t dif = end - start;
  packet.length = streamHeaderLength + dif;
  const uint8_t* payload = reinterpret_cast<const uint8_t*>(buf); //TODO move later into createSteamDataPacket
#if enable_debug_output
  Serial.print("packet.length: "); Serial.println(packet.length);
  Serial.print("packetCounter == 0"); Serial.println();
  Serial.print("start "); Serial.print(start); Serial.println();
  Serial.print("end: "); Serial.print(end); Serial.println();
  Serial.print("dif: "); Serial.print(dif); Serial.println();
#endif
  if (start + 0 < end) {
    packet.payload0 = *payload++;
  } if (start + 1 < end) {
    packet.payload1 = *payload++;
  } if (start + 2 < end) {
    packet.payload2 = *payload++;
  } if (start + 3 < end) {
    packet.payload3 = *payload++;
  } if (start + 4 < end) {
    packet.payload4 = *payload++;
  } if (start + 5 < end) {
    packet.payload5 = *payload++;
  } if (start + 6 < end) {
    packet.payload6 = *payload++;
  } if (start + 7 < end) {
    packet.payload7 = *payload++;
  } if (start + 8 < end) {
    packet.payload8 = *payload++;
  } if (start + 9 < end) {
    packet.payload9 = *payload++;
  } if (start + 10 < end) {
    packet.payload10 = *payload++;
  } if (start + 11 < end) {
    packet.payload11 = *payload++;
  } if (start + 12 < end) {
    packet.payload12 = *payload++;
  } if (start + 13 < end) {
    packet.payload13 = *payload++;
  } if (start + 14 < end) {
    packet.payload14 = *payload++;
  } if (start + 15 < end) {
    packet.payload15 = *payload++;
  } if (start + 15 < end) {
    packet.payload16 = *payload++;
  } if (start + 16 < end) {
    packet.payload17 = *payload++;
  } if (start + 17 < end) {
    packet.payload18 = *payload++;
  }
#if _send_profiling
  logEnd("nRF24L01p::buildSingleRequestDataPacket", 2);
#endif
  return packet;
}

bool nRF24L01p::sendStreamRequestDataPacket(const void* buf, uint8_t len) { // return true when sending succeeded, false when sending failed
#if _send_profiling
  logStart(2);
#endif
  _rf24.stopListening();
  delay(switchingDelay);
#if enable_debug_output
  Serial.println("sendStreamRequestDataPacket");
#endif
  for (uint8_t tried = 0; tried < stream_request_retry; tried++) {
    if (_rf24.write( buf, len )) { // irgendjemand empfängt das
#if enable_debug_output
      Serial.print("send out ");
#endif
      _rf24.startListening();
#if _send_profiling
      logEnd("nRF24L01p::sendStreamRequestDataPacket | OK", 2);
#endif
      return OK;
    } else {
      // nobody receives it, maybe everybody in range is sending
      long sleepDuration = random(stream_request_retry_minSleepDuration, stream_request_retry_maxSleepDuration) ;
#if enable_debug_output
      Serial.print("sleeping "); Serial.println(sleepDuration);
#endif
      delay(sleepDuration); // TODO exchange with a real sleep
    }
  }
  _rf24.startListening();
#if _send_profiling
  logEnd("nRF24L01p::sendStreamRequestDataPacket | Error", 2);
#endif
  return Error;
}


nRF_address nRF24L01p::tonRFAddress(const void* mac) {
  nRF_address address;
  const uint8_t* current = reinterpret_cast<const uint8_t*>(mac);
  address.bytes[0] = *current++;
  address.bytes[1] = *current++;
  address.bytes[2] = *current++;
  address.bytes[3] = *current++;
  address.bytes[4] = *current;
  return address;
}

nRF_stream_request nRF24L01p::buildSingleRequestPacket(uint8_t streamLength) {
#if _send_profiling
  logStart(2);
#endif
  nRF_stream_request packet;
  packet.length = ((uint8_t) streamHeaderLength);
  packet.type = (uint8_t) StreamRequest;
  packet.destination = destinationMac;
  packet.source = myMac;
  packet.streamLength = streamLength;
#if _send_profiling
  logEnd("nRF24L01p::buildSingleRequestPacket", 2);
#endif
  return packet;
}

nRF_stream_ack nRF24L01p::buildStreamAckPacket(nRF_address address, uint8_t streamFlags) {
#if _send_profiling
  logStart(0);
#endif
  //const uint8_t* payload = reinterpret_cast<const uint8_t*>(buf);
  nRF_stream_ack packet;
  packet.length = ((uint8_t) sizeof(packet));
  packet.type = (uint8_t) StreamAck;
  packet.destination = address;
  packet.source = myMac;
  packet.streamFlags = streamFlags;
#if _send_profiling
  logEnd("nRF24L01p::buildStreamAckPacket", 0);
#endif
  return packet;
}

nRF_stream_data nRF24L01p::createStreamDataPacket(const void* buf, uint8_t size, uint8_t  packetCount, uint8_t packetCounter) {
  const uint8_t* payload = reinterpret_cast<const uint8_t*>(buf); //TODO move later into createSteamDataPacket
#if _send_profiling
  logStart(2);
#endif
  nRF_stream_data packet;
  packet.type = (uint8_t) StreamData;
  packet.streamPacketCounter = packetCounter;
  packet.destination = destinationMac;
  packet.source = myMac;

  uint8_t start = streamDataLength * ((packetCount - 1) - packetCounter);
  uint8_t end = streamDataLength * (packetCount - packetCounter);


  if (size % streamDataLength == 0 ) {


    for (uint16_t i = 0 ; i < start; i++) {
      *payload++;
    }
    packet.length = ((uint8_t) packetLength );
    packet.payload0 = *payload++; packet.payload1 = *payload++; packet.payload2 = *payload++; packet.payload3 = *payload++; packet.payload4 = *payload++ ;
    packet.payload5 = *payload++; packet.payload6 = *payload++; packet.payload7 = *payload++; packet.payload8 = *payload++; packet.payload9 = *payload++;
    packet.payload10 = *payload++; packet.payload11 = *payload++; packet.payload12 = *payload++; packet.payload13 = *payload++; packet.payload14 = *payload++;
    packet.payload15 = *payload++; packet.payload16 = *payload++; packet.payload17 = *payload++; packet.payload18 = *payload++;
#if _send_profiling
    logEnd("nRF24L01p::createStreamDataPacket", 2);
#endif
    return packet;
  }


  if (packetCounter == 0) {
    end = size;
    uint8_t dif = end - start;
    packet.length = streamHeaderLength + dif;

    for (uint16_t i = 0 ; i < start; i++) {
      *payload++;
    }
#if enable_debug_output
    Serial.print("packet.length: "); Serial.println(packet.length);
    Serial.print("packetCounter == 0"); Serial.println();
    Serial.print("start "); Serial.print(start); Serial.println();
    Serial.print("end: "); Serial.print(end); Serial.println();
    Serial.print("dif: "); Serial.print(dif); Serial.println();
#endif
    if (start + 0 < end) {
      packet.payload0 = *payload++;
    } if (start + 1 < end) {
      packet.payload1 = *payload++;
    } if (start + 2 < end) {
      packet.payload2 = *payload++;
    } if (start + 3 < end) {
      packet.payload3 = *payload++;
    } if (start + 4 < end) {
      packet.payload4 = *payload++;
    } if (start + 5 < end) {
      packet.payload5 = *payload++;
    } if (start + 6 < end) {
      packet.payload6 = *payload++;
    } if (start + 7 < end) {
      packet.payload7 = *payload++;
    } if (start + 8 < end) {
      packet.payload8 = *payload++;
    } if (start + 9 < end) {
      packet.payload9 = *payload++;
    } if (start + 10 < end) {
      packet.payload10 = *payload++;
    } if (start + 11 < end) {
      packet.payload11 = *payload++;
    } if (start + 12 < end) {
      packet.payload12 = *payload++;
    } if (start + 13 < end) {
      packet.payload13 = *payload++;
    } if (start + 14 < end) {
      packet.payload14 = *payload++;
    } if (start + 15 < end) {
      packet.payload15 = *payload++;
    } if (start + 15 < end) {
      packet.payload16 = *payload++;
    } if (start + 16 < end) {
      packet.payload17 = *payload++;
    } if (start + 17 < end) {
      packet.payload18 = *payload++;
    }

  } else {
#if enable_debug_output
    Serial.print("start: "); Serial.println(start);
    Serial.print("end "); Serial.println(end);
#endif
    for (uint16_t i = 0 ; i < start; i++) {
      *payload++;
    }
    packet.length = ((uint8_t) packetLength );
    packet.payload0 = *payload++; packet.payload1 = *payload++; packet.payload2 = *payload++; packet.payload3 = *payload++; packet.payload4 = *payload++ ;
    packet.payload5 = *payload++; packet.payload6 = *payload++; packet.payload7 = *payload++; packet.payload8 = *payload++; packet.payload9 = *payload++;
    packet.payload10 = *payload++; packet.payload11 = *payload++; packet.payload12 = *payload++; packet.payload13 = *payload++; packet.payload14 = *payload++;
    packet.payload15 = *payload++; packet.payload16 = *payload++; packet.payload17 = *payload++; packet.payload18 = *payload++;
  }
#if _send_profiling
  logEnd("nRF24L01p::createStreamDataPacket", 2);
#endif
  return packet;
}




bool  nRF24L01p::sendAsStream(const void* buf, uint8_t len) {
#if _send_profiling
  logStart(1);
#endif

  _rf24.stopListening();
  uint8_t streamLength = calculateStreamLength(len);
  nRF_stream_request streamRequest = buildSingleRequestPacket(streamLength);
#if enable_debug_output
  printStreamRequestPacket(streamRequest);
#endif
  if (sendStreamRequestPacket(&streamRequest, sizeof(streamRequest))) {

    uint8_t streamPacketCounter = streamLength - 1;
    for (int16_t i = streamLength - 1 ; i >= 0 ; i--, streamPacketCounter--) {
      nRF_stream_data data_packet = createStreamDataPacket(buf, len, streamLength, streamPacketCounter);
#if enable_debug_output
      printStreamDataPacket(data_packet);
#endif
      if (i != 0) { // normal packet
        if (sendStreamDataPacket(&data_packet, sizeof(data_packet))) {

          // continue;
        } else {
          _rf24.startListening();
#if _send_profiling
          logEnd("nRF24L01p::sendAsStream | Error", 1);
#endif
          return Error;
        }
      } else { // last packet
        if (sendStreamDataPacket(&data_packet, sizeof(data_packet))) {
          _rf24.startListening();
#if _send_profiling
          logEnd("nRF24L01p::sendAsStream | OK", 1);
#endif
          return OK; // done;
        } else {
          _rf24.startListening();
#if _send_profiling
          logEnd("nRF24L01p::sendAsStream | Error", 1);
#endif
          return Error;
        }
      }
    }


    _rf24.startListening();
#if _send_profiling
    logEnd("nRF24L01p::sendAsStream | OK", 1);
#endif
    return OK;
  } else {
#if enable_debug_output
    Serial.println("No StreamAck");
#endif
    _rf24.startListening();
#if _send_profiling
    logEnd("nRF24L01p::sendAsStream | Error", 1);
#endif
    return Error;
  }
}

bool nRF24L01p::sendStreamRequestPacket(const void* buf, uint8_t len) { // return true when sending succeeded, false when sending failed
  for (uint8_t tried = 0; tried < stream_request_retry; tried++) {
#if _send_profiling
    logStart(2);
#endif
#if enable_debug_output
    Serial.print("sendStreamRequestPacket tried "); Serial.println(tried);
#endif
    _rf24.stopListening();
    delay(switchingDelay);
    yield();

    if (_rf24.write( buf, len )) { // someone is hearing us and the underlying layer is ok
#if enable_debug_output
      Serial.print("wait for ack "); Serial.println(tried);
#endif
      if (waitForStreamAckPacket(StreamFlag_streamAccepted)) {
#if _send_profiling
        logEnd("nRF24L01p::sendStreamRequestPacket | OK", 2);
#endif
        return OK;
      } else {
        // denied or timeout (no ack received)
        long sleepDuration = random(stream_request_retry_minSleepDuration, stream_request_retry_maxSleepDuration) ;
#if enable_debug_output
        Serial.print("sleeping "); Serial.println(sleepDuration);
#endif
        delay(sleepDuration); // TODO exchange with a real sleep
      }
    } else {
      // indicates error with the underlying layer or no one hears us because everyone in range is sending
#if enable_debug_output
      Serial.println("no one is hearing us ");
#endif
      long sleepDuration = random(stream_request_retry_minSleepDuration, stream_request_retry_maxSleepDuration) ;
#if enable_debug_output
      Serial.print("sleeping "); Serial.println(sleepDuration);
#endif
      delay(sleepDuration); // TODO exchange with a real sleep
    }
    /*
      long sleepDuration = random(request_minSleepDuration, request_maxSleepDuration) ;
      #if enable_debug_output
      Serial.print("sleeping "); Serial.println(sleepDuration);
      #endif
      delay(sleepDuration);
    */
  }
#if enable_debug_output
  Serial.println("timout: after retries no ack received (timout)");
#endif
#if _send_profiling
  logEnd("nRF24L01p::sendStreamRequestPacket | Error", 2);
#endif
  return Error;
}

bool nRF24L01p::sendStreamDataPacket(const void* buf, uint8_t len) { // return true when sending succeeded, false when sending failed
#if _send_profiling
  logStart(2);
#endif
  if (_rf24.write( buf, len )) { // alternativ das auch noch irgendwie überschreiben und mit der nächsten methode mergen
    bool  result = waitForStreamAckPacket(StreamFlag_dataComplete);
#if _send_profiling
    logEnd("nRF24L01p::sendStreamDataPacket | OK", 2);
#endif
    return result;
  }
#if _send_profiling
  logEnd("nRF24L01p::sendStreamDataPacket | Error", 2);
#endif
  return Error;
}


bool  nRF24L01p::waitForStreamAckPacket(uint8_t streamFlags) { // return true when the packet arrives, false when a timeout appears
#if _send_profiling
  logStart(3);
#endif
  uint32_t startTimeout = millis();
  _rf24.startListening();
  delay(switchingDelay);
  uint16_t delayTime = 300;
#if enable_debug_output
  Serial.println("Waiting");
#endif

  //for (int16_t i = 0; i < 5; i++) {
  while (!isTimeout(startTimeout, stream_request_ack_timeout)) {
    if ( _rf24.available()) {
#if enable_debug_output
      Serial.println("AckPayloadAvailable");
#endif
      uint8_t buffer[32];
      for (int i = 0; i < 32; i++) {
        buffer[i] = 0;
      }
      _rf24.read( &buffer, sizeof(buffer) );

#if enable_debug_output
      Serial.print("received: ");
      for (int i = 0; i < 32; i++) {
        Serial.print(buffer[i], HEX); Serial.print(" ");
      }
      Serial.println();
#endif

      message_header* header = (message_header*) buffer;
      if (isStreamAckPacket(header->type)) {
        nRF_stream_ack* nRF_stream_ack_pack = (nRF_stream_ack*) buffer;
        if (isForMe( nRF_stream_ack_pack->destination) && isFromDestination( nRF_stream_ack_pack->source)) {
          yield();
          if (nRF_stream_ack_pack->streamFlags & streamFlags) {
#if enable_debug_output
            printStreamAckPacket(*nRF_stream_ack_pack);
#endif
            _rf24.stopListening();
            delay(switchingDelay);
#if _send_profiling
            logEnd("nRF24L01p::waitForStreamAckPacket | OK", 3);
#endif
            return OK;
          } else {
            _rf24.stopListening();
            delay(switchingDelay);
#if _send_profiling
            logEnd("nRF24L01p::waitForStreamAckPacket | Error stream denied", 3);
#endif
            return Error;
          }
        }
      } else {
#if enable_debug_output
        Serial.println("type not fitting");
#endif
        continue;
      }
    }
    yield();
  }


  _rf24.stopListening();
  delay(switchingDelay);
#if enable_debug_output
  Serial.println("ack packet timout");
#endif
#if _send_profiling
  logEnd("nRF24L01p::waitForStreamAckPacket | Error timout", 3);
#endif
  return Error;
}


uint8_t nRF24L01p::calculateStreamLength(uint8_t size) {
  if (size < streamDataLength) {
    return 1;
  }

  uint8_t length = size / streamDataLength;
  if (size % streamDataLength == 0) {
    return length;
  }
  length++;
  return length;
}

bool nRF24L01p::writeAck(const void* buf, uint8_t len) {
#if _receiving_profiling
  logStart(0);
#endif
  _rf24.stopListening();
  delay(switchingDelay);
  if (_rf24.write( buf, len )) {
    _rf24.startListening();
    delay(switchingDelay);
#if _receiving_profiling
    logEnd("nRF24L01p::writeAck | OK", 0);
#endif
    return OK;
  }
  _rf24.startListening();
  delay(switchingDelay);
#if _receiving_profiling
  logEnd("nRF24L01p::writeAck | Error", 0);
#endif
  return Error;
}

bool nRF24L01p::resetDestinationMac() {
  destinationMac.bytes[0] = 0x00;
  destinationMac.bytes[1] = 0x00;
  destinationMac.bytes[2] = 0x00;
  destinationMac.bytes[3] = 0x00;
  destinationMac.bytes[4] = 0x00;
  return true;
}

bool nRF24L01p::isFromDestination(nRF_address address) {
  if (0x00 == destinationMac.bytes[0] &&
      0x00 == destinationMac.bytes[1] &&
      0x00 == destinationMac.bytes[2] &&
      0x00 == destinationMac.bytes[3] &&
      0x00 == destinationMac.bytes[4]) {
    return true;
  }
  if (address.bytes[0] == destinationMac.bytes[0] &&
      address.bytes[1] == destinationMac.bytes[1] &&
      address.bytes[2] == destinationMac.bytes[2] &&
      address.bytes[3] == destinationMac.bytes[3] &&
      address.bytes[4] == destinationMac.bytes[4]) {
    return true;
  }
  return false;
}

bool nRF24L01p::isForMe(nRF_address address) {
  if (address.bytes[0] == myMac.bytes[0] &&
      address.bytes[1] == myMac.bytes[1] &&
      address.bytes[2] == myMac.bytes[2] &&
      address.bytes[3] == myMac.bytes[3] &&
      address.bytes[4] == myMac.bytes[4]) {
    return true;
  }
  return false;
}

bool nRF24L01p::areEqual(nRF_address address1, nRF_address address2) {
  if (address1.bytes[0] == address2.bytes[0] &&
      address1.bytes[1] == address2.bytes[1] &&
      address1.bytes[2] == address2.bytes[2] &&
      address1.bytes[3] == address2.bytes[3] &&
      address1.bytes[4] == address2.bytes[4]) {
    return true;
  }
  return false;
}

bool nRF24L01p::isStreamDataPacket(uint8_t type) {
  if (type == StreamData) {
    return true;
  }
  return false;
}

bool nRF24L01p::isStreamAckPacket(uint8_t type) {
  if (type == StreamAck) {
    return true;
  }
  return false;
}

bool nRF24L01p::isStreamRequestPacket(uint8_t type) {
  if (type == StreamRequest) {
    return true;
  }
  return false;
}


void nRF24L01p::printStreamRequestPacket(nRF_stream_request packet) {
  Serial.println("=== Stream Request ===");

  Serial.print("length "); Serial.print(packet.length); Serial.println(" bytes");
  Serial.print("type "); Serial.println(packet.type, HEX);


  Serial.print("destination ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.destination.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("source ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.source.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("streamLength "); Serial.println(packet.streamLength);
  Serial.println("=========================================================");
}

void nRF24L01p::printStreamRequestDataPacket(nRF_stream_request_data packet) {
  nRF_stream_request_data_rec* rec = (nRF_stream_request_data_rec*) &packet;
  printStreamRequestDataPacket( *rec);
}

void nRF24L01p::printStreamRequestDataPacket(nRF_stream_request_data_rec packet) {
  Serial.println("=== Stream Data ===");

  Serial.print("length "); Serial.print(packet.length); Serial.println(" bytes");
  Serial.print("type "); Serial.println(packet.type, HEX);
  Serial.print("streamLength "); Serial.println(packet.streamLength);

  Serial.print("destination ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.destination.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("source ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.source.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("payload ");
  for (int i = 0; i < packet.length - streamHeaderLength; i++) {
    Serial.print(packet.payload[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("empty-area ");

  for (int16_t i = packet.length - streamHeaderLength; i < streamDataLength; i++) {
    Serial.print(packet.payload[i], HEX); Serial.print(" ");
  }
  Serial.println();
  Serial.println("=========================================================");
}

void nRF24L01p::printStreamDataPacket(nRF_stream_data packet) {
  nRF_stream_data_rec* rec = (nRF_stream_data_rec*) &packet;
  printStreamDataPacket( *rec);
}
void nRF24L01p::printStreamDataPacket(nRF_stream_data_rec packet) {
  Serial.println("=== Stream Data ===");

  Serial.print("length "); Serial.print(packet.length); Serial.println(" bytes");
  Serial.print("type "); Serial.println(packet.type, HEX);
  Serial.print("streamPacketCounter "); Serial.println(packet.streamPacketCounter);

  Serial.print("destination ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.destination.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("source ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.source.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("payload ");
  for (int i = 0; i < packet.length - streamHeaderLength; i++) {
    Serial.print(packet.payload[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("empty-area ");

  for (int16_t i = packet.length - streamHeaderLength; i < streamDataLength; i++) {
    Serial.print(packet.payload[i], HEX); Serial.print(" ");
  }
  Serial.println();
  Serial.println("=========================================================");
}
void nRF24L01p::printStreamAckPacket(nRF_stream_ack packet) {
  Serial.println("=== Stream Ack ===");

  Serial.print("length "); Serial.print(packet.length); Serial.println(" bytes");
  Serial.print("type "); Serial.println(packet.type, HEX);


  Serial.print("destination ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.destination.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("source ");
  for (int i = 0; i < 5; i++) {
    Serial.print(packet.source.bytes[i], HEX); Serial.print(" ");
  }
  Serial.println();

  Serial.print("streamFlags "); Serial.println(packet.streamFlags, BIN);
  Serial.println("=========================================================");
}

bool nRF24L01p::isTimeout(uint32_t startTimeout, uint32_t maxTimoutValue) {
  uint32_t end = millis();
  uint32_t duration = 0;
  if (end < startTimeout) {
    // overflow
    duration = UINT64_MAX - startTimeout;
    duration +=  end;
  } else {
    duration = end - startTimeout;
  }
  if (duration > maxTimoutValue) {
    return true;
  }
  return false;
}

#if defined(_startup_profiling) || defined(_send_profiling) || defined(_receiving_profiling)

void nRF24L01p::logStart( uint8_t timer) {
  start[timer] = millis();
}

void nRF24L01p::logStop( uint8_t timer) {
  start[timer] = 0;
}

void nRF24L01p::logEnd(const char* profile_message, uint8_t timer) {
  uint64_t end = millis();
  uint64_t duration = 0;
  if (end < start[timer]) {
    // overflow
    duration = UINT64_MAX - start[timer];
    duration +=  end;
  } else {
    duration = end - start[timer];
  }
  Serial.print(profile_message);
  Serial.print(" duration: ");

  Serial.println((unsigned long) duration);

  start[timer] = 0;
}

#endif


