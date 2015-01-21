
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
static const T_CalValues calValues =
{
  {0,0,0},
  {2048,2048,2048}
};

enum E_CalibrationSelection {
  calibrateEmpty = 1,
  calibrateFull = 2
};

// This needs to come from the EEPROM
const char UUID[] = "1ea8fdb9-2418-440b-a67b-fa16210f0c9e";
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
static void readMilkWeight(unsigned char doorStatus);
static void applyFsrCurve(uint32_t *pScaledVal, uint32_t *pRawValue);
static void readFromSensors(uint32_t *paMeas);
static void storeLimits(void);
static void factoryCalibrate(uint8_t full);
static uint32_t getMilkWeight(void);
static uint32_t calculateMilkWeight(uint32_t *pSensorReadings);
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
  
  // load FSR limits here
  for (int j = 0; j < 3; j++) {
    LO_MEAS[j] = calValues.LO_MEAS[j];
    HI_MEAS[j] = calValues.HI_MEAS[j];
  }
}

void deviceAnnounce() { 
  DebugUart_UartPutString("\r\nRegistering with the chillhub.\r\n");
  
  // register the name (type) of this device with the chillhub
  ChillHub.setup(deviceType, UUID, &uartInterface);

  // add a listener for device ID request type
  ChillHub.subscribe(deviceIdRequestType, (chillhubCallbackFunction)deviceAnnounce);
  
  // subscribe to door messages to trigger 
  ChillHub.subscribe(doorStatusMsgType, (chillhubCallbackFunction)readMilkWeight);
  
  // setup factory calibration listener and create cloud resource
  ChillHub.addCloudListener(calibrateID, (chillhubCallbackFunction)factoryCalibrate);
  ChillHub.createCloudResourceU16("calibrate", calibrateID, 1, 0);
  
  // Create cloud resource for weight
  ChillHub.createCloudResourceU16("weight", weightID, FALSE, 0);
  
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
  uint32_t sensorReadings[3];
  uint32_t weight;
  
	CyGlobalIntDisable;
	ticksCopy = ticks;
	CyGlobalIntEnable;
		
	if ((ticksCopy-oldTicks) >= (500))
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

int main()
{
  hardwareSetup();
  
  DebugUart_UartPutString("\r\n");
  DebugUart_UartPutString("************************\r\n");
  DebugUart_UartPutString("* Milky Weigh Starting *\r\n");
  DebugUart_UartPutString("************************\r\n");
  
  CyGlobalIntEnable; /* Uncomment this line to enable global interrupts. */

	deviceAnnounce();
	
	DebugUart_UartPutString("\r\nMain program running...\r\n");
	
	LED_Write(0);
	
	for(;;)
	{
		ChillHub.loop();
    
    checkForReset();
    periodicPrintOfWeight();
  }
}

static uint32 calculateMilkWeight(uint32_t *pSensorReadings) {
  uint32_t sensorWeights[3];

  applyFsrCurve(sensorWeights, pSensorReadings);
  
  uint32_t weight = sensorWeights[0] + 
    sensorWeights[1] + 
    sensorWeights[2];
    
  return weight;
}

static uint32_t getMilkWeight(void) {
  uint32_t sensorReadings[3];

  readFromSensors(sensorReadings);
  uint32_t weight = calculateMilkWeight(sensorReadings);

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

static void readMilkWeight(unsigned char doorStatus) {
  uint8_t doorNowOpen = (doorStatus & 0x01);
  
  if (doorWasOpen && !doorNowOpen) {
    
    sendWeight(getMilkWeight());
  }
  doorWasOpen = doorNowOpen;
}

static void applyFsrCurve(uint32_t *pScaledVal, uint32_t *pRawValue) {
  for (int j = 0; j < 3; j++) {
    pScaledVal[j] = (pRawValue[j] - LO_MEAS[j]) * W_MAX[j] / (HI_MEAS[j] - LO_MEAS[j]);
  }
}

static void readFromSensors(uint32_t *paMeas) {
  int16_t meas;
  
  meas = ADC_GetResult16(WeightA_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[0] = (uint32_t)((int32_t)meas);
  
  meas = ADC_GetResult16(WeightB_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[1] = (uint32_t)((int32_t)meas);
  
  meas = ADC_GetResult16(WeightC_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[2] = (uint32_t)((int32_t)(uint16_t)meas);  
}

static void storeLimits(void) {
  T_CalValues limits;
  for (int j = 0; j < 3; j++) {
    limits.LO_MEAS[j] = LO_MEAS[j];
    limits.HI_MEAS[j] = HI_MEAS[j];
  }
  
  EmNvMem_Write((const uint8_t*)&limits, (const uint8_t*)&calValues, 
    sizeof(T_CalValues));
}

static void factoryCalibrate(uint8_t which) {
  uint32_t sensorReadings[3];
  uint32_t *pMeas;
  
  DebugUart_UartPutString("Got a factory calibrate message.\r\n");
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
      // do nothing
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
