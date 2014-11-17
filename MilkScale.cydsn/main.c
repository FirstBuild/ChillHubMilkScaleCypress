
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
uint16_t LO_MEAS[3] = { 0, 0, 0 };
uint16_t HI_MEAS[3] = { 2048, 2048, 2048 };

// This is the EEPROM storage for the calibration values.
typedef struct T_CalValues {
  uint16_t LO_MEAS[3];
  uint16_t HI_MEAS[3];
} T_CalValues;
static const T_CalValues calValues =
{
  {0,0,0},
  {2048,2048,2048}
};

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
uint16_t W_MAX[3] = {18908, 12507, 28585};

// Internal function prototypes
static void readMilkWeight(unsigned char doorStatus);
static void applyFsrCurve(uint16_t *pScaledVal, uint16_t *pRawValue);
static void readFromSensors(uint16_t *paMeas);
static void storeLimits(void);
static void factoryCalibrate(uint8_t full);
static uint16_t getMilkWeight(void);
static uint16 calculateMilkWeight(uint16_t *pSensorReadings);
static void checkForReset(void);
//static uint16_t doSensorRead(unsigned char pinNumber);

// Timer interrupt for the time base.
CY_ISR(isr_timer_interrupt) {
    time_base_ClearInterrupt(time_base_INTR_MASK_TC | time_base_INTR_MASK_CC_MATCH);
    
    ticks++;
}

// Comm interface for the chillhub "object".
// This is kinda stupid.  Do something different here.
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
}

void setup() {
  // register the name (type) of this device with the chillhub
  ChillHub.setup("milkscale", 9, &uartInterface);

  // load FSR limits here
  for (int j = 0; j < 3; j++) {
    LO_MEAS[j] = calValues.LO_MEAS[j];
    HI_MEAS[j] = calValues.HI_MEAS[j];
  }
  
  // subscribe to door messages to trigger 
  ChillHub.subscribe(doorStatusMsgType, (chillhubCallbackFunction)readMilkWeight);
  
  // setup factory calibration listener
  ChillHub.addCloudListener(0x94, (chillhubCallbackFunction)factoryCalibrate);
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

void periodicPrintOfWeight(void) {
  uint32 ticksCopy;
  static uint32 oldTicks=0;
  uint16_t sensorReadings[3];
	
	CyGlobalIntDisable;
	ticksCopy = ticks;
	CyGlobalIntEnable;
		
	if ((ticksCopy-oldTicks) >= (1000 * 6))
	{
    oldTicks = ticksCopy;
    readFromSensors(sensorReadings);
    
    DebugUart_UartPutString("Sensor A: ");
    printU16(sensorReadings[0]);
    DebugUart_UartPutString("\r\n");
    DebugUart_UartPutString("Sensor B: ");
    printU16(sensorReadings[1]);
    DebugUart_UartPutString("\r\n");
    DebugUart_UartPutString("Sensor C: ");
    printU16(sensorReadings[2]);
    DebugUart_UartPutString("\r\n");
    
    DebugUart_UartPutString("Milk weight: ");
    printU16(calculateMilkWeight(sensorReadings));
    DebugUart_UartPutString("\r\n");
  }
}

int main()
{
  hardwareSetup();

	setup();
	
	DebugUart_UartPutString("\r\nMain program running...\r\n");
	
	LED_Write(0);
	
	CyGlobalIntEnable; /* Uncomment this line to enable global interrupts. */
	for(;;)
	{
		ChillHub.loop();
    
    checkForReset();
    periodicPrintOfWeight();
  }
}

static uint16 calculateMilkWeight(uint16_t *pSensorReadings) {
  uint16_t sensorWeights[3];

  applyFsrCurve(sensorWeights, pSensorReadings);
  
  uint16_t weight = sensorWeights[0] + 
    sensorWeights[1] + 
    sensorWeights[2];
    
  return weight;
}

static uint16_t getMilkWeight(void) {
  uint16_t sensorReadings[3];

  readFromSensors(sensorReadings);
  uint16_t weight = calculateMilkWeight(sensorReadings);

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
    
    ChillHub.sendU16Msg(0x51, getMilkWeight()/600);
  }
  doorWasOpen = doorNowOpen;
}

static void applyFsrCurve(uint16_t *pScaledVal, uint16_t *pRawValue) {
  for (int j = 0; j < 3; j++) {
    pScaledVal[j] = (pRawValue[j] - LO_MEAS[j]) * 1.0 * W_MAX[j] / (HI_MEAS[j] - LO_MEAS[j]);
  }
}

static void readFromSensors(uint16_t *paMeas) {
  int16_t meas;
  
  meas = ADC_GetResult16(WeightA_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[0] = (uint16_t)meas;
  
  meas = ADC_GetResult16(WeightB_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[1] = (uint16_t)meas;
  
  meas = ADC_GetResult16(WeightC_Channel);
  if (meas < 0) {meas = 0;}
  paMeas[2] = (uint16_t)meas;  
}

#ifdef KILL
// Not sure what is going on here.
// Can we just average over a bunch of readings or do
// we need to wait until the sinusoid is at peak before
// reading?
static uint16_t doSensorRead(unsigned char pinNumber) {
    uint16_t newReading = 0;
    uint16_t anaVal;
    // oversample to get the max amplitude
    for (int j = 0; j < 5000; j++) {
      anaVal = analogRead(pinNumber);
      newReading = max(newReading,anaVal);
    }
    return newReading;
}
#endif

static void storeLimits(void) {
  T_CalValues limits;
  for (int j = 0; j < 3; j++) {
    limits.LO_MEAS[j] = LO_MEAS[j];
    limits.HI_MEAS[j] = HI_MEAS[j];
  }
  
  EmNvMem_Write((const uint8_t*)&limits, (const uint8_t*)&calValues, 
    sizeof(T_CalValues));
}

static void factoryCalibrate(uint8_t full) {
  uint16_t sensorReadings[3];
  readFromSensors(sensorReadings);
  
  if (full) {
    for (int j = 0; j < 3; j++)
      HI_MEAS[j] = sensorReadings[j];
  }
  else {
    for (int j = 0; j < 3; j++)
      LO_MEAS[j] = sensorReadings[j];
  }
  
  storeLimits();
}


/* [] END OF FILE */
