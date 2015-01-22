//#include "Arduino.h"
#include <project.h>
#include "chillhub.h"
#include <cytypes.h>
#include <stdlib.h>
#include <cylib.h>
#include <string.h>
#include "Uart.h"
#include "Uart_SPI_UART.h"
#include "DebugUart.h"
#include "ringbuf.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef MSB_OF_U16
  #define MSB_OF_U16(v) ((v>>8)&0x00ff) 
#endif
#ifndef LSB_OF_U16
  #define LSB_OF_U16(v) (v&0x00ff) 
#endif

#define STX 0xff
#define ESC 0xfe

/*
 * Private Stuff
 */

// Message handling stuff
static unsigned char recvBuf[64] = { 0 };
static uint8_t bufIndex;
static uint8_t payloadLen;
static uint8_t msgType;
static uint8_t dataType;

// Packet handling stuff
static unsigned char packetBuf[64] = { 0 };
static T_RingBufferCB packetBufCb;
static uint8_t packetLen;
static uint8_t packetIndex;

#define MAX_CALLBACKS (10)
#define NO_CALLBACK (0xff)
static chCbTableType callbackTable[MAX_CALLBACKS];

/*
 * Private function prototypes
 */
static void storeCallbackEntry(unsigned char id, unsigned char typ, void(*fcn)());
static chillhubCallbackFunction callbackLookup(unsigned char sym, unsigned char typ);
static uint8_t getIndexOfCallback(unsigned char sym, unsigned char typ);
static uint8_t getUnusedIndexFromCallbackTable(void);
static void callbackRemove(unsigned char sym, unsigned char typ);
static void setName(const char* name, const char *UUID);
static void setup(const char* name, const char *UUID, const T_Serial* serial);
static void subscribe(unsigned char type, chillhubCallbackFunction cb);
static void unsubscribe(unsigned char type);
static void setAlarm(unsigned char ID, char* cronString, unsigned char strLength, chillhubCallbackFunction cb);
static void unsetAlarm(unsigned char ID);
static void getTime(chillhubCallbackFunction cb);
static void addCloudListener(unsigned char msgType, chillhubCallbackFunction cb);
static void createCloudResourceU16(const char *name, uint8_t resID, uint8_t canUpdate, uint16_t initVal);
static void updateCloudResourceU16(uint8_t resID, uint16_t val);
static void sendU8Msg(unsigned char msgType, unsigned char payload);
static void sendU16Msg(unsigned char msgType, unsigned int payload);
static void sendI8Msg(unsigned char msgType, signed char payload);
static void sendI16Msg(unsigned char msgType, signed int payload);
static void sendBooleanMsg(unsigned char msgType, unsigned char payload);
static void loop(void);
static void sendPacket(uint8_t *buf, uint8_t len);
static uint8_t isControlChar(uint8_t c);
static void outputChar(uint8_t c);

// The singleton ChillHub instance
const chInterface ChillHub = {
   .setup = setup,
   .subscribe = subscribe,
   .unsubscribe = unsubscribe,
   .setAlarm = setAlarm,
   .unsetAlarm = unsetAlarm,
   .getTime = getTime,
   .addCloudListener = addCloudListener,
   .createCloudResourceU16 = createCloudResourceU16,
   .updateCloudResourceU16 = updateCloudResourceU16,
   .sendU8Msg = sendU8Msg,
   .sendU16Msg = sendU16Msg,
   .sendI8Msg = sendI8Msg,
   .sendI16Msg = sendI16Msg,
   .sendBooleanMsg = sendBooleanMsg,
   .loop = loop
};

static const T_Serial *Serial;

enum eMsgByteIndices {
  lenIndex = 0,
  msgTypeIndex = 1,
  dataTypeIndex = 2
};

const char resIdKey[] = "resID";
const char valKey[] = "val";

const uint8_t sizeOfU16JsonField = 3;
const uint8_t sizeOfU8JsonField = 2;

/*
 * Functions
 */

uint8_t sizeOfJsonKey(const char *key) {
  return strlen(key) + 1;  
}

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

void printU32(uint32_t val) {
  uint8_t digits[10];
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

static void setup(const char* name, const char *UUID, const T_Serial* serial) {
  uint8_t i;
  Serial = serial;
  
  RingBuffer_Init(&packetBufCb, &packetBuf[0], sizeof(packetBuf));
  
  // Initialize callback array
  for(i=0; i<MAX_CALLBACKS; i++) {
    callbackTable[i].inUse = FALSE;
  }
  
  // register device type with chillhub mailman
  DebugUart_UartPutString("Initializing chillhub interface...\r\n");
  setName(name, UUID);
  DebugUart_UartPutString("...initialized.\r\n");
}

static void sendU8Msg(unsigned char msgType, unsigned char payload) {
  uint8_t buf[16];
  uint8_t index=0;
  
  DebugUart_UartPutString("Sending U8 message.\r\n");

  buf[index++] = 3;
  buf[index++] = msgType;
  buf[index++] = unsigned8DataType;
  buf[index++] = payload;
  sendPacket(buf, index);
}

static void sendI8Msg(unsigned char msgType, signed char payload) {
  uint8_t buf[16];
  uint8_t index=0;
  
  DebugUart_UartPutString("Sendig I8 message.\r\n");

  buf[index++] = 3;
  buf[index++] = msgType;
  buf[index++] = signed8DataType;
  buf[index++] = payload;
  sendPacket(buf, index);
}

static void sendU16Msg(unsigned char msgType, unsigned int payload) {
  uint8_t buf[16];
  uint8_t index=0;

  DebugUart_UartPutString("Sendig U16 message.\r\n");

  buf[index++] = 4;
  buf[index++] = msgType;
  buf[index++] = unsigned16DataType;
  buf[index++] = (payload >> 8) & 0xff;
  buf[index++] = payload & 0xff;
  sendPacket(buf, index);
}

static void sendI16Msg(unsigned char msgType, signed int payload) {
  uint8_t buf[16];
  uint8_t index=0;

  DebugUart_UartPutString("Sendig I16 message.\r\n");

  buf[index++] = 4;
  buf[index++] = msgType;
  buf[index++] = signed16DataType;
  buf[index++] = (payload >> 8) & 0xff;
  buf[index++] = payload & 0xff;
  sendPacket(buf, index);
}

static void sendBooleanMsg(unsigned char msgType, unsigned char payload) {
  uint8_t buf[16];
  uint8_t index=0;

  DebugUart_UartPutString("Sending boolean message.\r\n");

  buf[index++] = 3;
  buf[index++] = msgType;
  buf[index++] = booleanDataType;
  buf[index++] = payload;
  sendPacket(buf, index);
}

static void setName(const char* name, const char *UUID) {
  uint8_t buf[256];
  uint8_t nameLen = strlen(name);
  uint8_t uuidLen = strlen(UUID);
  uint8_t index=0;
  
  if ((nameLen + uuidLen) >= sizeof(buf)) {
    DebugUart_UartPutString("Can't set name.");
  }
  
  // send header info
  buf[index++] = nameLen + uuidLen + 6; // length of the following message
  buf[index++] = deviceIdMsgType;
  buf[index++] = arrayDataType;
  buf[index++] = 2; // number of elements
  buf[index++] = stringDataType; // data type of elements

  // send device type
  buf[index++] = nameLen;
  strcat((char *)&buf[index], name);
  index += nameLen;
  
  // send UUID
  buf[index++] = uuidLen;
  strcat((char *)&buf[index], UUID);
  index += uuidLen;
  sendPacket(buf, index);
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
  uint8_t buf[256];
  uint8_t index=0;

  DebugUart_UartPutString("Received set alarm request.\r\n");
  
  storeCallbackEntry(ID, CHILLHUB_CB_TYPE_CRON, callback);  
  buf[index++] = strLength + 4; // message length
  buf[index++] = setAlarmMsgType;
  buf[index++] = stringDataType;
  buf[index++] = strLength + 1; // string length
  buf[index++] = ID; // callback id... it's best to use a character here otherwise things don't work right
  strncat((char *)&buf[index], cronString, strLength);
  index += strLength;
  sendPacket(buf, index);
}

static void unsetAlarm(unsigned char ID) {
  DebugUart_UartPutString("Received unset alarm request.\r\n");
  
  sendU8Msg(unsetAlarmMsgType, ID);
  callbackRemove(ID, CHILLHUB_CB_TYPE_CRON);
}

static void getTime(chillhubCallbackFunction cb) {
  uint8_t buf[16];
  uint8_t index=0;
  
  DebugUart_UartPutString("Sending get time message.\r\n");
  
  storeCallbackEntry(0, CHILLHUB_CB_TYPE_TIME, cb);

  buf[index++] = 1;
  buf[index++] = getTimeMsgType;
  sendPacket(buf, index);
}

static void addCloudListener(unsigned char ID, chillhubCallbackFunction cb) {
  DebugUart_UartPutString("Adding cloud listener.\r\n");
  
  storeCallbackEntry(ID, CHILLHUB_CB_TYPE_CLOUD, cb);
}

static uint8_t appendJsonKey(uint8_t *pBuf, const char *key) {
  uint8_t keyLen = strlen(key);
  *pBuf = keyLen;
  pBuf++;
  strncpy((char *)pBuf, key, keyLen);
  return keyLen + 1;
}

static uint8_t appendJsonString(uint8_t *pBuf, const char *s) {
  uint8_t len = strlen(s);
  *pBuf++ = stringDataType;
  *pBuf++ = len;
  strcpy((char*)pBuf, s);
  return len + 2;
}

static uint8_t appendJsonU8(uint8_t *pBuf, uint8_t v) {
  *pBuf++ = unsigned8DataType;
  *pBuf++ = v;
  return 2;
}

static uint8_t appendJsonU16(uint8_t *pBuf, uint16_t v) {
  *pBuf++ = unsigned16DataType;
  *pBuf++ = MSB_OF_U16(v);
  *pBuf++ = LSB_OF_U16(v);
  return 3;
}

static void createCloudResourceU16(const char *name, uint8_t resID, uint8_t canUpdate, uint16_t initVal) {
  uint8_t buf[256];
  uint8_t index=0;
  
  // set up message header and send
  index = 0;
  buf[index++] = 37 + strlen(name); // length
  buf[index++] = registerResourceType; // message type
  buf[index++] = jsonDataType; // message data type
  buf[index++] = 4; // JSON fields

  index += appendJsonKey(&buf[index], "name");
  index += appendJsonString(&buf[index], name);
  
  index += appendJsonKey(&buf[index], resIdKey);
  index += appendJsonU8(&buf[index], resID);

  index += appendJsonKey(&buf[index], "canUp");
  index += appendJsonU8(&buf[index], canUpdate);
  
  index += appendJsonKey(&buf[index], "initVal");
  index += appendJsonU16(&buf[index], initVal);
  
  sendPacket(buf, index);
}

static void updateCloudResourceU16(uint8_t resID, uint16_t val) {
  uint8_t buf[64];
  uint8_t index = 0;

  buf[index++] = 3 + 
    sizeOfJsonKey(resIdKey) + sizeOfU8JsonField +
    sizeOfJsonKey(valKey) + sizeOfU16JsonField;
    
  buf[index++] = updateResourceType;
  buf[index++] = jsonDataType;
  buf[index++] = 2; // number of json fields
  
  index += appendJsonKey(&buf[index], resIdKey);
  index += appendJsonU8(&buf[index], resID);
  index += appendJsonKey(&buf[index], valKey);
  index += appendJsonU16(&buf[index], val);
  
  sendPacket(buf, index);
}

// the communication states
enum ECommState {
  State_WaitingForStx,
  State_WaitingForLength,
  State_WaitingForPacket,
  State_Invalid = 0xff
};

static void processChillhubMessagePayload(void) {
  chillhubCallbackFunction callback = NULL;
  
  // got the payload, process the message
  bufIndex = 0;
  payloadLen = recvBuf[bufIndex++];
  msgType = recvBuf[bufIndex++];
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
      switch(dataType) {
        case stringDataType:
          DebugUart_UartPutString("Data type is a string.\r\n");
          ((chCbFcnStr)callback)((char *)&recvBuf[bufIndex]);
          break;
        case unsigned8DataType:
        case booleanDataType:
          ((chCbFcnU8)callback)(recvBuf[bufIndex++]);
          break;
        case unsigned16DataType: {
          unsigned int payload = 0;
          DebugUart_UartPutString("Data type is a U16.\r\n");
          payload |= (recvBuf[bufIndex++] << 8);
          payload |= recvBuf[bufIndex++];
          ((chCbFcnU16)callback)(payload);
          break;
        }
        case unsigned32DataType: {
          unsigned long payload = 0;
          DebugUart_UartPutString("Data type is a U32.\r\n");
          for (char j = 0; j < 4; j++) {
            payload = payload << 8;
            payload |= recvBuf[bufIndex++];
          }
          ((chCbFcnU32)callback)(payload);          
          break;
        }
        default:
          DebugUart_UartPutString("Don't know what this data type is: ");
          printU8(dataType);
          DebugUart_UartPutString("\r\n");
      }
      #ifdef KILL
      if ((dataType == unsigned8DataType) || (dataType == booleanDataType)) {
        DebugUart_UartPutString("Data type is U8 or bool.\r\n");
        ((chCbFcnU8)callback)(recvBuf[bufIndex++]);
      }
      else if (dataType == unsigned16DataType) {
        unsigned int payload = 0;
        DebugUart_UartPutString("Data type is a U16.\r\n");
        payload |= (recvBuf[bufIndex++] << 8);
        payload |= recvBuf[bufIndex++];
        ((chCbFcnU16)callback)(payload);
      }
      else if (dataType == unsigned32DataType) {
        unsigned long payload = 0;
        DebugUart_UartPutString("Data type is a U32.\r\n");
        for (char j = 0; j < 4; j++) {
          payload = payload << 8;
          payload |= recvBuf[bufIndex++];
        }
        ((chCbFcnU32)callback)(payload);
      } else {
        DebugUart_UartPutString("Don't know what this data type is: ");
        printU8(dataType);
        DebugUart_UartPutString("\r\n");
      }
      #endif
    } else {
      DebugUart_UartPutString("No callback for this message found.\r\n");
    }
  }
}

static void ReadFromSerialPort(void) {
  if (Serial->available() > 0) {
    // Get the payload length.  It is one less than the message length.
    if (RingBuffer_IsFull(&packetBufCb) == RING_BUFFER_IS_FULL) {
      DebugUart_UartPutString("Ringbuffer was full, removing a byte.\r\n");
      RingBuffer_Read(&packetBufCb); 
    }
    RingBuffer_Write(&packetBufCb, Serial->read());
  }
}

static void CheckPacket(void) {
  uint8_t i;
  uint16_t cs = 42;
  uint16_t csSent = (recvBuf[bufIndex-2]<<8) + recvBuf[bufIndex-1];
  bufIndex -= 2;
  
  for(i=0; i<bufIndex; i++) {
    cs += recvBuf[i];
  }
  
  if (cs == csSent) {
    DebugUart_UartPutString("Checksum checks!\r\n");
    processChillhubMessagePayload();
  } else {
    DebugUart_UartPutString("Checksum FAILED!\r\n");
    DebugUart_UartPutString("Checksum received: ");
    printU16(csSent);
    DebugUart_UartPutString("\r\nChecksum calc'd: ");
    printU16(cs);
    DebugUart_UartPutString("\r\n");
  }
}

// state handlers
static uint8_t StateHandler_WaitingForStx(void) {
  ReadFromSerialPort();
  
  // process bytes in the buffer
  while(RingBuffer_IsEmpty(&packetBufCb) == RING_BUFFER_NOT_EMPTY) {
    if (RingBuffer_Read(&packetBufCb) == STX) {
      DebugUart_UartPutString("Got STX.\r\n");
      return State_WaitingForLength;
    }
  }
  
  return State_WaitingForStx;
}

static uint8_t StateHandler_WaitingForLength(void) {
  ReadFromSerialPort();
  
  if (RingBuffer_IsEmpty(&packetBufCb) == RING_BUFFER_NOT_EMPTY) {
    packetLen = RingBuffer_Peek(&packetBufCb, 0);
    if (packetLen == ESC) {
      if (RingBuffer_BytesUsed(&packetBufCb) > 1) {
        RingBuffer_Read(&packetBufCb);
      } else {
        return State_WaitingForLength;
      }
    }
    packetLen = RingBuffer_Read(&packetBufCb);
    if (packetLen < sizeof(packetBuf)-2) {
      bufIndex = 0;
      payloadLen = 0;
      msgType = 0;
      dataType = 0;
      packetIndex = 0;
      DebugUart_UartPutString("Got length!\r\n");
      return State_WaitingForPacket;
    } else {
      DebugUart_UartPutString("Length is too long, aborting.\r\n");
      return State_WaitingForStx;
    }
  }
  
  return State_WaitingForLength;
}
  
static uint8_t StateHandler_WaitingForPacket(void) {
  uint8_t bytesUsed;
  uint8_t b;
  ReadFromSerialPort();
  
  bytesUsed = RingBuffer_BytesUsed(&packetBufCb);
  while (bytesUsed > packetIndex) {
    if (RingBuffer_Peek(&packetBufCb, packetIndex) == ESC) {
      if ((bytesUsed - packetIndex) > 1) {
        packetIndex++;
      } else {
        return State_WaitingForPacket;
      }
    }
    b = RingBuffer_Peek(&packetBufCb, packetIndex++);
    recvBuf[bufIndex++] =  b;
    DebugUart_UartPutString("Got a byte: ");
    printU8(b);
    DebugUart_UartPutString("\r\n");
    if (bufIndex >= packetLen + 2) {
      CheckPacket();
      return State_WaitingForStx;
    }
  }
  
  return State_WaitingForPacket;
}

typedef uint8_t (*StateHandler_fp)(void);
// Array of state handlers
static const StateHandler_fp StateHandlers[] = {
  StateHandler_WaitingForStx,
  StateHandler_WaitingForLength,
  StateHandler_WaitingForPacket,
  NULL
};

static uint8_t currentState = State_WaitingForStx;

static void loop(void) {
  if (currentState < State_Invalid) {
    if(StateHandlers[currentState] != NULL) {
      currentState = StateHandlers[currentState]();
    } 
  }
}

static void storeCallbackEntry(unsigned char sym, unsigned char typ, chillhubCallbackFunction fcn) {
  uint8_t index = getIndexOfCallback(sym, typ);
  
  // Does this exist already?
  if (index == NO_CALLBACK) {
    // not found, store 
    DebugUart_UartPutString("Storing a new callback entry.\r\n");
    index = getUnusedIndexFromCallbackTable();
  } else {
    DebugUart_UartPutString("Revising an existing callback entry.\r\n");
  }
  
  if (index != NO_CALLBACK) {
    callbackTable[index].callback = fcn;
    callbackTable[index].inUse = TRUE;
    callbackTable[index].symbol = sym;
    callbackTable[index].type = typ;
    DebugUart_UartPutString("Callback added.\r\n");      
  } else {
    DebugUart_UartPutString("No room left in callback table.\r\n");
  }
} 

static chillhubCallbackFunction callbackLookup(unsigned char sym, unsigned char typ) {
  uint8_t index;
  
  index = getIndexOfCallback(sym, typ);
  if (index != NO_CALLBACK) {
    return callbackTable[index].callback;
  } else {
    return NULL;
  }
}

static uint8_t getIndexOfCallback(unsigned char sym, unsigned char typ) {
  uint8_t index = NO_CALLBACK;
  uint8_t i;
  
  for(i=0; i<MAX_CALLBACKS; i++) {
    if (callbackTable[i].inUse == TRUE) {
      if ((callbackTable[i].type == typ) && (callbackTable[i].symbol == sym)) {
        index = i;
        break;
      }
    }
  }
  
  return index;
}

static uint8_t getUnusedIndexFromCallbackTable(void) {
  uint8_t index = NO_CALLBACK;
  uint8_t i;
  
  for(i=0; i<MAX_CALLBACKS; i++) {
    if (callbackTable[i].inUse == FALSE) {
      index = i;
      break;
    }
  }
  
  return index;
}

static void callbackRemove(unsigned char sym, unsigned char typ) {
  uint8_t index = getIndexOfCallback(sym, typ);
  
  if (index != NO_CALLBACK) {
    callbackTable[index].inUse = FALSE;
  }
}

static uint8_t isControlChar(uint8_t c) {
  switch(c) {
    case STX:
      return 1;
    case ESC:
      return 1;
    default:
      return 0;
  }
}

static void outputChar(uint8_t c) {
  uint8_t buf[2];
  uint8_t index=0;

  if (isControlChar(c)) {
    buf[index++] = ESC;
  }
  buf[index++] = c;
  
  Serial->write(buf, index);
}
     
static void sendPacket(uint8_t *pBuf, uint8_t len){
  uint16_t checksum = 42;
  uint8_t buf[1];
  uint8_t i;
  
  // send STX
  buf[0] = STX;
  Serial->write(buf, 1);
  // send packet length
  outputChar(len);
  
  // send packet
  for(i=0; i<len; i++) {
    checksum += pBuf[i];
    outputChar(pBuf[i]);
  }
  
  // send CS
  outputChar(MSB_OF_U16(checksum));
  outputChar(LSB_OF_U16(checksum));
}
