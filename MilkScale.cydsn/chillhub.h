#include <cytypes.h>
#include <stdint.h>

#ifndef CHILLHUB_H
#define CHILLHUB_H
  
#ifndef FALSE
  #define FALSE 0
#endif
#ifndef TRUE
  #define TRUE !FALSE
#endif

#define CHILLHUB_CB_TYPE_FRIDGE 0
#define CHILLHUB_CB_TYPE_CRON 1
#define CHILLHUB_CB_TYPE_TIME 2
#define CHILLHUB_CB_TYPE_CLOUD 3

typedef void (*chillhubCallbackFunction)(uint32_t val);

void printU16(uint16_t val);
void printI16(int16_t val);
void printU8(uint8_t val);
void printU32(uint32_t val);

typedef struct chCbTableType {
  unsigned char symbol;
  unsigned char type;  // 0: fridge data, 1: cron alarm, 2: time, 3: cloud
  uint8_t inUse;
  chillhubCallbackFunction callback;
} chCbTableType;

typedef struct T_Serial {
    void (*write)(const uint8 wrBuf[], uint32 count);
    uint32 (*available)(void);
    uint32 (*read)(void);
    void (*print)(const char8 string[]);
} T_Serial;

typedef void (*chCbFcnU8)(unsigned char);
typedef void (*chCbFcnU16)(unsigned int);
typedef void (*chCbFcnU32)(unsigned long);
typedef void (*chCbFcnTime)(unsigned char[4]);
typedef void (*chCbFcnStr)(char *pStr);
  
/*
 * Function prototypes
 */
typedef struct chInterface {  
  void (*setup)(const char* name, const char *UUID, const T_Serial* serial);
  void (*subscribe)(unsigned char type, chillhubCallbackFunction cb);
  void (*unsubscribe)(unsigned char type);
  void (*setAlarm)(unsigned char ID, char* cronString, unsigned char strLength, chillhubCallbackFunction cb);
  void (*unsetAlarm)(unsigned char ID);
  void (*getTime)(chillhubCallbackFunction cb);
  void (*addCloudListener)(unsigned char msgType, chillhubCallbackFunction cb);
  void (*createCloudResourceU16)(const char *name, uint8_t resId, uint8_t canUpdate, uint16_t initVal);
  void (*updateCloudResourceU16)(uint8_t resID, uint16_t val);
  void (*sendU8Msg)(unsigned char msgType, unsigned char payload);
  void (*sendU16Msg)(unsigned char msgType, unsigned int payload);
  void (*sendI8Msg)(unsigned char msgType, signed char payload);
  void (*sendI16Msg)(unsigned char msgType, signed int payload);
  void (*sendBooleanMsg)(unsigned char msgType, unsigned char payload);
  void (*loop)(void);
} chInterface;

// Chill Hub data types
enum ChillHubDataTypes {
  arrayDataType = 0x01,
  stringDataType = 0x02,
  unsigned8DataType = 0x03,
  signed8DataType = 0x04,
  unsigned16DataType = 0x05,
  signed16DataType = 0x06,
  unsigned32DataType = 0x07,
  signed32DataType = 0x08,
  jsonDataType = 0x09,
  booleanDataType = 0x10
};

// Chill Hub message types
enum ChillHubMsgTypes {
  deviceIdMsgType = 0x00,
  subscribeMsgType = 0x01,
  unsubscribeMsgType = 0x02,
  setAlarmMsgType = 0x03,
  unsetAlarmMsgType = 0x04,
  alarmNotifyMsgType = 0x05,
  getTimeMsgType = 0x06,
  timeResponseMsgType = 0x07,
  deviceIdRequestType = 0x08,
  registerResourceType = 0x09,
  updateResourceType = 0x0a,
  resourceUpdatedType = 0x0b,
  setDeviceUUIDType = 0x0c,
  // 0x0d-0x0F Reserved for Future Use
  filterAlertMsgType = 0x10,
  waterFilterCalendarTimerMsgType = 0x11,
  waterFilterCalendarPercentUsedMsgType = 0x12,
  waterFilterHoursRemainingMsgType = 0x13,
  waterUsageTimerMsgType = 0x14,
  waterFilterUsageTimePercentUsedMsgType = 0x15,
  waterFilterOuncesRemainingMsgType = 0x16,
  commandFeaturesMsgType = 0x17,
  temperatureAlertMsgType = 0x18,
  freshFoodDisplayTemperatureMsgType = 0x19,
  freezerDisplayTemperatureMsgType = 0x1A,
  freshFoodSetpointTemperatureMsgType = 0x1B,
  freezerSetpointTemperatureMsgType = 0x1C,
  doorAlarmAlertMsgType = 0x1D,
  iceMakerBucketStatusMsgType = 0x1E,
  odorFilterCalendarTimerMsgType = 0x1F,
  odorFilterPercentUsedMsgType = 0x20,
  odorFilterHoursRemainingMsgType = 0x21,
  doorStatusMsgType = 0x22,
  dcSwitchStateMsgType = 0x23,
  acInputStateMsgType = 0x24,
  iceMakerMoldThermistorTemperatureMsgType = 0x25,
  iceCabinetThermistorTemperatureMsgType = 0x26,
  hotWaterThermistor1TemperatureMsgType = 0x27,
  hotWaterThermistor2TemperatureMsgType = 0x28,
  dctSwitchStateMsgType = 0x29,
  relayStatusMsgType = 0x2A,
  ductDoorStatusMsgType = 0x2B,
  iceMakerStateSelectionMsgType = 0x2C,
  iceMakerOperationalStateMsgType = 0x2D
  // 0x2E-0x4F Reserved for Future Use
  // 0x50-0xFF User Defined Messages
};

#define CHILLHUB_RESV_MSG_MAX 0x4F

extern const chInterface ChillHub;

#endif
