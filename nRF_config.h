
#ifndef __nRF_config
#define __nRF_config


// nRF24L01 hardware configuration
#if ESP8266
#define RF24Pin1        4
#define RF24Pin2        15
#else
#define RF24Pin1        7
#define RF24Pin2        8
#endif

#define maxPayloadLength 57 // 8 packets maximum
#define used_pipes	{ 0x9B9B9B9B9B9B }

// default retry, sleeping and timeout
#define stream_request_retry                     3 // times
#define stream_request_retry_minSleepDuration 1000 // ms
#define stream_request_retry_maxSleepDuration 3000 // ms
#define stream_request_ack_timeout            2000 // ms
#define stream_data_ack_timeout               2000 // ms
#define stream_data_timeout	              1000 // ms

#define switchingDelay                           5 // ms, delay between switching stop-/startListening 
#define stream_request_ack_switching_delay       10 // ms, best practive value

// change to 1 for output
#define enable_debug_output       0 
#define enable_payload_printout   0
#define _startup_profiling        0
#define _send_profiling           0
#define _receiving_profiling      0


#endif
