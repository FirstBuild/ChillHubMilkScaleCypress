
/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include <project.h>
#include <string.h>
#include "time_base.h"
#include "LED.h"
#include "Uart.h"
#include "chillhub.h"
#include "DebugUart.h"
#include "crc.h"

uint32 ticks = 0;
uint8_t buttonWasPressed = 0;

uint16_t doorCounts = 0;

#ifndef FALSE
  #define FALSE 0
  #define TRUE !FALSE
#endif

#define FULL_WEIGHT 60000
#define EMPTY_WEIGHT 0
#define DIFF_THRESHOLD 1200  // 2%

uint8_t doorWasOpen = FALSE;
uint32_t LO_MEAS[3] = { 0, 0, 0 };
uint32_t HI_MEAS[3] = { 2048, 2048, 2048 };

// This is the EEPROM storage for the calibration values.
typedef struct T_CalValues {
  uint32_t LO_MEAS[3];
  uint32_t HI_MEAS[3];
} T_CalValues;

#define MAX_UUID_LENGTH 48

typedef struct T_EEPROM {
  T_CalValues calValues;
  char UUID[MAX_UUID_LENGTH+1];
} T_EEPROM;

static const T_EEPROM eeprom __attribute__ ((section (".EEPROMDATA"))) = {
  {
    {0,0,0},
    {600,1500,600}
  },
  "1ea8fdb9-2418-440b-a67b-fa16210f0c9e"
} ;

enum E_CalibrationSelection {
  calibrateEmpty = 1,
  calibrateFull = 2
};

// This is the device type reported to the chill hub.
const char deviceType[] = "milkyWeighs";

/* 
 * we find the full weight on each sensor by solving the statics problem:
 *
 * W1 + W2 + W3 = W
 * 2.374*W1 + 1.837*W2 - 2.374*W3 = 0  (sum Mx = 0)
 * 2.285*W1 - 2.323*W2 - 0.495*W3 = 0  (sum My = 0)
 *
 * This results in full W1 is 31.51%, W2 is 20.85%, and W3 is 47.64%
 * If we arbitrarily say we want the full weight to be 60000 (near limit of U16), then:
 */
//uint16_t W_MAX[3] = {18908, 12507, 28585};
uint32_t W_MAX[3] = {15016, 30000, 14984};

// Internal function prototypes
static void readMilkWeight(uint8_t dataType, void *pData);
static void applyFsrCurve(int32_t *pScaledVal, int32_t *pRawValue);
static void readFromSensors(int32_t *paMeas);
static void storeLimits(void);
static void factoryCalibrate(uint8_t dataType, void *pData);
static int32_t getMilkWeight(void);
static int32_t calculateMilkWeight(int32_t *pSensorReadings);
static void checkForReset(void);
//static uint16_t doSensorRead(unsigned char pinNumber);

// Timer interrupt for the time base.
CY_ISR(isr_timer_interrupt) {
    time_base_ClearInterrupt(time_base_INTR_MASK_TC | time_base_INTR_MASK_CC_MATCH);
    
    ticks++;
}

static const T_Serial uartInterface = {
    .write = Uart_SpiUartPutArray,
    .available = Uart_SpiUartGetRxBufferSize,
    .read = Uart_SpiUartReadRxData,
    .print = Uart_UartPutString
};

enum {
  WeightA_Channel,
  WeightB_Channel,
  WeightC_Channel,
};

typedef enum cloudResorceId {
  weightID = 0x91,
  calibrateID = 0x94
} T_cloudResourceId;

void setDeviceUUID(uint8_t dataType, void *pData) {
  (void)dataType;
  char *pUUID = (char*)pData;
  uint8_t len = (uint8_t)pUUID[0];
  char *pStr = &pUUID[1];
  
  if (len <= MAX_UUID_LENGTH) {
    // add null terminator
    pStr[len] = 0;
    EmNvMem_Write((const uint8_t *)pStr, (const uint8_t*)&eeprom.UUID, len+1);
    DebugUart_UartPutString("New UUID written to device.\r\n");
  } else {
    DebugUart_UartPutString("Can't write UUID, it is too long.\r\n");
  }
}

static void hardwareSetup(void) {
  isr_timer_StartEx(isr_timer_interrupt);
  time_base_Start();

  Uart_Start();
  DebugUart_Start();
  SineSource_Start();
  Opamp_Start();
  ADC_Start();
  ADC_StartConvert();  
  SampleStartDelay_Start();
  UsbChipReset_Write(0);
  
  // load FSR limits here
  for (int j = 0; j < 3; j++) {
    LO_MEAS[j] = eeprom.calValues.LO_MEAS[j];
    HI_MEAS[j] = eeprom.calValues.HI_MEAS[j];
  }
  
}

static uint32_t keepAliveCheckTimer = 0;

void operateUsbReset(void) {
  uint32 ticksCopy;
  static uint32 resetStartTicks=0;
	
	CyGlobalIntDisable;
	ticksCopy = ticks;
	CyGlobalIntEnable;
  
  // Anything received in 10 seconds?
	if ((ticksCopy-keepAliveCheckTimer) >= 20000)
	{
    DebugUart_UartPutString("No chillhub message received, resetting USB.\r\n");
    // no, reset the USB
    UsbChipReset_Write(0);
    // Start the reset pin timer
    resetStartTicks = ticksCopy;
  }
		
	if ((ticksCopy-resetStartTicks) >= 500)
	{
    DebugUart_UartPutString("USB reset complete.\r\n");
    UsbChipReset_Write(1);
  }
  
  if (UsbChipReset_Read() == 1) {
    resetStartTicks = ticksCopy;
  } else {
    keepAliveCheckTimer = ticksCopy;
  }
}

void keepaliveCallback(uint8_t dataType, void *pData) {
  (void)dataType;
  (void)pData;
  
  DebugUart_UartPutString("Keepalive message received from chillhub.\r\n");
  
	CyGlobalIntDisable;
	keepAliveCheckTimer = ticks;
	CyGlobalIntEnable;
}

void deviceAnnounce(uint8_t dataType, void *pData) { 
  (void)dataType;
  (void)pData;
  
  DebugUart_UartPutString("\r\nRegistering with the chillhub.\r\n");
  
  // register the name (type) of this device with the chillhub
  ChillHub.setup(deviceType, eeprom.UUID, &uartInterface);

  // add a listener for device ID request type
  ChillHub.subscribe(deviceIdRequestType, deviceAnnounce);

  // add a listener for keepalive from chillhub
  ChillHub.subscribe(keepAliveType, keepaliveCallback);

  // subscribe to door messages to trigger 
  ChillHub.subscribe(doorStatusMsgType, readMilkWeight);
  
  // setup factory calibration listener and create cloud resource
  DebugUart_UartPutString("Address of factory calibrate: ");
  printU32((uint32_t)&factoryCalibrate);
  DebugUart_UartPutString("\r\n");
  ChillHub.addCloudListener(calibrateID, &factoryCalibrate);
  ChillHub.createCloudResourceU16("calibrate", calibrateID, 1, 0);
  
  DebugUart_UartPutString("Address of checkForReset: ");
  printU32(((uint32_t)(checkForReset)));
  DebugUart_UartPutString("\r\n");

  DebugUart_UartPutString("Size of void*: ");
  printU32(sizeof(void*));
  DebugUart_UartPutString("\r\n");

  
  // Create cloud resource for weight
  ChillHub.createCloudResourceU16("weight", weightID, FALSE, 0);
  
  // add a listener for setting the UUID of the device
  ChillHub.subscribe(setDeviceUUIDType, setDeviceUUID);

  DebugUart_UartPutString("Registration complete.\r\n");

  for (uint8_t j = 0; j < 3; j++) {
    DebugUart_UartPutString("Low ");
    printU8(j);
    DebugUart_UartPutString(": ");
    printU32(LO_MEAS[j]);
    DebugUart_UartPutString("\r\n");
    DebugUart_UartPutString("Hi  ");
    printU8(j);
    DebugUart_UartPutString(": ");
    printU32(HI_MEAS[j]);
    DebugUart_UartPutString("\r\n");
  }
}

static void checkForReset(void) {
  uint32 ticksCopy;
  static uint32 oldTicks=0;
	
	CyGlobalIntDisable;
	ticksCopy = ticks;
	CyGlobalIntEnable;
		
	if ((ticksCopy-oldTicks) >= 1000)
	{
    oldTicks = ticksCopy;
		if (UserButton_Read() == 0) {
				if (buttonWasPressed < 5) {
					buttonWasPressed++;
				}
		} else {
			if (buttonWasPressed > 0) {
				buttonWasPressed--;
			}
		}	
	}
	
	if (UserButton_Read() == 0) {
		if (buttonWasPressed < 5) {
  		LED_Write(1);            
	  } else {
		  LED_Write(0);
	  }
  } else {
	  if (buttonWasPressed >= 5) {
		  // reset
		  CySoftwareReset();
	  }
	  LED_Write(0);
  }
}

static void sendWeight(uint32_t weight) {
  uint16_t percent = (uint16_t)(weight/600);
  
  if (percent > 100) {
    percent = 100;
  }
  DebugUart_UartPutString("Updating weight: ");
  printU16(percent);
  DebugUart_UartPutString("\r\n");
  
  ChillHub.updateCloudResourceU16(weightID, percent);
}

void periodicPrintOfWeight(void) {
  uint32 ticksCopy;
  static uint32 oldTicks=0;
  int32_t sensorReadings[3];
  int32_t weight;
  
	CyGlobalIntDisable;
	ticksCopy = ticks;
	CyGlobalIntEnable;
		
	if ((ticksCopy-oldTicks) >= (2000))
	{
    oldTicks = ticksCopy;
    
    readFromSensors(sensorReadings);
    
    DebugUart_UartPutString("Sensor A: ");
    printU16((uint16_t)sensorReadings[0]);
    DebugUart_UartPutString("\r\n");
    DebugUart_UartPutString("Sensor B: ");
    printU16((uint16_t)sensorReadings[1]);
    DebugUart_UartPutString("\r\n");
    DebugUart_UartPutString("Sensor C: ");
    printU16((uint16_t)sensorReadings[2]);
    DebugUart_UartPutString("\r\n");
    
    DebugUart_UartPutString("Milk weight: ");
    weight = calculateMilkWeight(sensorReadings);
    printU32(weight);
    DebugUart_UartPutString("\r\n");
    
    sendWeight(weight);
  }
}

void delayMS(uint32 waitTicks) 
{
  uint32 ticksCopy;
  uint32 oldTicks=0;
	
	CyGlobalIntDisable;
	ticksCopy = ticks;
	CyGlobalIntEnable;
  oldTicks = ticksCopy;
		
	while ((ticksCopy-oldTicks) <= waitTicks)
	{
  	CyGlobalIntDisable;
  	ticksCopy = ticks;
  	CyGlobalIntEnable;
	}
}

int main()
{
  hardwareSetup();
  
  DebugUart_UartPutString("\r\n");
  DebugUart_UartPutString("************************\r\n");
  DebugUart_UartPutString("* Milky Weigh Starting *\r\n");
  DebugUart_UartPutString("************************\r\n");
  
  CyGlobalIntEnable; /* Uncomment this line to enable global interrupts. */

	deviceAnnounce(42, NULL);
	
	DebugUart_UartPutString("\r\nMain program running...\r\n");
	
	LED_Write(0);
  
	for(;;)
	{
		ChillHub.loop();
    
    checkForReset();
    periodicPrintOfWeight();
    operateUsbReset();
  }
}

static int32_t calculateMilkWeight(int32_t *pSensorReadings) {
  int32_t sensorWeights[3];

  applyFsrCurve(sensorWeights, pSensorReadings);
  
  int32_t weight = sensorWeights[0] + 
    sensorWeights[1] + 
    sensorWeights[2];
  
  if (weight < 0) {
      weight = 0;
  }
    
  return weight;
}

static int32_t getMilkWeight(void) {
  int32_t sensorReadings[3];

  readFromSensors(sensorReadings);
  int32_t weight = calculateMilkWeight(sensorReadings);

  if (weight >= (FULL_WEIGHT - DIFF_THRESHOLD)) {
    for (int j = 0; j < 3; j++)
      HI_MEAS[j] = sensorReadings[j];
    storeLimits();
  }
  else if (weight < (EMPTY_WEIGHT + DIFF_THRESHOLD)) {
    for (int j = 0; j < 3; j++)
      LO_MEAS[j] = sensorReadings[j];
    storeLimits();
  }
  
  return weight;
}

static void readMilkWeight(uint8_t dataType, void *pData) {
  (void)dataType;
  unsigned char doorStatus = *(uint32_t*)pData;
  uint8_t doorNowOpen = (doorStatus & 0x01);
  
  if (doorWasOpen && !doorNowOpen) {
    
    sendWeight(getMilkWeight());
  }
  doorWasOpen = doorNowOpen;
}

static void applyFsrCurve(int32_t *pScaledVal, int32_t *pRawValue) {
  for (int j = 0; j < 3; j++) {
    if (pRawValue[j] >= (int32_t)LO_MEAS[j]) {      
      pScaledVal[j] = (pRawValue[j] - LO_MEAS[j]) * W_MAX[j] / (HI_MEAS[j] - LO_MEAS[j]);
    } else {
      pScaledVal[j] = 0;
    }
  }
}

static void readFromSensors(int32_t *paMeas) {
  int16_t meas;
  
  meas = ADC_GetResult16(WeightA_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[0] = (int32_t)meas;
  
  meas = ADC_GetResult16(WeightB_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[1] = (int32_t)meas;
  
  meas = ADC_GetResult16(WeightC_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[2] = (int32_t)meas;  
}

static void storeLimits(void) {
  T_CalValues limits;
  for (int j = 0; j < 3; j++) {
    limits.LO_MEAS[j] = LO_MEAS[j];
    limits.HI_MEAS[j] = HI_MEAS[j];
  }
  
  EmNvMem_Write((const uint8_t*)&limits, (const uint8_t*)&eeprom.calValues, 
    sizeof(T_CalValues));
}

static void factoryCalibrate(uint8_t dataType, void *pData) {
  (void)dataType;
  int32_t sensorReadings[3];
  uint32_t *pMeas;
  uint8_t *pU8Data = pData;
  uint32_t which=0;

  DebugUart_UartPutString("Got a factory calibrate message.\r\n");
  
  switch(dataType) {
    case unsigned32DataType:
      which = (pU8Data[0] << 24 ) +
              (pU8Data[1] << 16 ) +
              (pU8Data[2] << 8 ) +
               pU8Data[3];
      break;
     default:
        DebugUart_UartPutString("Did not receive a U32.\r\n");
        break;
  }
    
  DebugUart_UartPutString("Value is: ");
  printU8(which);
  DebugUart_UartPutString("\r\n");
  
  switch(which) {
    case calibrateEmpty:
      // calibrate low end
      pMeas = &LO_MEAS[0];
      break;
     
    case calibrateFull:
      // calibrate full scale
      pMeas = &HI_MEAS[0];
      break;
    
    default:
      // illegal value, reset to 0
      ChillHub.updateCloudResourceU16(calibrateID, 0);
      return;
  }
  
  readFromSensors(sensorReadings);
    
  for (int j = 0; j < 3; j++) {
    *pMeas++ = sensorReadings[j];
  }
  
  storeLimits();
  
  ChillHub.updateCloudResourceU16(calibrateID, 0);

}


/* [] END OF FILE */
