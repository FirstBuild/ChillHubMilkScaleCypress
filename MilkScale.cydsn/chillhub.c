//#include "Arduino.h"
#include <project.h>
#include "chillhub.h"
#include <cytypes.h>
#include <stdlib.h>
#include <cylib.h>
#include "Uart.h"
#include "Uart_SPI_UART.h"
#include "DebugUart.h"
//#include "pt.h"

#ifndef NULL
#define NULL 0
#endif

/*
 * Private Stuff
 */
static unsigned char recvBuf[64] = { 0 };
static uint8_t bufIndex;

static chCbTableType* callbackTable = NULL;

// Message handling stuff
static uint8_t payloadLen;
static uint8_t msgType;
static uint8_t dataType;

/*
 * Private function prototypes
 */
static void storeCallbackEntry(unsigned char id, unsigned char typ, void(*fcn)());
static chillhubCallbackFunction callbackLookup(unsigned char sym, unsigned char typ);
static void callbackRemove(unsigned char sym, unsigned char typ);
static void setName(char* name, unsigned char strLength);
static void setup(char* name, unsigned char strLength, const T_Serial* serial);
static void subscribe(unsigned char type, chillhubCallbackFunction cb);
static void unsubscribe(unsigned char type);
static void setAlarm(unsigned char ID, char* cronString, unsigned char strLength, chillhubCallbackFunction cb);
static void unsetAlarm(unsigned char ID);
static void getTime(chillhubCallbackFunction cb);
static void addCloudListener(unsigned char msgType, chillhubCallbackFunction cb);
static void sendU8Msg(unsigned char msgType, unsigned char payload);
static void sendU16Msg(unsigned char msgType, unsigned int payload);
static void sendI8Msg(unsigned char msgType, signed char payload);
static void sendI16Msg(unsigned char msgType, signed int payload);
static void sendBooleanMsg(unsigned char msgType, unsigned char payload);
static void loop(void);

// The singleton ChillHub instance
const chInterface ChillHub = {
   .setup = setup,
   .subscribe = subscribe,
   .unsubscribe = unsubscribe,
   .setAlarm = setAlarm,
   .unsetAlarm = unsetAlarm,
   .getTime = getTime,
   .addCloudListener = addCloudListener,
   .sendU8Msg = sendU8Msg,
   .sendU16Msg = sendU16Msg,
   .sendI8Msg = sendI8Msg,
   .sendI16Msg = sendI16Msg,
   .sendBooleanMsg = sendBooleanMsg,
   .loop = loop
};

static const T_Serial *Serial;

/*
 * Functions
 */

void printU8(uint8_t val) {
  uint8_t digits[3];
  uint8_t i;
  
  for (i=0; i<sizeof(digits); i++) {
    digits[sizeof(digits)-i-1] = val % 10;
    val = val / 10;
  }

  for(i=0; (i<(sizeof(digits)-1))&&(digits[i] == 0); i++);
  
  for (; i<sizeof(digits); i++) {
    DebugUart_SpiUartWriteTxData(digits[i]+'0');
  }
}

void printU16(uint16_t val) {
  uint8_t digits[5];
  uint8_t i;
  
  for (i=0; i<sizeof(digits); i++) {
    digits[sizeof(digits)-i-1] = val % 10;
    val = val / 10;
  }

  for(i=0; (i<(sizeof(digits)-1))&&(digits[i] == 0); i++);
  
  for (; i<sizeof(digits); i++) {
    DebugUart_SpiUartWriteTxData(digits[i]+'0');
  }
}

void printI16(int16_t val) {
  uint8_t digits[5];
  uint8_t i;
  
  if (val < 0) {
    DebugUart_SpiUartWriteTxData('-');
    val = -val;
  }
  
  for (i=0; i<sizeof(digits); i++) {
    digits[sizeof(digits)-i-1] = val % 10;
    val = val / 10;
  }

  for(i=0; (i<(sizeof(digits)-1))&&(digits[i] == 0); i++);
  
  for (; i<sizeof(digits); i++) {
    DebugUart_SpiUartWriteTxData(digits[i]+'0');
  }
}

static void setup(char* name, unsigned char strLength, const T_Serial* serial) {
  Serial = serial;
  // register device type with chillhub mailman
  DebugUart_UartPutString("Initializing chillhub interface...\r\n");
  setName(name, strLength);
  DebugUart_UartPutString("...initialized.\r\n");
}

static void sendU8Msg(unsigned char msgType, unsigned char payload) {
  uint8_t buf[4];
  
  DebugUart_UartPutString("Sendig U8 message.\r\n");

  buf[0] = 3;
  buf[1] = msgType;
  buf[2] = unsigned8DataType;
  buf[3] = payload;
  Serial->write(buf, 4);
}

static void sendI8Msg(unsigned char msgType, signed char payload) {
  uint8_t buf[4];
  
  DebugUart_UartPutString("Sendig I8 message.\r\n");

  buf[0] = 3;
  buf[1] = msgType;
  buf[2] = signed8DataType;
  buf[3] = payload;
  Serial->write(buf, 4);
}

static void sendU16Msg(unsigned char msgType, unsigned int payload) {
  uint8_t buf[5];

  DebugUart_UartPutString("Sendig U16 message.\r\n");

  buf[0] = 4;
  buf[1] = msgType;
  buf[2] = unsigned16DataType;
  buf[3] = (payload >> 8) & 0xff;
  buf[4] = payload & 0xff;
  Serial->write(buf, 5);
}

static void sendI16Msg(unsigned char msgType, signed int payload) {
  uint8_t buf[5];

  DebugUart_UartPutString("Sendig I16 message.\r\n");

  buf[0] = 4;
  buf[1] = msgType;
  buf[2] = signed16DataType;
  buf[3] = (payload >> 8) & 0xff;
  buf[4] = payload & 0xff;
  Serial->write(buf, 5);
}

static void sendBooleanMsg(unsigned char msgType, unsigned char payload) {
  uint8_t buf[4];

  DebugUart_UartPutString("Sendig boolean message.\r\n");

  buf[0] = 3;
  buf[1] = msgType;
  buf[2] = booleanDataType;
  buf[3] = payload;
  Serial->write(buf, 4);
}

static void setName(char* name, unsigned char strLength) {
  uint8_t buf[4];
  
  DebugUart_UartPutString("Setting name.\r\n");

  buf[0] = strLength + 3; // length of the following message
  buf[1] = deviceIdMsgType;
  buf[2] = stringDataType;
  buf[3] = strLength; // string length
  Serial->write(buf,4); // send all that so that we can use Serial.print for the string
  Serial->print(name);
}

static void subscribe(unsigned char type, chillhubCallbackFunction callback) {
  DebugUart_UartPutString("Received subscription request.\r\n");
  
  storeCallbackEntry(type, CHILLHUB_CB_TYPE_FRIDGE, callback);
  sendU8Msg(subscribeMsgType, type);
}

static void unsubscribe(unsigned char type) {
  DebugUart_UartPutString("Received unsubscription request.\r\n");
  
  sendU8Msg(unsubscribeMsgType, type);
  callbackRemove(type, CHILLHUB_CB_TYPE_FRIDGE);
}

static void setAlarm(unsigned char ID, char* cronString, unsigned char strLength, chillhubCallbackFunction callback) {
  uint8_t buf[5];

  DebugUart_UartPutString("Received set alarm request.\r\n");
  
  storeCallbackEntry(ID, CHILLHUB_CB_TYPE_CRON, callback);  
  buf[0] = strLength + 4; // message length
  buf[1] = setAlarmMsgType;
  buf[2] = stringDataType;
  buf[3] = strLength + 1; // string length
  buf[4] = ID; // callback id... it's best to use a character here otherwise things don't work right
  Serial->write(buf,5); // send all that so that we can use Serial.print for the string
  Serial->print(cronString);
}

static void unsetAlarm(unsigned char ID) {
  DebugUart_UartPutString("Received unset alarm request.\r\n");
  
  sendU8Msg(unsetAlarmMsgType, ID);
  callbackRemove(ID, CHILLHUB_CB_TYPE_CRON);
}

static void getTime(chillhubCallbackFunction cb) {
  uint8_t buf[2];
  
  DebugUart_UartPutString("Sending get time message.\r\n");
  
  storeCallbackEntry(0, CHILLHUB_CB_TYPE_TIME, cb);

  buf[0] = 1;
  buf[1] = getTimeMsgType;
  Serial->write(buf,2);
}

static void addCloudListener(unsigned char ID, chillhubCallbackFunction cb) {
  DebugUart_UartPutString("Adding cloud listener.\r\n");
  
  storeCallbackEntry(ID, CHILLHUB_CB_TYPE_CLOUD, cb);
}

// the communication states
enum ECommState {
  State_WaitingForFirstByte,
  State_WaitingForMessageType,
  State_WaitingForPayload,
  State_Invalid = 0xff
};

static void processChillhubMessagePayload(void) {
  chillhubCallbackFunction callback = NULL;
  
  // got the payload, process the message
  bufIndex = 0;
  dataType = recvBuf[bufIndex++];
  
  if ((msgType == alarmNotifyMsgType) || (msgType == timeResponseMsgType)) {
    // data is an array, don't care about data type or length
    bufIndex+=2;
    if (msgType == alarmNotifyMsgType) {
      DebugUart_UartPutString("Got an alarm notification.\r\n");
      callback = callbackLookup(recvBuf[bufIndex++], CHILLHUB_CB_TYPE_CRON);
    }
    else {
      DebugUart_UartPutString("Received a time response.\r\n");
      callback = callbackLookup(0, CHILLHUB_CB_TYPE_TIME);
    }

    if (callback) {
      unsigned char time[4];
      for (uint8_t j = 0; j < 4; j++)
      {
        time[j] = recvBuf[bufIndex++];
      }
      DebugUart_UartPutString("Calling time response/alarm callback.\r\n");
      ((chCbFcnTime)callback)(time); // <-- I don't think this works this way...

      if (msgType == timeResponseMsgType) {
        callbackRemove(0, CHILLHUB_CB_TYPE_TIME);
      }
    } else {
      DebugUart_UartPutString("No callback found.\r\n");
    }
  }
  else {
    DebugUart_UartPutString("Received a message: ");
    printU8(msgType);
    DebugUart_UartPutString("\r\n");
    callback = callbackLookup(msgType, (msgType <= CHILLHUB_RESV_MSG_MAX)?CHILLHUB_CB_TYPE_FRIDGE:CHILLHUB_CB_TYPE_CLOUD);

    if (callback) {
      DebugUart_UartPutString("Found a callback for this message, calling...\r\n");
      if ((dataType == unsigned8DataType) || (dataType == booleanDataType)) {
        ((chCbFcnU8)callback)(recvBuf[bufIndex++]);
      }
      else if (dataType == unsigned16DataType) {
        unsigned int payload = 0;
        payload |= (recvBuf[bufIndex++] << 8);
        payload |= recvBuf[bufIndex++];
        ((chCbFcnU16)callback)(payload);
      }
      else if (dataType == unsigned32DataType) {
        unsigned long payload = 0;
        for (char j = 0; j < 4; j++) {
          payload = payload << 8;
          payload |= recvBuf[bufIndex++];
        }
        ((chCbFcnU32)callback)(payload);
      }
    } else {
      DebugUart_UartPutString("No callback for this message found.\r\n");
    }
  }
}

// state handlers
static uint8_t StateHandler_WaitingForFirstByte(void) {
  if (Serial->available() > 0) {
    // Get the payload length.  It is one less than the message length.
    payloadLen = Serial->read() - 1;
    DebugUart_UartPutString("Got the payload length: ");
    printU8(payloadLen);
    DebugUart_UartPutString("\r\n");
    // Do a size check. Message must fit in the buffer.
    // If it doesn't, we just keep scanning.
    if (payloadLen <= sizeof(recvBuf)) {
      return State_WaitingForMessageType;
    }
  }
  
  return State_WaitingForFirstByte;
}

static uint8_t StateHandler_WaitingForMessageType(void) {
  if (Serial->available() > 0) {
    // get the message type
    msgType = Serial->read();
    DebugUart_UartPutString("Got the message type: ");
    printU8(msgType);
    DebugUart_UartPutString("\r\n");
    bufIndex = 0;
    return State_WaitingForPayload;
  }
  
  return State_WaitingForMessageType;  
}

static uint8_t StateHandler_WaitingForPayload(void) {
  uint8_t b;
  
  if (Serial->available() > 0) {
    b = Serial->read();
    recvBuf[bufIndex++] = b;
    DebugUart_UartPutString("Got the byte: ");
    printU8(b);
    DebugUart_UartPutString("\r\n");
  }
  if (bufIndex >= payloadLen)
  {
    processChillhubMessagePayload();
    return State_WaitingForFirstByte;
  }
  
  return State_WaitingForPayload;  
}

typedef uint8_t (*StateHandler_fp)(void);
// Array of state handlers
static const StateHandler_fp StateHandlers[] = {
  StateHandler_WaitingForFirstByte,
  StateHandler_WaitingForMessageType,
  StateHandler_WaitingForPayload,
  NULL
};

static uint8_t currentState = State_WaitingForFirstByte;

static void loop(void) {
  if (currentState < State_Invalid) {
    if(StateHandlers[currentState] != NULL) {
      currentState = StateHandlers[currentState]();
    } 
  }
}

static void storeCallbackEntry(unsigned char sym, unsigned char typ, chillhubCallbackFunction fcn) {
  chCbTableType* newEntry = (chCbTableType *)malloc(sizeof(chCbTableType));
  newEntry->symbol = sym;
  newEntry->type = typ;
  newEntry->callback = fcn;
  newEntry->rest = callbackTable;
  callbackTable = newEntry;
}

static chillhubCallbackFunction callbackLookup(unsigned char sym, unsigned char typ) {
  chCbTableType* entry = callbackTable;
  while (entry) {
    if ((entry->type == typ) && (entry->symbol == sym))
      return (entry->callback);
    else
      entry = entry->rest;
  }
  return NULL;
}

static void callbackRemove(unsigned char sym, unsigned char typ) {
  chCbTableType* prev = callbackTable;
  chCbTableType* entry;
  if (prev)
    entry = prev->rest;

  while (entry) {
    if ((entry->type == typ) && (entry->symbol == sym)) {
      prev->rest = entry->rest;
      free(entry);
    }
    else {
      prev = entry;
      entry = entry->rest;
    }
  }
}
