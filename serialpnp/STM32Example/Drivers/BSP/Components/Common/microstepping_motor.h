/**
  ******************************************************************************
  * @file    microstepping_motor.h
  * @author  IPD SYSTEM LAB & TECH MKTG
  * @version V0.0.1
  * @date    04-June-2015
  * @brief   This file contains all the functions prototypes for the microstepping
  *          motor driver with motion engine.   
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MICROSTEPPINGMOTOR_H
#define __MICROSTEPPINGMOTOR_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
   
/** @addtogroup BSP
  * @{
  */

/** @addtogroup Components
  * @{
  */ 

/** @defgroup MicrosteppingMotorDriver
  * @{
  */
    
/** @defgroup StepperMotorExportedTypes
  * @{
  */

/**
  * @brief The L6470 Registers Identifiers.
  */
typedef enum {
  L6470_ABS_POS_ID = 0,         //!< Current position
  L6470_EL_POS_ID,              //!< Electrical position
  L6470_MARK_ID,                //!< Mark position
  L6470_SPEED_ID,               //!< Current speed
  L6470_ACC_ID,                 //!< Acceleration
  L6470_DEC_ID,                 //!< Deceleration
  L6470_MAX_SPEED_ID,           //!< Maximum speed
  L6470_MIN_SPEED_ID,           //!< Minimum speed
  L6470_FS_SPD_ID,              //!< Full-step speed
  L6470_KVAL_HOLD_ID,           //!< Holding KVAL
  L6470_KVAL_RUN_ID,            //!< Constant speed KVAL
  L6470_KVAL_ACC_ID,            //!< Acceleration starting KVAL
  L6470_KVAL_DEC_ID,            //!< Deceleration starting KVAL
  L6470_INT_SPEED_ID,           //!< Intersect speed
  L6470_ST_SLP_ID,              //!< Start slope
  L6470_FN_SLP_ACC_ID,          //!< Acceleration final slope
  L6470_FN_SLP_DEC_ID,          //!< Deceleration final slope
  L6470_K_THERM_ID,             //!< Thermal compensation factor
  L6470_ADC_OUT_ID,             //!< ADC output, (the reset value is according to startup conditions)
  L6470_OCD_TH_ID,              //!< OCD threshold
  L6470_STALL_TH_ID,            //!< STALL threshold
  L6470_STEP_MODE_ID,           //!< Step mode
  L6470_ALARM_EN_ID,            //!< Alarm enable
  L6470_CONFIG_ID,              //!< IC configuration
  L6470_STATUS_ID               //!< Status, (the reset value is according to startup conditions)  
} eL6470_RegId_t;

/**
  * @brief The L6470 Application Commands Identifiers.
  */
typedef enum {
  L6470_NOP_ID = 0,             //!< Nothing
  L6470_SETPARAM_ID,            //!< Writes VALUE in PARAM register
  L6470_GETPARAM_ID,            //!< Returns the stored value in PARAM register
  L6470_RUN_ID,                 //!< Sets the target speed and the motor direction
  L6470_STEPCLOCK_ID,           //!< Puts the device into Step-clock mode and imposes DIR direction
  L6470_MOVE_ID,                //!< Makes N_STEP (micro)steps in DIR direction (Not performable when motor is running)
  L6470_GOTO_ID,                //!< Brings motor into ABS_POS position (minimum path)
  L6470_GOTODIR_ID,             //!< Brings motor into ABS_POS position forcing DIR direction
  L6470_GOUNTIL_ID,             //!< Performs a motion in DIR direction with speed SPD until SW is closed, the ACT action is executed then a SoftStop takes place
  L6470_RELEASESW_ID,           //!< Performs a motion in DIR direction at minimum speed until the SW is released (open), the ACT action is executed then a HardStop takes place
  L6470_GOHOME_ID,              //!< Brings the motor into HOME position
  L6470_GOMARK_ID,              //!< Brings the motor into MARK position
  L6470_RESETPOS_ID,            //!< Resets the ABS_POS register (set HOME position)
  L6470_RESETDEVICE_ID,         //!< Device is reset to power-up conditions
  L6470_SOFTSTOP_ID,            //!< Stops motor with a deceleration phase
  L6470_HARDSTOP_ID,            //!< Stops motor immediately
  L6470_SOFTHIZ_ID,             //!< Puts the bridges into high impedance status after a deceleration phase
  L6470_HARDHIZ_ID,             //!< Puts the bridges into high impedance status immediately
  L6470_GETSTATUS_ID            //!< Returns the STATUS register value
} eL6470_AppCmdId_t;

/**
  * @brief The L6470 Status Register Flag identifiers.
  */
typedef enum {
  HiZ_ID = 0,                       //!< HiZ flag identifier inside the L6470 Status Register
  BUSY_ID,                          //!< BUSY flag identifier inside the L6470 Status Register
  SW_F_ID,                          //!< SW_F flag identifier inside the L6470 Status Register
  SW_EVN_ID,                        //!< SW_EVN flag identifier inside the L6470 Status Register
  DIR_ID,                           //!< DIR flag identifier inside the L6470 Status Register
  MOT_STATUS_ID,                    //!< MOT_STATUS flag identifier inside the L6470 Status Register
  NOTPERF_CMD_ID,                   //!< NOTPERF_CMD flag identifier inside the L6470 Status Register
  WRONG_CMD_ID,                     //!< WRONG_CMD flag identifier inside the L6470 Status Register
  UVLO_ID,                          //!< UVLO flag identifier inside the L6470 Status Register
  TH_WRN_ID,                        //!< TH_WRN flag identifier inside the L6470 Status Register
  TH_SD_ID,                         //!< TH_SD flag identifier inside the L6470 Status Register
  OCD_ID,                           //!< OCD flag identifier inside the L6470 Status Register
  STEP_LOSS_A_ID,                   //!< STEP_LOSS_A flag identifier inside the L6470 Status Register
  STEP_LOSS_B_ID,                   //!< STEP_LOSS_B flag identifier inside the L6470 Status Register
  SCK_MOD_ID                        //!< SCK_MOD flag identifier inside the L6470 Status Register
} eL6470_StatusRegisterFlagId_t;

/**
  * @brief The L6470 Direction identifiers.
  */
typedef enum {
  L6470_DIR_REV_ID = 0,             //!< Reverse direction
  L6470_DIR_FWD_ID                  //!< Forward direction
} eL6470_DirId_t;

/**
  * @brief The L6470 Action identifiers about ABS_POS register.
  */
typedef enum {
  L6470_ACT_RST_ID = 0,             //!< ABS_POS register is reset
  L6470_ACT_CPY_ID                  //!< ABS_POS register value is copied into the MARK register
} eL6470_ActId_t;

/**
  * @brief The L6470 Status Register Flag states.
  */
typedef enum {
  ZERO_F = 0,                       //!< The flag is '0'
  ONE_F = !ZERO_F                   //!< The flag is '1'
} eFlagStatus_t;

/**
  * @brief The L6470 Motor Directions.
  */
typedef enum {
  REVERSE_F = 0,                    //!< Reverse motor direction
  FORWARD_F = !REVERSE_F            //!< Forward motor direction
} eMotorDirection_t;

/**
  * @brief The L6470 Motor Status.
  */
typedef enum {
  STOPPED_F = 0,                    //!< Stopped
  ACCELERATION_F = 1,               //!< Acceleration
  DECELERATION_F = 2,               //!< Deceleration
  CONSTANTSPEED_F = 3               //!< Constant speed
} eMotorStatus_t;

/**
  * @brief  The possible stepping modes for L6470.
  */
typedef enum
{
  FULL_STEP       = 0x00,   //!< Full-step
  HALF_STEP       = 0x01,   //!< Half-step
  MICROSTEP_1_4   = 0x02,   //!< 1/4 microstep
  MICROSTEP_1_8   = 0x03,   //!< 1/8 microstep
  MICROSTEP_1_16  = 0x04,   //!< 1/16 microstep
  MICROSTEP_1_32  = 0x05,   //!< 1/32 microstep
  MICROSTEP_1_64  = 0x06,   //!< 1/64 microstep
  MICROSTEP_1_128 = 0x07    //!< 1/128 microstep
} eMotorStepMode_t;

/**
  * @brief  The identifiers for the possible L6470 alarm conditions.
  */
typedef enum
{
  L6470_OVERCURRENT                       = 0x01, //!< Overcurrent
  L6470_THERMAL_SHUTDOWN                  = 0x02, //!< Thermal shutdown
  L6470_THERMAL_WARNING                   = 0x04, //!< Thermal warning
  L6470_UNDERVOLTAGE                      = 0x08, //!< Undervoltage
  L6470_STALL_DETECTION_A                 = 0x10, //!< Stall detection (Bridge A)
  L6470_STALL_DETECTION_B                 = 0x20, //!< Stall detection (Bridge B)
  L6470_SWITCH_TURN_ON_EVENT              = 0x40, //!< Switch turn-on event
  L6470_WRONG_OR_NON_PERFORMABLE_COMMAND  = 0x80  //!< Wrong or non-performable command
} eL6470_AlarmCondition_t;

/**
  * @brief The L6470 STEP_MODE Register (see L6470 DataSheet for more details).
  */
typedef struct {
  uint8_t STEP_SEL: 3;              //!< Step mode
  uint8_t WRT: 1;                   //!< When the register is written, this bit should be set to 0.
  uint8_t SYNC_SEL: 3;              //!< Synchronization selection
  uint8_t SYNC_EN: 1;               //!< Synchronization enable
} sL6470_StepModeRegister_t;

/**
  * @brief The L6470 ALARM_EN Register (see L6470 DataSheet for more details).
  */
typedef struct {
  uint8_t OCD_EN: 1;                //!< Overcurrent
  uint8_t TH_SD_EN: 1;              //!< Thermal shutdown
  uint8_t TH_WRN_EN: 1;             //!< Thermal warning
  uint8_t UVLO_EN: 1;               //!< Undervoltage
  uint8_t STEP_LOSS_A_EN: 1;        //!< Stall detection (Bridge A)
  uint8_t STEP_LOSS_B_EN: 1;        //!< Stall detection (Bridge B)
  uint8_t SW_EVN_EN: 1;             //!< Switch turn-on event
  uint8_t WRONG_NOTPERF_CMD_EN: 1;  //!< Wrong or non-performable command
} sL6470_AlarmEnRegister_t;

/**
  * @brief The L6470 CONFIG Register (see L6470 DataSheet for more details).
  */
typedef struct {
  uint16_t OSC_SEL: 3;              //!< Oscillator Selection
  uint16_t EXT_CLK: 1;              //!< External Clock
  uint16_t SW_MODE: 1;              //!< Switch mode
  uint16_t EN_VSCOMP: 1;            //!< Motor supply voltage compensation
  uint16_t RESERVED: 1;             //!< RESERVED
  uint16_t OC_SD: 1;                //!< Overcurrent event
  uint16_t POW_SR: 2;               //!< Output slew rate
  uint16_t F_PWM_DEC: 3;            //!< Multiplication factor
  uint16_t F_PWM_INT: 3;            //!< Integer division factor
} sL6470_ConfigRegister_t;

/**
  * @brief The L6470 STATUS Register (see L6470 DataSheet for more details).
  */
typedef struct {
  uint16_t HiZ: 1;                  //!< The bridges are in high impedance state (the flag is active high)
  uint16_t BUSY: 1;                 //!< BUSY pin status (the flag is active low)
  uint16_t SW_F: 1;                 //!< SW input status (the flag is low for open and high for closed)
  uint16_t SW_EVN: 1;               //!< Switch turn-on event (the flag is active high)
  uint16_t DIR: 1;                  //!< The current motor direction (1 as forward, 0 as reverse)
  uint16_t MOT_STATUS: 2;           //!< The current motor status (0 as stopped, 1 as acceleration, 2 as deceleration, 3 as constant speed)
  uint16_t NOTPERF_CMD: 1;          //!< The command received by SPI cannot be performed (the flag is active high)
  uint16_t WRONG_CMD: 1;            //!< The command received by SPI does not exist at all (the flag is active high)
  uint16_t UVLO: 1;                 //!< Undervoltage lockout or reset events (the flag is active low)
  uint16_t TH_WRN: 1;               //!< Thermal warning event (the flag is active low)
  uint16_t TH_SD: 1;                //!< Thermal shutdown event (the flag is active low)
  uint16_t OCD: 1;                  //!< Overcurrent detection event (the flag is active low)
  uint16_t STEP_LOSS_A: 1;          //!< Stall detection on bridge A (the flag is active low)
  uint16_t STEP_LOSS_B: 1;          //!< Stall detection on bridge B (the flag is active low)
  uint16_t SCK_MOD: 1;              //!< Step-clock mode (the flag is active high)
} sL6470_StatusRegister_t;

/**
  * @brief  Motor Parameter Data  
  */ 
typedef struct {
  float     motorvoltage;           //!< motor supply voltage in V
  float     fullstepsperrevolution; //!< min number of steps per revolution for the motor
  float     phasecurrent;           //!< max motor phase voltage in A
  float     phasevoltage;           //!< max motor phase voltage in V
    
  float     speed;                  //!< motor initial speed [step/s]
  float     acc;                    //!< motor acceleration [step/s^2] (comment for infinite acceleration mode)
  float     dec;                    //!< motor deceleration [step/s^2] (comment for infinite deceleration mode)
  float     maxspeed;               //!< motor maximum speed [step/s]
  float     minspeed;               //!< motor minimum speed [step/s]
  float     fsspd;                  //!< motor full-step speed threshold [step/s]
  float     kvalhold;               //!< holding kval [V]
  float     kvalrun;                //!< constant speed kval [V]
  float     kvalacc;                //!< acceleration starting kval [V]
  float     kvaldec;                //!< deceleration starting kval [V]
  float     intspeed;               //!< intersect speed for bemf compensation curve slope changing [step/s]
  float     stslp;                  //!< start slope [s/step]
  float     fnslpacc;               //!< acceleration final slope [s/step]
  float     fnslpdec;               //!< deceleration final slope [s/step]
  uint8_t   kterm;                  //!< thermal compensation factor (range [0, 15])
  float     ocdth;                  //!< ocd threshold [ma] (range [375 ma, 6000 ma])
  float     stallth;                //!< stall threshold [ma] (range [31.25 ma, 4000 ma])
  uint8_t   step_sel;               //!< step mode selection
  uint8_t   alarmen;                //!< alarm conditions enable
  uint16_t  config;                 //!< ic configuration
}MotorParameterData_t;

/**
  * @brief Stepper Motor Registers
  */
typedef struct
{
  uint32_t  ABS_POS;                //!< CurrentPosition Register
  uint16_t  EL_POS;                 //!< ElectricalPosition Register
  uint32_t  MARK;                   //!< MarkPosition Register
  uint32_t  SPEED;                  //!< CurrentSpeed Register
  uint16_t  ACC;                    //!< Acceleration Register
  uint16_t  DEC;                    //!< Deceleration Register
  uint16_t  MAX_SPEED;              //!< MaximumSpeed Register
  uint16_t  MIN_SPEED;              //!< MinimumSpeed Register
  uint16_t  FS_SPD;                 //!< FullStepSpeed Register
  uint8_t   KVAL_HOLD;              //!< HoldingKval Register
  uint8_t   KVAL_RUN;               //!< ConstantSpeedKval Register
  uint8_t   KVAL_ACC;               //!< AccelerationStartingKval Register
  uint8_t   KVAL_DEC;               //!< DecelerationStartingKval Register
  uint16_t  INT_SPEED;              //!< IntersectSpeed Register
  uint8_t   ST_SLP;                 //!< StartSlope Register
  uint8_t   FN_SLP_ACC;             //!< AccelerationFinalSlope Register
  uint8_t   FN_SLP_DEC;             //!< DecelerationFinalSlope Register
  uint8_t   K_THERM;                //!< ThermalCompensationFactor Register
  uint8_t   ADC_OUT;                //!< AdcOutput Register
  uint8_t   OCD_TH;                 //!< OcdThreshold Register
  uint8_t   STALL_TH;               //!< StallThreshold Register
  uint8_t   STEP_MODE;              //!< StepMode Register
  uint8_t   ALARM_EN;               //!< AlarmEnable Register
  uint16_t  CONFIG;                 //!< Config Register
  uint16_t  STATUS;                 //!< Status Register
}StepperMotorRegister_t;

/**
  * @brief Stepper Motor Driver Structure
  */
typedef struct
{
  void (*SetParam)(uint8_t, eL6470_RegId_t, uint32_t);
  uint32_t (*GetParam)(uint8_t, eL6470_RegId_t);
  void (*Run)(uint8_t, eL6470_DirId_t, uint32_t);
  void (*StepClock)(uint8_t, eL6470_DirId_t);
  void (*Move)(uint8_t, eL6470_DirId_t, uint32_t);
  void (*GoTo)(uint8_t L6470_Id, uint32_t AbsPos);
  void (*GoToDir)(uint8_t, eL6470_DirId_t, uint32_t);
  void (*GoUntil)(uint8_t, eL6470_ActId_t, eL6470_DirId_t, uint32_t);
  void (*ReleaseSW)(uint8_t, eL6470_ActId_t, eL6470_DirId_t);
  void (*GoHome)(uint8_t);
  void (*GoMark)(uint8_t);
  void (*ResetPos)(uint8_t);
  void (*ResetDevice)(uint8_t);
  void (*SoftStop)(uint8_t);
  void (*HardStop)(uint8_t);
  void (*SoftHiZ)(uint8_t);
  void (*HardHiZ)(uint8_t);
  uint16_t (*GetStatus)(uint8_t);

  void (*PrepareSetParam)(uint8_t, eL6470_RegId_t, uint32_t);
  void (*PrepareGetParam)(uint8_t, eL6470_RegId_t);
  void (*PrepareRun)(uint8_t, eL6470_DirId_t, uint32_t);
  void (*PrepareStepClock)(uint8_t, eL6470_DirId_t);
  void (*PrepareMove)(uint8_t, eL6470_DirId_t, uint32_t);
  void (*PrepareGoTo)(uint8_t L6470_Id, uint32_t AbsPos);
  void (*PrepareGoToDir)(uint8_t, eL6470_DirId_t, uint32_t);
  void (*PrepareGoUntil)(uint8_t, eL6470_ActId_t, eL6470_DirId_t, uint32_t);
  void (*PrepareReleaseSW)(uint8_t, eL6470_ActId_t, eL6470_DirId_t);
  void (*PrepareGoHome)(uint8_t);
  void (*PrepareGoMark)(uint8_t);
  void (*PrepareResetPos)(uint8_t);
  void (*PrepareResetDevice)(uint8_t);
  void (*PrepareSoftStop)(uint8_t);
  void (*PrepareHardStop)(uint8_t);
  void (*PrepareSoftHiZ)(uint8_t);
  void (*PrepareHardHiZ)(uint8_t);
  void (*PrepareGetStatus)(uint8_t);
  uint8_t (*CheckStatusRegisterFlag)(uint8_t, uint8_t);
}StepperMotorCommand_t;

/**
  * @brief Stepper Motor Board Driver Structure
  */
typedef struct
{
  void (*SetParam)(uint8_t, uint8_t, eL6470_RegId_t, uint32_t);
  uint32_t (*GetParam)(uint8_t, uint8_t, eL6470_RegId_t);
  void (*Run)(uint8_t, uint8_t, eL6470_DirId_t, uint32_t);
  void (*StepClock)(uint8_t, uint8_t, eL6470_DirId_t);
  void (*Move)(uint8_t, uint8_t, eL6470_DirId_t, uint32_t);
  void (*GoTo)(uint8_t, uint8_t L6470_Id, uint32_t AbsPos);
  void (*GoToDir)(uint8_t, uint8_t, eL6470_DirId_t, uint32_t);
  void (*GoUntil)(uint8_t, uint8_t, eL6470_ActId_t, eL6470_DirId_t, uint32_t);
  void (*ReleaseSW)(uint8_t, uint8_t, eL6470_ActId_t, eL6470_DirId_t);
  void (*GoHome)(uint8_t, uint8_t);
  void (*GoMark)(uint8_t, uint8_t);
  void (*ResetPos)(uint8_t, uint8_t);
  void (*ResetDevice)(uint8_t, uint8_t);
  void (*SoftStop)(uint8_t, uint8_t);
  void (*HardStop)(uint8_t, uint8_t);
  void (*SoftHiZ)(uint8_t, uint8_t);
  void (*HardHiZ)(uint8_t, uint8_t);
  uint16_t (*GetStatus)(uint8_t, uint8_t);
  uint8_t (*CheckStatusRegisterFlag)(uint8_t, uint8_t, uint8_t);
  uint8_t* (*PerformPreparedApplicationCommand)(void);
}StepperMotorBoardCommand_t;

/**
  * @brief  Stepper Motor Handle Structure
  */ 
typedef struct __StepperMotorDriver_HandleTypeDef
{
  uint8_t DaisyChainPosition;
  void (*Config)(MotorParameterData_t*);
  StepperMotorCommand_t *Command;
  StepperMotorRegister_t Register;
}StepperMotorDriverHandle_t;

/**
  * @brief  Stepper Motor Handle Structure  
  */ 
typedef struct __StepperMotorBoard_HandleTypeDef
{
  uint8_t StackedPosition;
  void (*Config)(MotorParameterData_t*);
  StepperMotorBoardCommand_t *Command;
  StepperMotorDriverHandle_t *StepperMotorDriverHandle[2];
  uint8_t (*Select)(uint8_t);
}StepperMotorBoardHandle_t;

/**
  * @}
  */    /* StepperMotorExportedTypes */

/**
  * @}
  */    /* MicrosteppingMotorDriver */
  
/**
  * @}
  */    /* Components */

/**
  * @}
  */    /* BSP */

#ifdef __cplusplus
}
#endif

#endif /* __MICROSTEPPINGMOTOR_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
