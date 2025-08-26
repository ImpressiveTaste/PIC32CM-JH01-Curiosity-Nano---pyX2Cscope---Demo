/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <string.h>                     // For string manipulation functions
#include <stdio.h>                      // For standard input/output functions

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true and false
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes

// Define the I2C address for the temperature sensor
#define TEMP_SENSOR_SLAVE_ADDR                  0x004F
// Define the register address for the temperature sensor
#define TEMP_SENSOR_REG_ADDR                    0x00

/* RTC Time period match values for input clock of 1 KHz */
#define PERIOD_500MS                            512 // 0x200 in hexadecimal (default value in MCC)
#define PERIOD_1S                               1024
#define PERIOD_2S                               2048
#define PERIOD_4S                               4096

// Enumeration for temperature sampling rates
typedef enum
{
    TEMP_SAMPLING_RATE_500MS = 0,
    TEMP_SAMPLING_RATE_1S = 1,
    TEMP_SAMPLING_RATE_2S = 2,
    TEMP_SAMPLING_RATE_4S = 3,
} TEMP_SAMPLING_RATE;

// Initialize the temperature sampling rate to 500ms
static TEMP_SAMPLING_RATE tempSampleRate = TEMP_SAMPLING_RATE_500MS;

// Define volatile flags for various events
static volatile bool isRTCTimerExpired = false;
static volatile bool changeTempSamplingRate = false;
static volatile bool isUSARTTxComplete = true;
static volatile bool isTemperatureRead = false;

// Define variables for temperature value and I2C data
static uint8_t temperatureVal;
static uint8_t i2cWrData = TEMP_SENSOR_REG_ADDR;
static uint8_t i2cRdData[2] = {0};
static uint8_t uartTxBuffer[100] = {0};

uint8_t TemperatureValueX2C=0;

// Function to convert raw temperature value to readable format (Degree Celsius)
static uint8_t getTemperature(uint8_t* rawTempValue)
{
    int16_t temp;
    // Convert the temperature value read from sensor to readable format (Degree Celsius)
    // For demonstration purpose, temperature value is assumed to be positive.
    // The maximum positive temperature measured by sensor is +125 C
    temp = (rawTempValue[0] << 8) | rawTempValue[1];
    temp = (temp >> 7) * 0.5; // Celsius
    // temp = (temp * 9/5) + 32; // Fahrenheit
    return (uint8_t)temp;
}

// Interrupt handler for external interrupt controller
static void EIC_User_Handler(uintptr_t context)
{
    changeTempSamplingRate = true;      
}

// RTC event handler
static void rtcEventHandler (RTC_TIMER32_INT_MASK intCause, uintptr_t context)
{
    if (intCause & RTC_MODE0_INTENSET_CMP0_Msk)
    {            
        isRTCTimerExpired = true;                              
    }
}

// I2C event handler
static void i2cEventHandler(uintptr_t contextHandle)
{
    if (SERCOM2_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE)
    {
        isTemperatureRead = true;
    }
}

// USART DMA channel handler
static void usartDmaChannelHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    if (event == DMAC_TRANSFER_EVENT_COMPLETE)
    {
        isUSARTTxComplete = true;
    }
}

// 1ms callback for the X2C update
static void TC0_Callback_InterruptHandler(TC_TIMER_STATUS status, uintptr_t context)
{
        // Keep this fast; just push samples to the X2Cscope buffer
        X2Cscope_Update();
}

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    // Register callback functions for I2C, DMA, RTC, and EIC
    SERCOM2_I2C_CallbackRegister(i2cEventHandler, 0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, usartDmaChannelHandler, 0);
    RTC_Timer32CallbackRegister(rtcEventHandler, 0);
    EIC_CallbackRegister(EIC_PIN_15,EIC_User_Handler, 0);
    
    /* Register callback function for TC3 period interrupt */
    TC0_TimerCallbackRegister(TC0_Callback_InterruptHandler, (uintptr_t)NULL);

    /* Start the timer*/
    TC0_TimerStart();

    // Print start message
    sprintf((char*)uartTxBuffer, "Start Of Program \r\n");
    // Start the RTC timer
    RTC_Timer32Start();

    while ( true )
    {
        X2Cscope_Communicate();
        // Check if RTC timer has expired
        if (isRTCTimerExpired == true)
        {
            isRTCTimerExpired = false;
            // Initiate I2C read for temperature sensor
            SERCOM2_I2C_WriteRead(TEMP_SENSOR_SLAVE_ADDR, &i2cWrData, 1, i2cRdData, 2);
        }
        // Check if temperature read is complete
        if (isTemperatureRead == true)
        {
            isTemperatureRead = false;
            if(changeTempSamplingRate == false)
            {
                // Get the temperature value and print it
                temperatureVal = getTemperature(i2cRdData);
                TemperatureValueX2C = temperatureVal;
                sprintf((char*)uartTxBuffer, "Temperature = %02d C\r\n", temperatureVal);
                // Toggle LED1
                LED1_Toggle();
            }
            else
            {
                changeTempSamplingRate = false;
                // Change the temperature sampling rate based on current rate
                if(tempSampleRate == TEMP_SAMPLING_RATE_500MS)
                {
                    tempSampleRate = TEMP_SAMPLING_RATE_1S;
                    sprintf((char*)uartTxBuffer, "Sampling Temperature every 1 second \r\n");
                    RTC_Timer32CompareSet(PERIOD_1S);
                }
                else if(tempSampleRate == TEMP_SAMPLING_RATE_1S)
                {
                    tempSampleRate = TEMP_SAMPLING_RATE_2S;
                    sprintf((char*)uartTxBuffer, "Sampling Temperature every 2 seconds \r\n");        
                    RTC_Timer32CompareSet(PERIOD_2S);                        
                }
                else if(tempSampleRate == TEMP_SAMPLING_RATE_2S)
                {
                    tempSampleRate = TEMP_SAMPLING_RATE_4S;
                    sprintf((char*)uartTxBuffer, "Sampling Temperature every 4 seconds \r\n");        
                    RTC_Timer32CompareSet(PERIOD_4S);                                        
                }    
                else if(tempSampleRate == TEMP_SAMPLING_RATE_4S)
                {
                    tempSampleRate = TEMP_SAMPLING_RATE_500MS;
                    sprintf((char*)uartTxBuffer, "Sampling Temperature every 500 ms \r\n");        
                    RTC_Timer32CompareSet(PERIOD_500MS);
                }
                else
                {
                    ;
                }
            }
            // Initiate DMA transfer for UART transmission
            DMAC_ChannelTransfer(DMAC_CHANNEL_0, uartTxBuffer, \
                    (const void *)&(SERCOM1_REGS->USART_INT.SERCOM_DATA), \
                    strlen((const char*)uartTxBuffer));
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/
