/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== main.c ========
 */

/* XDC module Headers */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS module Headers */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/ADC.h>
//#include <ti/drivers/Board.h>
#include <ti/devices/cc13x2_cc26x2/driverlib/cpu.h>

/* Example/Board Header files */
#include "Board.h"
#include "uart_printf.h"
#include "FIFO.h"
#include "util.h"

/*********************************************************************
 * MACROS
 */
#ifdef MOTOR_PWM

#define startMotor1() do { \
                           if (motor1PWM) \
                           PWM_start(motor1PWM); \
                      } while(0)

#define startMotor2() do { \
                           if (motor2PWM) \
                           PWM_start(motor2PWM); \
                      } while(0)

#define stopMotor() do { \
                        if (motor1PWM) { \
                            PWM_stop(motor1PWM); \
                        } \
                        if (motor2PWM) { \
                            PWM_stop(motor2PWM); \
                        } \
                    } while(0)

#else

#define startMotor1() do { \
                           PIN_setOutputValue(motorDRPinHandle, CC26X2R1_JANUS_L1_MOTOR1_PWMPIN0, 1); \
                      } while(0)

#define startMotor2() do { \
                           PIN_setOutputValue(motorDRPinHandle, CC26X2R1_JANUS_L1_MOTOR2_PWMPIN1, 1); \
                      } while(0)

#define stopMotor() do { \
                        PIN_setOutputValue(motorDRPinHandle, CC26X2R1_JANUS_L1_MOTOR1_PWMPIN0, 0); \
                        PIN_setOutputValue(motorDRPinHandle, CC26X2R1_JANUS_L1_MOTOR2_PWMPIN1, 0); \
                    } while(0)

#endif

#define motorSWPreparation() do { \
                                 PIN_setInterrupt(motorSWPinHandle, Board_DIO14_MOTOR_SW1 | PIN_IRQ_DIS); \
                                 PIN_setInterrupt(motorSWPinHandle, Board_DIO15_MOTOR_SW2 | PIN_IRQ_DIS); \
                                 firstTriggeredSW = 0; \
                                 secondTriggeredSW = 0; \
                                 PIN_setInterrupt(motorSWPinHandle, Board_DIO14_MOTOR_SW1 | PIN_IRQ_NEGEDGE); \
                                 PIN_setInterrupt(motorSWPinHandle, Board_DIO15_MOTOR_SW2 | PIN_IRQ_NEGEDGE); \
                             } while(0)

/*********************************************************************
 * CONSTANTS
 */

#define TASKSTACKSIZE   512
#define FIFOSIZE   32

/* 200ms */
#define UI_CLOCK_PERIOD 200

#define CLOSE_TOUCH_CLOCK_PERIOD 35

#define BUZZER_PWM_FREQ     2730

#define DUTY_VOLUME_HIGH     PWM_DUTY_FRACTION_MAX * 0.5
//#define DUTY_VOLUME_HIGH    100
#define DUTY_VOLUME_MEDIUM  60
#define DUTY_VOLUME_LOW     20
#define DUTY_VOLUME_OFF     0

#define MOTOR_PWM_FREQ     10000
#define MOTOR_PWM_DUTY     PWM_DUTY_FRACTION_MAX
#define MOTOR_JAMMED_DETECT_MS 2400

#ifdef OLD_ME
#define MOTOR_DELAY_UNLOCK_STOP_MS 330
#define MOTOR_DELAY_LOCK_STOP_MS 330
#else
#define MOTOR_DELAY_UNLOCK_STOP_MS 420
#define MOTOR_DELAY_LOCK_STOP_MS 420
#endif

#define BEEP_FOREVER -1
#define LED_FLASH_FOREVER -1

#define EVENT_PRESS         0x01
#define EVENT_PRESS_LONG    0x02
#define EVENT_RELEASE       0x04

/* ADC sample count */
#define ADC_SAMPLE_COUNT  (20)

#ifdef OLD_ME
#define MOTOR_DELAY_STOP_TIME 270000 / Clock_tickPeriod   //nice 150000  //250000
#else
#define MOTOR_DELAY_STOP_TIME 150000 / Clock_tickPeriod   //nice 150000  //250000
#endif

#define CYCLE_TEST_VERSION 1
#define FUNCTION_TEST_VERSION 1
/*********************************************************************
 * TYPEDEFS
 */

typedef enum {
    ORIENTATION_NOT_FOUND,
    ORIENTATION_RIGHT,
    ORIENTATION_LEFT,
} LockOrientation;

typedef enum {
    NOT_FOUND = 0,
    LOCK,
    UNLOCK,
} RLCheckResult;

typedef enum {
    UNKNOW_STATE,
    LOCK_STATE,
    UNLOCK_STATE,
} LockState;

typedef enum _lowPowerLedType {
    LOWPOWER_LED_NONE,
    LOWPOWER_LED_ON,
    LOWPOWER_LED_FLASH,
} LowPowerLedType;

typedef enum _setLed {
    LED_FR,
    LED_FG,
    LED_BR,
    LED_BG,
    LED_LOWPOWER,
    LED_ALL_ON,
    LED_ALL_OFF,
} SetLED;

typedef enum _beepType {
    BEEP_NONE,
    BEEP_ON,
    BEEP_SHORT,
} BeepType;

typedef enum _led {
    LED_NONE,
    LED_R,
    LED_G,
} LED;

typedef enum _ledType {
    LED_ON,
    LED_FLASH,
} LedType;

typedef struct _uiMsg {
    LED led[2];  //which led to action; first element: front led, second element: back led
    LedType ledType[2];  //which action for led; first element: front led, second element: back led
    int32_t ledTimes[2]; //the times for the action of led (unit 200ms);  first element: front led, second element: back led
    LowPowerLedType lowPower;//which action for low power led
    int32_t lowPowerTimes; //the times for the action of low power led (unit 200ms)
    BeepType beepType; //which action for buzzer
    int32_t beepTimes; //the times for the action of buzzer (unit 200ms)
} UIMsg;

/*********************************************************************
 * GLOBAL VARIABLES
 */
UInt32 sleepTickCount;

Task_Struct maintaskStruct;
Char maintaskStack[TASKSTACKSIZE];
FIFO_Buf FIFOBuf;
Char FIFOStack[FIFOSIZE];
Char KeyBuffer[FIFOSIZE];

PIN_Config ledPinTable[] = {
    Board_DIO22_FRONT_RLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    Board_DIO21_FRONT_GLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    Board_DIO19_BACK_RLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    Board_DIO18_BACK_GLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    Board_DIO0_LOWPOWER_LED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
#ifdef CLOSE_TOUCH_PANEL
PIN_Config keypadIntPinTable[] = {
    Board_DIO28_KEYPAD_INT | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL,
    PIN_TERMINATE
};
#else
PIN_Config keypadIntPinTable[] = {
    Board_DIO28_KEYPAD_INT | PIN_INPUT_EN | PIN_NOPULL | PIN_IRQ_BOTHEDGES,
    PIN_TERMINATE
};
#endif
PIN_Config batteryCntPinTable[] = {
    Board_DIO13_BAT_CTRL | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

PIN_Config buttonPinTable[] = {
    Board_DIO12_FACTORY_BTN | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_DIS,
    Board_DIO4_PRIVACY_BTN | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_DIS,
    PIN_TERMINATE
};

PIN_Config keyPadPowerPinTable[] = {
    Board_DIO11_KEYPAD_PWR | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static PIN_Config coverDetectPinTable[] = {
    Board_DIO1_COVER_DETECT | PIN_INPUT_EN | PIN_NOPULL | PIN_IRQ_DIS,
    PIN_TERMINATE
};

static PIN_Config motorSWPinTable[] = {
    Board_DIO14_MOTOR_SW1 | PIN_INPUT_EN | PIN_NOPULL | PIN_IRQ_DIS,
    Board_DIO15_MOTOR_SW2 | PIN_INPUT_EN | PIN_NOPULL | PIN_IRQ_DIS,
    PIN_TERMINATE
};

#ifndef MOTOR_PWM
PIN_Config motorDRPinTable[] = {
    CC26X2R1_JANUS_L1_MOTOR1_PWMPIN0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    CC26X2R1_JANUS_L1_MOTOR2_PWMPIN1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL |
    PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
#endif

#ifdef TEST
static PIN_Config tsetTable[] = {
    Board_DIO5_TEST1 | PIN_INPUT_EN | PIN_PULLDOWN | PIN_IRQ_DIS,
    Board_DIO6_TEST2 | PIN_INPUT_EN | PIN_PULLDOWN | PIN_IRQ_DIS,
    Board_DIO7_TEST3 | PIN_INPUT_EN | PIN_PULLDOWN | PIN_IRQ_DIS,
    PIN_TERMINATE
};
#endif

static PIN_Handle ledPinHandle;
static PIN_State ledPinState;

static PIN_Handle keypadIntPinHandle;
static PIN_State keypadIntPinState;

static PIN_Handle batteryCntPinHandle;
static PIN_State batteryCntPinState;

static PIN_Handle buttonPinHandle;
static PIN_State buttonPinState;

static PIN_Handle keypadPwrPinHandle;
static PIN_State keypadPwrPinState;

static PIN_Handle coverDetectPinHandle;
static PIN_State coverDetectPinState;

static PIN_Handle motorSWPinHandle;  //Pin driver handles
static PIN_State motorSWPinState;  //Global memory storage for a PIN_Config table

#ifndef MOTOR_PWM
static PIN_Handle motorDRPinHandle;
static PIN_State motorDRPinState;
#endif

#ifdef TEST
static PIN_Handle testPinHandle;
static PIN_State testPinState;
#endif

static PWM_Handle buzzerPWM;

#ifdef MOTOR_PWM
static PWM_Handle motor1PWM; //counterclockwise
static PWM_Handle motor2PWM; //clockwise
#endif

static Clock_Struct uiClockStruct;
static Clock_Handle uiClock;

#ifdef CLOSE_TOUCH_PANEL
static Clock_Struct touchClockStruct;
static Clock_Handle touchClock;
#endif

static Clock_Struct buttonClockStruct;
static Clock_Handle buttonClockHandle;

static Clock_Struct lockDelayedStopMotorClock;
static Clock_Handle lockDelayedStopMotorClockHandle;

#ifdef BATTERY_TEST
static Semaphore_Struct semStruct;
static Semaphore_Handle semHandle;
#endif

static UIMsg UI;

UART_Params uartParams;
UART_Handle keyPadUart;

#ifdef CYCLE_TEST
int Count;
bool Proximity;
bool UartOn = false;
#endif

static LockOrientation lockOrientation;
static volatile uint16_t lockOrientationClockCount;
static volatile PIN_Id firstTriggeredSW;
static volatile PIN_Id secondTriggeredSW;
static volatile PIN_Id motorSW;
static volatile LockState lockState;
static volatile int button1Event;
static volatile int button2Event;
#ifdef CLOSE_TOUCH_PANEL
volatile uint16_t touchCount;
#endif

/**
  * @brief  Start motor clockwise rotation.
  * @param none
  * @return none
  */
void startMotorClockwise(void)
{
    stopMotor();
    startMotor2();
}

/**
  * @brief  Start motor counter clockwise rotation.
  * @param none
  * @return none
  */
void startMotorCounterclockwise(void)
{
    stopMotor();
    startMotor1();
}

/**
  * @brief  Start motor for lock.
  * @param none
  * @return none
  */
void MotorLock(void)
{
    if(lockOrientation == ORIENTATION_LEFT)
        startMotorCounterclockwise();
    else if(lockOrientation == ORIENTATION_RIGHT)
        startMotorClockwise();
}

/**
  * @brief  Start motor for unlock.
  * @param none
  * @return none
  */
void MotorUnlock(void)
{
    if(lockOrientation == ORIENTATION_LEFT)
        startMotorClockwise();
    else if(lockOrientation == ORIENTATION_RIGHT)
        startMotorCounterclockwise();
}

/**
  * @brief Get lock state.
  * @param none
  * @return lock state
  */
LockState GetLockState()
{
    return lockState;
}

/**
  * @brief Get motor SW.
  * @param none
  * @return motor SW
  */
PIN_Id GetMotorSW()
{
    return motorSW;
}

/**
  * @brief unlock door.
  * @param wait: true: to wait the action of unlock finish; false: not wait the action of unlock finish
  * @return 0: Not touch swith; 1: Touch switch
  */
void UnLock(bool wait)
{
#ifdef DEBUG
    System_printf("UnLocking ...\r\n");
#endif
    /*
    if(GetLockState() == UNLOCK_STATE)
    {
        System_printf("Already UnLock\r\n");
        return;
    }
    */
    motorSWPreparation();
#ifdef OLD_ME
    lockOrientationClockCount = 70; //1.4sec, unit = 20ms //50 //60
#else
    lockOrientationClockCount = 75;
#endif
    MotorUnlock();
    do {
        if(firstTriggeredSW == Board_DIO14_MOTOR_SW1)
        {
#ifdef CYCLE_TEST
            Count++;
#endif
            break;
        }
    } while (lockOrientationClockCount);

    if(lockOrientationClockCount)
    {
        Util_restartClock(&lockDelayedStopMotorClock, MOTOR_DELAY_UNLOCK_STOP_MS);
        if(wait)
        {
            while(Util_isActive(&lockDelayedStopMotorClock));
        }
    }
    else
    {
        stopMotor();
    }
}

/**
  * @brief lock door.
  * @param wait: true: to wait the action of lock finish; false: not wait the action of lock finish
  * @return 0: Not touch swith; 1: Touch switch
  */
void Lock(bool wait)
{
#ifdef DEBUG
    System_printf("Locking ...\r\n");
#endif
    /*
    if(GetLockState() == LOCK_STATE)
    {
        System_printf("Already Lock\r\n");
        return;
    }
    */
    motorSWPreparation();
#ifdef OLD_ME
    lockOrientationClockCount = 70; //1.4sec, unit = 20ms //50ms //60
#else
    lockOrientationClockCount = 75;
#endif
    MotorLock();
    do {
        if(firstTriggeredSW == Board_DIO15_MOTOR_SW2)
        {
#ifdef CYCLE_TEST
            Count++;
#endif
            break;
        }
    } while (lockOrientationClockCount);

    if(lockOrientationClockCount)
    {
        Util_restartClock(&lockDelayedStopMotorClock, MOTOR_DELAY_LOCK_STOP_MS);
        if(wait)
        {
            while(Util_isActive(&lockDelayedStopMotorClock));
        }
    }
    else
    {
        stopMotor();
    }
}

/**
  * @brief  Initialize debug port.
  * @param none
  * @return none
  */
void InitDebugPort()
{
    UART_Params uartParams;

    // Enable System_printf(..) UART output
    UART_Params_init(&uartParams);
    UartPrintf_init(UART_open(Board_UART0, &uartParams));
}

/**
  * @brief  Stop a clock.
  * @param pClock : pointer to clock struct.
  * @param wait : true: To wait clock finish, false: to stop clock immediately
  * @return none
  */
void StopClock(Clock_Struct *pClock, bool wait)
{
    if(wait)
    {
        while(Util_isActive(pClock));
    }
    else
    {
        if(Util_isActive(pClock))
        {
            Util_stopClock(pClock);
        }
    }
}

/**
  * @brief  check lock direction.
  * @param none
  * @return door direction and found or not
  */
RLCheckResult RLCheck()
{
    RLCheckResult result = NOT_FOUND;

    motorSWPreparation();
#ifdef DEBUG
    System_printf("start motor clockwise\r\n");
#endif
    startMotorClockwise();
#ifdef OLD_ME
    lockOrientationClockCount = 50;  //1 sec , unit = 20ms  //50 //75
#else
    lockOrientationClockCount = 50;
#endif
#ifdef CYCLE_TEST
    Count++;
#endif
    do {
        if((Board_DIO15_MOTOR_SW2 == firstTriggeredSW) && (Board_DIO14_MOTOR_SW1 == secondTriggeredSW))
        {
#ifdef DEBUG
            System_printf("C: SW2--SW1\r\n");
#endif
            lockOrientation = ORIENTATION_LEFT;
            Task_sleep(MOTOR_DELAY_STOP_TIME);
            stopMotor();
            //isDoorLocked = false;
            lockState = UNLOCK_STATE;
            result = UNLOCK;
            break;
        }
        else if ((Board_DIO14_MOTOR_SW1 == firstTriggeredSW) && (Board_DIO15_MOTOR_SW2 == secondTriggeredSW))
        {
#ifdef DEBUG
            System_printf("C: SW1--SW2\r\n");
#endif
            lockOrientation = ORIENTATION_RIGHT;
            Task_sleep(MOTOR_DELAY_STOP_TIME);
            stopMotor();
            //isDoorLocked = true;
            lockState = LOCK_STATE;
            result = LOCK;
            break;
        }
       // Task_sleep(100 / Clock_tickPeriod); //prevent busy task
    } while (lockOrientationClockCount);
    stopMotor();

    if (ORIENTATION_NOT_FOUND == lockOrientation)
    {
#ifdef CYCLE_TEST
        Count++;
#endif
        motorSWPreparation();
#ifdef DEBUG
        System_printf("start motor counterclockwise\r\n");
#endif
        startMotorCounterclockwise();
#ifdef OLD_ME
        lockOrientationClockCount = 100; //2sec, unit = 20ms 100 //150
#else
        lockOrientationClockCount = 150;
#endif
        do {
            if((Board_DIO15_MOTOR_SW2 == firstTriggeredSW) && (Board_DIO14_MOTOR_SW1 == secondTriggeredSW))
            {
#ifdef DEBUG
                System_printf("CC: SW2--SW1\r\n");
#endif
                lockOrientation = ORIENTATION_RIGHT;
                Task_sleep(MOTOR_DELAY_STOP_TIME);
                stopMotor();
                //isDoorLocked = false;
                lockState = UNLOCK_STATE;
                result = UNLOCK;
                break;
            }
            else if((Board_DIO14_MOTOR_SW1 == firstTriggeredSW) && (Board_DIO15_MOTOR_SW2 == secondTriggeredSW))
            {
#ifdef DEBUG
                System_printf("CC: SW1--SW2\r\n");
#endif
                lockOrientation = ORIENTATION_LEFT;
                Task_sleep(MOTOR_DELAY_STOP_TIME);
                result = LOCK;
                lockState = LOCK_STATE;
                stopMotor();
                //isDoorLocked = true;
                /*
                 * Waste a few cycles to keep the CLK high for at
                  * least 45% of the period.
                      * 3 cycles per loop: 8 loops @ 48 Mhz = 0.5 us.
                      */
                     //CPUdelay(8);

                break;
            }
            //Task_sleep(100 / Clock_tickPeriod); //prevent busy task
        } while (lockOrientationClockCount);
        stopMotor();
    }

    if(lockOrientation == ORIENTATION_NOT_FOUND)
    {
#ifdef DEBUG
        System_printf("Not Found Direction\r\n");
#endif
    }
    else if(lockOrientation == ORIENTATION_RIGHT)
    {
#ifdef DEBUG
        System_printf("Direction = Right\r\n");
#endif
    }
    else
    {
#ifdef DEBUG
        System_printf("Direction = left\r\n");
#endif
    }
    return result;
}

/**
  * @brief  do door direction check.
  * @param none
  * @return none
  */
void DoRLCheck()
{
    int i;
    RLCheckResult result;

    lockOrientation = ORIENTATION_NOT_FOUND;
    for(i = 0; i < 2; i++)
    {
        result = RLCheck();
        if(result != NOT_FOUND)
        {
           break;
        }
    }

/*
    if(result == LOCK)
    {
        UnLock(true);
        Lock(true);
    }
    else if(result == UNLOCK)
    {
        Lock(true);
    }
*/
}
/**
  * @brief  UI CLOCK callback function.
  * @param arg0: input parameter for UI CLOCK callback function
  * @return none
  */
static void uiFxn(UArg arg0)
{
    static bool nonStopBeepFlag = false;
    int i;

    if((0 == UI.beepTimes) && (UI.beepType != BEEP_NONE))
    {
        PWM_stop(buzzerPWM);
       // PWM_close(buzzerPWM);
       // buzzerPWM = NULL;
    }

    if((0 == UI.lowPowerTimes) && (UI.lowPower != LOWPOWER_LED_NONE))
    {
        PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_OFF);
    }

    for(i = 0; i < 2; i++)
    {
        if((0 == UI.ledTimes[i]) && (UI.led[i] != LED_NONE))
        {
            if(i)  //back led
            {
                if(UI.led[i] == LED_R)
                    PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_OFF);
                else
                    PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_OFF);
            }
            else  //front led
            {
                if(UI.led[i] == LED_R)
                    PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_OFF);
                else
                    PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_OFF);
            }
        }

        if((LED_FLASH_FOREVER == UI.ledTimes[i]) && (LED_FLASH == UI.ledType[i]))
        {
            if(i)  //back led
            {
                if(UI.led[i] == LED_R)
                {
                    if(PIN_getOutputValue(Board_DIO19_BACK_RLED))
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, 0);
                    }
                    else
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, 1);
                    }
                }
                else if(UI.led[i] == LED_G)
                {
                    if(PIN_getOutputValue(Board_DIO18_BACK_GLED))
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, 0);
                    }
                    else
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, 1);
                    }
                }
            }
            else  //front led
            {
                if(UI.led[i] == LED_R)
                {
                    if(PIN_getOutputValue(Board_DIO22_FRONT_RLED))
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, 0);
                    }
                    else
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, 1);
                    }
                }
                else if(UI.led[i] == LED_G)
                {
                    if(PIN_getOutputValue(Board_DIO21_FRONT_GLED))
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, 0);
                    }
                    else
                    {
                        PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, 1);
                    }
                }
            }
        }
    }

    if((LED_FLASH_FOREVER == UI.lowPowerTimes) && (LOWPOWER_LED_FLASH == UI.lowPower))
    {
        if(PIN_getOutputValue(Board_DIO0_LOWPOWER_LED))
        {
            PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, 0);
        }
        else
        {
            PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, 1);
        }
    }

    if((BEEP_FOREVER == UI.beepTimes) && (BEEP_SHORT == UI.beepType))
    {
        nonStopBeepFlag = !nonStopBeepFlag;
        (nonStopBeepFlag) ? PWM_stop(buzzerPWM) : PWM_start(buzzerPWM);
    }

    if ((0 == UI.beepTimes) && (0 == UI.ledTimes[0]) && (0 == UI.ledTimes[1]) && (0 == UI.lowPowerTimes))
        Clock_stop(uiClock);

    if((UI.beepType != BEEP_NONE) && (UI.beepTimes > 0))
    {
        if (BEEP_SHORT == UI.beepType)
            (UI.beepTimes-- & 0x1) ? PWM_start(buzzerPWM) : PWM_stop(buzzerPWM);
        else
            UI.beepTimes--;
    }

    if((UI.lowPower != LOWPOWER_LED_NONE) && (UI.lowPowerTimes > 0))
    {
        if (LOWPOWER_LED_FLASH == UI.lowPower)
        {
            if(PIN_getOutputValue(Board_DIO0_LOWPOWER_LED))
            {
                PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, 0);
            }
            else
            {
                PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, 1);
            }
        }
        UI.lowPowerTimes --;
    }

    for(i = 0; i < 2; i++)
    {
        if((UI.led[i] != LED_NONE) && (UI.ledTimes[i] > 0))
        {
            if (LED_FLASH == UI.ledType[i])
            {
                if(i)  //back led
                {
                    if(UI.led[i] == LED_R)
                    {
                        if(PIN_getOutputValue(Board_DIO19_BACK_RLED))
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, 0);
                        }
                        else
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, 1);
                        }
                    }
                    else
                    {
                        if(PIN_getOutputValue(Board_DIO18_BACK_GLED))
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, 0);
                        }
                        else
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, 1);
                        }
                    }
                }
                else  //front led
                {
                    if(UI.led[i] == LED_R)
                    {
                        if(PIN_getOutputValue(Board_DIO22_FRONT_RLED))
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, 0);
                        }
                        else
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, 1);
                        }
                    }
                    else
                    {
                        if(PIN_getOutputValue(Board_DIO21_FRONT_GLED))
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, 0);
                        }
                        else
                        {
                            PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, 1);
                        }
                    }
                }
            }
            UI.ledTimes[i] --;
        }
    }
}

/*********************************************************************
 * @fn     motorSWCallbackFxn
 *
 * @brief  Callback from PIN driver on interrupt
 *
 *         Sets in motion the debouncing.
 *
 * @param  handle    The PIN_Handle instance this is about
 * @param  pinId     The pin that generated the interrupt
 */
static void motorSWCallbackFxn(PIN_Handle handle, PIN_Id pinId)
{
    static PIN_Id lastSW = 0;

  //  System_printf("PRE-SW : %d \r\n", pinId);
   // if(!PIN_getInputValue(pinId))
//    {
     //   if(!PIN_getInputValue(pinId))
    //    {
       //     if(!PIN_getInputValue(pinId))
      //      {
                motorSW = pinId;
       //     }
   //         else
   //         {
   //             return;
   //         }
     //  }
  //      else
  //      {
    //        return;
   //     }
   // }
  //  else
   // {
  //      return;
  //  }

    if(ORIENTATION_NOT_FOUND == lockOrientation)
    {
        if(!firstTriggeredSW)
        {
            firstTriggeredSW = pinId;
#ifdef DEBUG
            System_printf("firstTriggeredSW: %d\r\n", firstTriggeredSW);
#endif
        }
        else if ((!secondTriggeredSW) && (firstTriggeredSW != pinId))
        {
            secondTriggeredSW = pinId;
#ifdef DEBUG
            System_printf("secondTriggeredSW: %d\r\n", secondTriggeredSW);
#endif
        }
#ifdef DEBUG
        System_printf("kidd : %d \r\n", motorSW);
#endif
        lastSW = motorSW;
    }
    else
    {
      //  System_printf("SW : %d \r\n", motorSW);
        firstTriggeredSW = motorSW;
        if(lastSW != motorSW)
        {
            if(motorSW == Board_DIO15_MOTOR_SW2)
                lockState = LOCK_STATE;
            else
                lockState = UNLOCK_STATE;
            lastSW = motorSW;
        }
        else
        {
            if(lockState == UNKNOW_STATE)
            {
                if(motorSW == Board_DIO15_MOTOR_SW2)
                    lockState = LOCK_STATE;
                else
                    lockState = UNLOCK_STATE;
            }
            else
            {
                lockState == UNKNOW_STATE;
            }
        }
    }
}

/**
  * @brief  Get battery ADC.
  * @param microVolt : output battery micro volt; if microVolt = NULL, it doesn't output battery micro volt
  * @return battery raw ADC value
  */
uint16_t GetBatteryADC(uint32_t *microVolt)
{
    uint16_t     i = 0;
    ADC_Handle   adc;
    ADC_Params   params;
    int_fast16_t res;
    uint16_t adcValue[ADC_SAMPLE_COUNT];
    uint32_t adcvalue = 0;

    /* Turn on Battery voltage detect */
 //   PIN_setOutputValue(batteryCntPinHandle, Board_DIO13_BAT_CTRL, 1);
    ADC_Params_init(&params);
    adc = ADC_open(Board_POWER_ADC, &params);

    if (adc == NULL)
    {
#ifdef DEBUG
        System_printf("Open power ADC fail\r\n");
#endif
        /* Turn off Battery voltage detect */
 //       PIN_setOutputValue(batteryCntPinHandle, Board_DIO13_BAT_CTRL, 0);
        return 0;
    }

    while(i < ADC_SAMPLE_COUNT)
    {
        res = ADC_convert(adc, &adcValue[i]);
        if (res == ADC_STATUS_SUCCESS)
        {
           // System_printf("raw result: %d\r\n", adcValue[i]);
            i++;
           // System_flush();
        }
    }

    /* Turn off Battery voltage detect */
//    PIN_setOutputValue(batteryCntPinHandle, Board_DIO13_BAT_CTRL, 0);

    /* Skip the front 4 ADC value */
    for(i = 0; i < 16; i++)
    {
        adcvalue = adcvalue + adcValue[ADC_SAMPLE_COUNT - i - 1];
    }

    adcvalue = adcvalue >> 4;
    adcValue[0] = adcvalue;
#ifdef DEBUG
    System_printf("Ave ADC: %d\r\n", adcValue[0]);
#endif
    if(microVolt != NULL)
    {
        *microVolt = ADC_convertRawToMicroVolts(adc, adcValue[0]);
#ifdef DEBUG
        System_printf("Volt result: %d\r\n", *microVolt);
#endif
    }
    ADC_close(adc);

    return adcValue[0];
}

#ifdef CLOSE_TOUCH_PANEL
/**
  * @brief  touch CLOCK callback function.
  * @param arg0: input parameter for touch CLOCK callback function
  * @return none
  */
static void touchFxn(UArg arg0)
{
    touchCount ++;
/*
    if(touchCount == 1)
    {
        PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 0);
    }
    else if(touchCount >= 2)
    {
        PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 1);
        Clock_stop(touchClock);
    }
    */
    if(touchCount == 2)
    {
        PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 1);
    }
    else if(touchCount >= 3)
    {
        PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 0);
        Clock_stop(touchClock);
    }

}

/**
  * @brief  Initialize clock for closing touch.
  * @param none
  * @return none
  */
void InitCloseTouchClock()
{
    touchClock = Util_constructClock(&touchClockStruct, (Clock_FuncPtr) touchFxn, CLOSE_TOUCH_CLOCK_PERIOD, CLOSE_TOUCH_CLOCK_PERIOD, false, 0);
}

/**
  * @brief  Close Touch.
  * @param none
  * @return none
  */
void CloseTouch()
{
    StopClock(&touchClockStruct, false);
    touchCount = 0;
    Util_startClock(&touchClockStruct);
}
#endif
/**
  * @brief  Initialize UI.
  * @param none
  * @return none
  */
void InitUI()
{
    uiClock = Util_constructClock(&uiClockStruct, (Clock_FuncPtr) uiFxn, UI_CLOCK_PERIOD, UI_CLOCK_PERIOD, false, 0);
}
/**
  * @brief  Set UI.
  * @param ui : UI action.
  * @param wait : true: To wait the last UI finish, false: Not wait the last UI finish
  * @return none
  */
void SetUI(UIMsg ui, bool wait)
{
    int i;

    if ((BEEP_FOREVER == UI.beepTimes) || (LED_FLASH_FOREVER == UI.ledTimes[0]) || (LED_FLASH_FOREVER == UI.ledTimes[1]) || (LED_FLASH_FOREVER == UI.lowPowerTimes))
        StopClock(&uiClockStruct, false);  //force stop the last UI
    else
        StopClock(&uiClockStruct, wait);

    UI.beepType = ui.beepType;
    if(UI.beepType == BEEP_NONE)
    {
        UI.beepTimes = 0;
    }
    else
    {
        UI.beepTimes = ui.beepTimes;
    }

    UI.lowPower = ui.lowPower;
    if(UI.lowPower == LOWPOWER_LED_NONE)
    {
        UI.lowPowerTimes = 0;
    }
    else
    {
        UI.lowPowerTimes = ui.lowPowerTimes;
        PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_ON);
    }

    for(i = 0; i < 2; i++)
    {
        UI.led[i] = ui.led[i];
        UI.ledType[i] = ui.ledType[i];
        if(UI.led[i] == LED_NONE)
        {
            UI.ledTimes[i] = 0;
        }
        else
        {
            UI.ledTimes[i] = ui.ledTimes[i];
            if(i)  //back led
            {
                if(UI.led[i] == LED_R)
                    PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_ON);
                else
                    PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_ON);
            }
            else  //front led
            {
                if(UI.led[i] == LED_R)
                    PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_ON);
                else
                    PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_ON);
            }
        }
    }

    if((UI.beepType == BEEP_ON))
        PWM_start(buzzerPWM);

    Util_startClock(&uiClockStruct);
}

/**
  * @brief  Stop UI.
  * @param none
  * @return none
  */
void StopUI()
{
    int i;

    if(Util_isActive(&uiClockStruct))
        Util_stopClock(&uiClockStruct);

    for(i = 0; i < 2; i++)
    {
        if(UI.led[i] != LED_NONE)
        {
            if(i)  //back led
            {
                if(UI.led[i] == LED_R)
                    PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_OFF);
                else
                    PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_OFF);
            }
            else  //front led
            {
                if(UI.led[i] == LED_R)
                    PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_OFF);
                else
                    PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_OFF);
            }
            UI.led[i] = LED_NONE;
        }
        UI.ledTimes[i] = 0;
    }

    if(UI.lowPower != LOWPOWER_LED_NONE)
    {
        UI.lowPower = LOWPOWER_LED_NONE;
        PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_OFF);
    }

    if(UI.beepType != BEEP_NONE)
    {
        PWM_stop(buzzerPWM);
        UI.beepType = BEEP_NONE;
    }

    UI.lowPowerTimes = 0;
    UI.beepTimes = 0;
}

/**
  * @brief  Set led.
  * @param led : which led to action.
  * @param value : to set led value.
  * @return none
  */
void SetLed(SetLED led, bool value)
{
    switch(led)
    {
        case LED_FR:
            if(value)
                PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_ON);
            else
                PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_OFF);
        break;

        case LED_FG:
            if(value)
                PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_ON);
            else
                PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_OFF);
        break;

        case LED_BR:
            if(value)
                PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_ON);
            else
                PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_OFF);
        break;

        case LED_BG:
            if(value)
                PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_ON);
            else
                PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_OFF);
        break;


        case LED_LOWPOWER:
            if(value)
                PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_ON);
            else
                PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_OFF);
        break;

        case LED_ALL_ON:
            PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_ON);
            PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_ON);
            PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_ON);
            PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_ON);
            PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_ON);
        break;

        case LED_ALL_OFF:
            PIN_setOutputValue(ledPinHandle, Board_DIO22_FRONT_RLED, Board_GPIO_LED_OFF);
            PIN_setOutputValue(ledPinHandle, Board_DIO21_FRONT_GLED, Board_GPIO_LED_OFF);
            PIN_setOutputValue(ledPinHandle, Board_DIO19_BACK_RLED, Board_GPIO_LED_OFF);
            PIN_setOutputValue(ledPinHandle, Board_DIO18_BACK_GLED, Board_GPIO_LED_OFF);
            PIN_setOutputValue(ledPinHandle, Board_DIO0_LOWPOWER_LED, Board_GPIO_LED_OFF);
        break;

        default:
        break;
    }
}

/**
  * @brief  lock delayed stop motor callback function.
  * @param arg0: input parameter for lock delayed stop motor callback function
  * @return none
  */
static void lockDelayedStopMotorFxn(UArg arg) {
    stopMotor();
#ifdef DEBUG
    System_printf("Stopped motor \r\n");
#endif
}

/**
  * @brief  Initialize puzzer.
  * @param none
  * @return none
  */
void InitPuzzer()
{
    PWM_Params params;

    PWM_Params_init(&params);
    params.dutyUnits = PWM_DUTY_FRACTION;
    params.dutyValue = DUTY_VOLUME_HIGH;
    params.periodUnits = PWM_PERIOD_HZ;
    params.periodValue = BUZZER_PWM_FREQ;
    buzzerPWM = PWM_open(Board_PWM_PUZZER, &params);
#ifdef DEBUG
    if (!buzzerPWM)
        System_printf("buzzer pwm open failed...\r\n");
#endif
}

/**
  * @brief  Initialize motor.
  * @param none
  * @return none
  */
void InitMotor()
{
#ifdef MOTOR_PWM
    PWM_Params pwmParams;
#endif

    lockDelayedStopMotorClockHandle = Util_constructClock(&lockDelayedStopMotorClock, lockDelayedStopMotorFxn, MOTOR_DELAY_UNLOCK_STOP_MS, 0, FALSE, 0);

#ifdef MOTOR_PWM
    PWM_Params_init(&pwmParams);
    pwmParams.dutyUnits = PWM_DUTY_FRACTION;
    pwmParams.dutyValue = MOTOR_PWM_DUTY;
    pwmParams.periodUnits = PWM_PERIOD_HZ;
    pwmParams.periodValue = MOTOR_PWM_FREQ;
    motor1PWM = PWM_open(Board_PWM_MOTOR1, &pwmParams);
    motor2PWM = PWM_open(Board_PWM_MOTOR2, &pwmParams);
#ifdef DEBUG
    if (!motor1PWM)
        System_printf("motor1PWM open failed...\r\n");
    if (!motor2PWM)
        System_printf("motor2PWM open failed...\r\n");
#endif
#else
    motorDRPinHandle = PIN_open(&motorDRPinState, motorDRPinTable);
#ifdef DEBUG
    if(!motorDRPinHandle)
           System_printf("motor dr pin open failed...\r\n");
#endif
#endif

    /* Enable motor SW interrupt */
    motorSWPinHandle = PIN_open(&motorSWPinState, motorSWPinTable);
#ifdef DEBUG
    if(!motorSWPinHandle)
        System_printf("motor sw pin open failed...\r\n");

    if(PIN_registerIntCb(motorSWPinHandle, &motorSWCallbackFxn) != 0)
        System_printf("register motor sw callback failed...\r\n");
#endif
}

/**
  * @brief  Initialize global parameter.
  * @param none
  * @return none
  */
void InitGlobalParameter()
{
    /* Initial FIFO buffer */
    InitialFIFO(FIFOSIZE, FIFOStack, &FIFOBuf);
    /* We want to sleep for 10000 microseconds */
    sleepTickCount = 500000 / Clock_tickPeriod;
    lockOrientation = ORIENTATION_NOT_FOUND;
    lockOrientationClockCount = 0;
    firstTriggeredSW = 0;
    secondTriggeredSW = 0;
    motorSW = 0;
    button1Event = 0;
    button2Event = 0;
#ifdef CLOSE_TOUCH_PANEL
    touchCount = 0;
#endif
    lockState = UNKNOW_STATE;
    buzzerPWM = NULL;
#ifdef MOTOR_PWM
    motor1PWM = NULL; //counterclockwise
    motor2PWM = NULL; //clockwise
#else
    motorDRPinHandle = NULL;
#endif

#ifdef TEST
    testPinHandle = NULL;
#endif
    keyPadUart = NULL;
    ledPinHandle = NULL;
    keypadIntPinHandle = NULL;
    batteryCntPinHandle = NULL;
    buttonPinHandle = NULL;
    keypadPwrPinHandle = NULL;
    coverDetectPinHandle = NULL;
    motorSWPinHandle = NULL;
#ifdef CYCLE_TEST
    Count = 0;
    Proximity = false;
#endif
}

/*********************************************************************
 * @fn     motorSWCallbackFxn
 *
 * @brief  Callback from PIN driver on interrupt
 *
 *         Sets in motion the debouncing.
 *
 * @param  handle    The PIN_Handle instance this is about
 * @param  pinId     The pin that generated the interrupt
 */
#ifndef CLOSE_TOUCH_PANEL
static void keypadIntCallbackFxn(PIN_Handle handle, PIN_Id pinId)
{
    if(PIN_getInputValue(PIN_ID(Board_DIO28_KEYPAD_INT)))
    {
#ifdef CYCLE_TEST
      //  keyPadUart = UART_open(Board_UART1, &uartParams);
      //  UART_read(keyPadUart, KeyBuffer, 1);
        Proximity = true;
        UartOn = true;
#endif
#ifdef DEBUG
        System_printf("positive edge\r\n");
#endif
    }
    else
    {
#ifdef CYCLE_TEST
      //  UART_close(keyPadUart);
        UartOn = false;
#endif
#ifdef DEBUG
        System_printf("negative edge\r\n");
#endif
    }
}
#endif
/*!
 *  @brief      The definition of a callback function used by the UART driver
 *              when used in #UART_MODE_CALLBACK
 *              The callback can occur in task or HWI context.
 *
 *  @param      UART_Handle             UART_Handle
 *
 *  @param      buf                     Pointer to read/write buffer
 *
 *  @param      count                   Number of elements read/written
 */
void keyPadCallback(UART_Handle handle, void *buf, size_t count)
{
    char input;
    int i;
    UIMsg ui;

    for(i = 0; i < count; i++)
    {
        input = ((char *) buf)[i];
     //  System_printf("keyPadCallback: Keypad data = %c\r\n", input);
        if((('0' <= input) && ('9' >= input)) || ('*' == input) || ('#' == input))
        {
            ui.led[0] = LED_NONE;
            ui.led[1] = LED_NONE;
            ui.ledTimes[0] = 0;  // 1sec
            ui.ledTimes[1] = 0;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_ON;
            ui.beepTimes = 1;
            SetUI(ui, true);
        }
        FIFOPutByte(&FIFOBuf, input);
    }
#ifdef BATTERY_TEST
        Semaphore_post(semHandle);
#endif
}
/**
  * @brief  button CLOCK callback function.
  * @param arg0: input parameter for button CLOCK callback function
  * @return none
  */
static void buttonClockFxn(UArg arg)
{
    static int btn1_status = 0, btn2_status = 0;

    if(PIN_getInputValue(PIN_ID(Board_DIO12_FACTORY_BTN)))
   // if(PIN_getInputValue(PIN_ID(Board_DIO13_MOTOR_SW1)))
    {
        if(btn1_status >= 10)
            button1Event |= EVENT_RELEASE;
        btn1_status = 0;
    }
    else  // press Board_DIO12_FACTORY_BTN
    {
        if(btn1_status >= 0)
            btn1_status ++;
        if(btn1_status == 10) //0.2 sec
        {
            button1Event |= EVENT_PRESS;
        }
        else if(btn1_status == 100) // 2sec
        {
            button1Event |= EVENT_PRESS_LONG;
          //  btn1_status = -1;
        }
    }

    if(PIN_getInputValue(PIN_ID(Board_DIO4_PRIVACY_BTN)))
    //if(PIN_getInputValue(PIN_ID(Board_DIO14_MOTOR_SW2)))
    {
        if( btn2_status >= 10 )
            button2Event |= EVENT_RELEASE;
        btn2_status = 0;
    }
    else  // press Board_DIO4_PRIVACY_BTN
    {
        if(btn2_status >= 0)
            btn2_status ++;
        if(btn2_status == 10)  //0.2 sec
        {
            button2Event |= EVENT_PRESS;
        }
        else if(btn2_status == 100) // 2sec
        {
            //btn2_status = -1;
            button2Event |= EVENT_PRESS_LONG;
        }
    }

    if(lockOrientationClockCount)
        lockOrientationClockCount--;
}
/**
  * @brief  Initialize maintask.
  * @param none
  * @return none
  */
void InitMaintask()
{
   // UART_Params uartParams;
   // char data;
    UIMsg ui;

    InitUI();
    InitPuzzer();
#ifdef CLOSE_TOUCH_PANEL
    InitCloseTouchClock();
#endif

    /* Open led */
    ledPinHandle = PIN_open(&ledPinState, ledPinTable);
#ifdef DEBUG
    if(!ledPinHandle)
        System_printf("led open failed...\r\n");
#endif
    /* Open keypad power */
    keypadPwrPinHandle = PIN_open(&keypadPwrPinState, keyPadPowerPinTable);
#ifdef DEBUG
    if(!keypadPwrPinHandle)
        System_printf("keypad power open failed...\r\n");
#endif
    /* Open battery control */
    batteryCntPinHandle = PIN_open(&batteryCntPinState, batteryCntPinTable);
#ifdef DEBUG
    if(!batteryCntPinHandle)
        System_printf("battery control open failed...\r\n");
#endif
    /* Open key cover */
    coverDetectPinHandle = PIN_open(&coverDetectPinState, coverDetectPinTable);
#ifdef DEBUG
    if(!coverDetectPinHandle)
        System_printf("key cover open failed...\r\n");
#endif
    /* Open Button */
    buttonPinHandle = PIN_open(&buttonPinState, buttonPinTable);
#ifdef DEBUG
    if(!buttonPinHandle)
        System_printf("button open failed...\r\n");
#endif

#ifdef CLOSE_TOUCH_PANEL
    keypadIntPinHandle = PIN_open(&keypadIntPinState, keypadIntPinTable);
    if(!keypadIntPinHandle)
        System_printf("keypad int pin open failed...\r\n");
#else
    /* Enable Keypad interrupt */
    keypadIntPinHandle = PIN_open(&keypadIntPinState, keypadIntPinTable);
    if(!keypadIntPinHandle)
        System_printf("keypad int pin open failed...\r\n");
    if(PIN_registerIntCb(keypadIntPinHandle, &keypadIntCallbackFxn) != 0)
        System_printf("register keypad int callback failed...\r\n");
#endif
    InitMotor();

#ifdef TEST
    /* Open test pin */
    testPinHandle = PIN_open(&testPinState, tsetTable);
#ifdef DEBUG
    if(!testPinHandle)
        System_printf("test pin open failed...\r\n");
#endif
#endif
    /* Enable keypad uart to read */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;
    uartParams.readMode = UART_MODE_CALLBACK;
   // uartParams.writeMode = UART_MODE_CALLBACK;
    uartParams.readCallback = keyPadCallback;


    /* Enable detect button event clock */
    buttonClockHandle = Util_constructClock(&buttonClockStruct, buttonClockFxn, 20, 20, TRUE, 0);

    /* Test led and buzzer */
    ui.led[0] = LED_G;
    ui.led[1] = LED_G;
    ui.ledTimes[0] = 15;  // 3sec
    ui.ledTimes[1] = 15;
    ui.ledType[0] = LED_FLASH;
    ui.ledType[1] = LED_FLASH;
    ui.lowPower = LOWPOWER_LED_FLASH;
    ui.lowPowerTimes = 15;
    ui.beepType = BEEP_ON;
    ui.beepTimes = 15;
    SetUI(ui, false);

    ui.led[0] = LED_R;
    ui.led[1] = LED_R;
    ui.ledTimes[0] = 10;  // 2sec
    ui.ledTimes[1] = 10;
    ui.ledType[0] = LED_FLASH;
    ui.ledType[1] = LED_FLASH;
    ui.lowPower = LOWPOWER_LED_NONE;
    ui.lowPowerTimes = 0;
    ui.beepType = BEEP_NONE;
    ui.beepTimes = 0;
    SetUI(ui, true);
#ifdef DEBUG
#ifdef CYCLE_TEST
    System_printf("Cycle test version = %d\r\n", CYCLE_TEST_VERSION);
#else
    System_printf("Function test version = %d\r\n", FUNCTION_TEST_VERSION);
#endif
#endif

    DoRLCheck();
#ifndef CYCLE_TEST
    keyPadUart = UART_open(Board_UART1, &uartParams);
 #ifdef DEBUG
    if (keyPadUart)
        System_printf("uart1 initialized\r\n");
    else
        System_printf("uart1 initialized failed...\r\n");
 #endif

    /* reads data from a UART with interrupt enabled */
    UART_read(keyPadUart, KeyBuffer, 1);
#endif
}

/**
  * @brief  Process keypad data.
  * @param none
  * @return none
  */
void ProcessKeypadData()
{
    char data;
    UIMsg ui;
    uint32_t microVolt;
    uint16_t adcValue;

#ifdef CYCLE_TEST
    if(UartOn)
    {
        if(keyPadUart == NULL)
        {
            keyPadUart = UART_open(Board_UART1, &uartParams);
#ifdef DEBUG
            if (keyPadUart)
                System_printf("uart1 initialized\r\n");
            else
                System_printf("uart1 initialized failed...\r\n");
#endif
            /* reads data from a UART with interrupt enabled */
            UART_read(keyPadUart, KeyBuffer, 1);
        }
    }
    else
    {
        if(keyPadUart)
        {
            UART_close(keyPadUart);
            keyPadUart = NULL;
        }
    }
#endif
    while(FIFOIsEmpty(&FIFOBuf) ==  false)
    {
        if(FIFOGetByte(&FIFOBuf, &data))
        {
            if((('0' <= data) && ('9' >= data)) || ('*' == data) || ('#' == data))
            {
#ifdef DEBUG
                System_printf("ProcessKeypadData success: Keypad data = %c\r\n", data);
#endif
#ifndef CYCLE_TEST
                if((data == '#') || ('*' == data))
                {
#ifdef CLOSE_TOUCH_PANEL
                    PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 1);
                    //CloseTouch();
#endif
                    //UART_read(keyPadUart, &data, 1);
                    adcValue = GetBatteryADC(&microVolt);
#ifdef DEBUG
                    System_printf("Before Battery ADC_vaule = %d, mV = %d \n\r ", adcValue, microVolt);
#endif
#ifndef CLOSE_TOUCH_PANEL
                    if(GetMotorSW() == Board_DIO15_MOTOR_SW2)  // Lock
                    {
                        ui.led[0] = LED_G;
                        ui.led[1] = LED_NONE;
                        ui.ledTimes[0] = 5; // 1sec
                        ui.ledTimes[1] = 0;
                        ui.ledType[0] = LED_FLASH;
                        ui.ledType[1] = LED_FLASH;
                        ui.lowPower = LOWPOWER_LED_NONE;
                        ui.lowPowerTimes = 0;
                        ui.beepType = BEEP_ON;
                        ui.beepTimes = 5;
                        SetUI(ui, false);
                        UnLock(true);
                    }
                    else  // Unlock
                    {
                        Lock(true);
                    }
#endif
                    adcValue = GetBatteryADC(&microVolt);
#ifdef DEBUG
                    System_printf("After Battery ADC_vaule = %d, mV = %d \n\r ", adcValue, microVolt);
#endif
#ifdef CLOSE_TOUCH_PANEL
                   PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 0);
#endif
                    UART_read(keyPadUart, &data, 1);
                    return;
                }
#endif
            }
            else
            {
#ifdef DEBUG
                System_printf("ProcessKeypadData fail: Keypad data = %c\r\n", data);
#endif
            }
            //System_flush();
        }
        UART_read(keyPadUart, &data, 1);
    }
#ifdef CYCLE_TEST
    if(Proximity)
    {
        adcValue = GetBatteryADC(&microVolt);
#ifdef DEBUG
        System_printf("Before Battery ADC_vaule = %d, mV = %d \n\r ", adcValue, microVolt);
#endif
//#ifdef CYCLE_TEST
//        Count++;
//#endif
        if(GetMotorSW() == Board_DIO15_MOTOR_SW2)  // Lock
        {
            ui.led[0] = LED_G;
            ui.led[1] = LED_NONE;
            ui.ledTimes[0] = 5; // 1sec
            ui.ledTimes[1] = 0;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_ON;
            ui.beepTimes = 5;
            SetUI(ui, false);
            UnLock(true);
        }
        else  // Unlock
        {
            Lock(true);
        }
2        adcValue = GetBatteryADC(&microVolt);
#ifdef DEBUG
#ifdef CYCLE_TEST
        System_printf("Count number = %d \n\r ", Count);
#endif
        System_printf("After Battery ADC_vaule = %d, mV = %d \n\r ", adcValue, microVolt);
#endif
        Proximity = false;
    }
#endif
}

/*
 *  ======== task1Fxn ========
 */
Void maintaskFxn(UArg arg0, UArg arg1)
{
    UIMsg ui;
   // uint32_t microVolt;
    //uint16_t adcValue;

    InitMaintask();
    for (;;)
    {
#ifdef BATTERY_TEST
        Semaphore_pend(semHandle, BIOS_WAIT_FOREVER);
#endif

        ProcessKeypadData();

        /*Detect Key cover */
        /*
        if(!PIN_getInputValue(PIN_ID(Board_DIO1_COVER_DETECT)))
        {
#ifdef DEBUG
            System_printf("Detect Key cover\r\n");
#endif
            ui.led[0] = LED_R;
            ui.led[1] = LED_NONE;
            ui.ledTimes[0] = 5;  // 1sec
            ui.ledTimes[1] = 0;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_ON;
            ui.beepTimes = 5;
            SetUI(ui, true);
        }
*/
/*
        if(GetMotorSW() == Board_DIO15_MOTOR_SW2)  // Lock
        {
            UnLock(true);
            Count++;
            System_printf("Count: %d\r\n", Count);
        }
        else  // Unlock
        {
            Lock(true);
            Count++;
            System_printf("Count: %d\r\n", Count);
        }
        */
#ifdef TEST
        /* Detect test pin */
        if(PIN_getInputValue(PIN_ID(Board_DIO5_TEST1)))
        {
#ifdef DEBUG
            System_printf("Detect test1 pin high\r\n");
#endif
        }
        else
        {
#ifdef DEBUG
            System_printf("Detect test1 pin low\r\n");
#endif
        }

        if(PIN_getInputValue(PIN_ID(Board_DIO6_TEST2)))
        {
#ifdef DEBUG
            System_printf("Detect test2 pin high\r\n");
#endif
        }
        else
        {
#ifdef DEBUG
            System_printf("Detect test2 pin low\r\n");
#endif
        }

        if(PIN_getInputValue(PIN_ID(Board_DIO7_TEST3)))
        {
#ifdef DEBUG
            System_printf("Detect test3 pin high\r\n");
#endif
        }
        else
        {
#ifdef DEBUG
            System_printf("Detect test3 pin low\r\n");
#endif
        }
#endif

        /* Detect button1 */
        if(button1Event & EVENT_PRESS)
        {
#ifdef CLOSE_TOUCH_PANEL
          //  PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 1);
            CloseTouch();
#endif
            ui.led[0] = LED_NONE;
            ui.led[1] = LED_G;
            ui.ledTimes[0] = 0;
            ui.ledTimes[1] = 4;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_NONE;
            ui.beepTimes = 0;
#ifdef DEBUG
            System_printf("Button1 press\r\n");
#endif
            button1Event = 0;
            SetUI(ui, true);
#ifdef CLOSE_TOUCH_PANEL
            //PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 0);
#endif
        }
        else if(button1Event & EVENT_PRESS_LONG)
        {
            ui.led[0] = LED_NONE;
            ui.led[1] = LED_G;
            ui.ledTimes[0] = 0;
            ui.ledTimes[1] = 5;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_ON;
            ui.beepTimes = 5;
#ifdef DEBUG
            System_printf("Button1 long press\r\n");
#endif
            button1Event = 0;
            SetUI(ui, true);
        }
        else if(button1Event & EVENT_RELEASE)
        {
#ifdef DEBUG
            System_printf("Button1 release\r\n");
#endif
            button1Event = 0;
        }

        /* Detect button2 */
        if(button2Event & EVENT_PRESS)
        {
#ifdef CLOSE_TOUCH_PANEL
           // PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 1);
            CloseTouch();
#endif
            ui.led[0] = LED_NONE;
            ui.led[1] = LED_R;
            ui.ledTimes[0] = 0;
            ui.ledTimes[1] = 4;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_NONE;
            ui.beepTimes = 0;
#ifdef DEBUG
            System_printf("Button2 press\r\n");
#endif
            button2Event = 0;
            SetUI(ui, true);
#ifdef CLOSE_TOUCH_PANEL
           // PIN_setOutputValue(keypadIntPinHandle, Board_DIO28_KEYPAD_INT, 0);
#endif
        }
        else if(button2Event & EVENT_PRESS_LONG)
        {
            ui.led[0] = LED_NONE;
            ui.led[1] = LED_R;
            ui.ledTimes[0] = 0;
            ui.ledTimes[1] = 5;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_ON;
            ui.beepTimes = 5;
#ifdef DEBUG
            System_printf("Button2 long press\r\n");
#endif
            button2Event = 0;
            SetUI(ui, true);
        }
        else if(button2Event & EVENT_RELEASE)
        {
#ifdef DEBUG
            System_printf("Button2 release\r\n");
#endif
            button2Event = 0;
        }

#ifdef DETECT_SW
        /* Detect SW1 */
        if(!PIN_getInputValue(PIN_ID(Board_DIO14_MOTOR_SW1)))
        {
            ui.led[0] = LED_G;
            ui.led[1] = LED_NONE;
            ui.ledTimes[0] = 5;
            ui.ledTimes[1] = 0;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_NONE;
            ui.beepTimes = 0;
#ifdef DEBUG
            System_printf("SW1 press\r\n");
#endif
            SetUI(ui, true);
        }

        /* Detect SW2 */
        if(!PIN_getInputValue(PIN_ID(Board_DIO15_MOTOR_SW2)))
        {
            ui.led[0] = LED_R;
            ui.led[1] = LED_NONE;
            ui.ledTimes[0] = 5;
            ui.ledTimes[1] = 0;
            ui.ledType[0] = LED_FLASH;
            ui.ledType[1] = LED_FLASH;
            ui.lowPower = LOWPOWER_LED_NONE;
            ui.lowPowerTimes = 0;
            ui.beepType = BEEP_NONE;
            ui.beepTimes = 0;
#ifdef DEBUG
            System_printf("SW2 press\r\n");
#endif
            SetUI(ui, true);
        }
#endif

        Task_sleep(sleepTickCount);
    }
}

/**
  * @brief Create main task.
  * @param none
  * @return none
  */
void MainTaskCreate(void)
{
    /* Construct BIOS objects */
    Task_Params taskParams;

    /* Construct main Task threads */
    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.stack = &maintaskStack;
    taskParams.priority = 1;
    Task_construct(&maintaskStruct, (Task_FuncPtr)maintaskFxn, &taskParams, NULL);
}

/**
  * @brief Initialize mutex.
  * @param none
  * @return none
  */
#ifdef BATTERY_TEST
void InitMutex(void)
{
    /* Construct BIOS objects */
    Semaphore_Params semParams;

    /* Construct a Semaphore object to be use as a resource lock, inital count 0 */
    Semaphore_Params_init(&semParams);
    Semaphore_construct(&semStruct, 0, &semParams);

    /* Obtain instance handle */
    semHandle = Semaphore_handle(&semStruct);
}
#endif
/*
 *  ======== main ========
 */
int main()
{
    /* Call driver init functions */
    Board_init();
    UART_init();
    PWM_init();
    ADC_init();
#ifdef DEBUG
    InitDebugPort();
#endif
    InitGlobalParameter();
#ifdef BATTERY_TEST
    InitMutex();
#endif
    MainTaskCreate();

    BIOS_start();    /* Does not return */
    return(0);
}
