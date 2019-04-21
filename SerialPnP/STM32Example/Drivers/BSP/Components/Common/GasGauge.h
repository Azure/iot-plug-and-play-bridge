/**
  ******************************************************************************
  * @file    GasGauge.h
  * @author  AST
  * @version V1.0.0
  * @date    6-August-2014
  * @brief   This header file contains the functions prototypes for the gas gauge driver.
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
  

/* Define to prevent recursive inclusion ------------------------------------ */
#ifndef __GASGAUGE_H
#define __GASGAUGE_H

#ifdef __cplusplus
extern "C" {
#endif


/* Includes ------------------------------------------------------------------*/
#include "component.h"

/** @addtogroup BSP BSP
 * @{
 */

/** @addtogroup COMPONENTS COMPONENTS
 * @{
 */

/** @addtogroup COMMON COMMON
 * @{
 */

/** @addtogroup GG GG
 * @{
 */

/** @addtogroup GG_Public_Types GG Public types
 * @{
 */

/** 
  * @brief  GG driver structure definition  
  */ 
typedef struct
{  
	DrvStatusTypeDef ( *Init                     ) ( DrvContextTypeDef* );
        
  DrvStatusTypeDef ( *DeInit         ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Sensor_Enable  ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Sensor_Disable ) ( DrvContextTypeDef* );
  DrvStatusTypeDef ( *Get_WhoAmI     ) ( DrvContextTypeDef*, uint8_t* );
  
	DrvStatusTypeDef ( *Task                     ) ( DrvContextTypeDef*,uint8_t* );
	DrvStatusTypeDef ( *Reset                    ) ( DrvContextTypeDef* );
	DrvStatusTypeDef ( *Stop                     ) ( DrvContextTypeDef* );
	DrvStatusTypeDef ( *GetSOC                   ) ( DrvContextTypeDef*, uint32_t* );
	DrvStatusTypeDef ( *GetOCV                   ) ( DrvContextTypeDef*, uint32_t* );
	DrvStatusTypeDef ( *GetCurrent               ) ( DrvContextTypeDef*, int32_t* );
	DrvStatusTypeDef ( *GetTemperature           ) ( DrvContextTypeDef*, int32_t* );
	DrvStatusTypeDef ( *GetVoltage               ) ( DrvContextTypeDef*, uint32_t* );
	DrvStatusTypeDef ( *GetChargeValue           ) ( DrvContextTypeDef*, uint32_t* );
	DrvStatusTypeDef ( *GetPresence              ) ( DrvContextTypeDef*, uint32_t* );
	
  DrvStatusTypeDef ( *GetAlarmStatus           ) ( DrvContextTypeDef*, uint32_t* );
	DrvStatusTypeDef ( *GetITState               ) ( DrvContextTypeDef*, uint8_t* );
	DrvStatusTypeDef ( *AlarmSetVoltageThreshold ) ( DrvContextTypeDef*, int32_t );
	DrvStatusTypeDef ( *AlarmSetSOCThreshold     ) ( DrvContextTypeDef*, int32_t );
	DrvStatusTypeDef ( *GetIT                    ) ( DrvContextTypeDef*, uint8_t* );
	DrvStatusTypeDef ( *SetIT                    ) ( DrvContextTypeDef* );
	DrvStatusTypeDef ( *StopIT                   ) ( DrvContextTypeDef* );
	DrvStatusTypeDef ( *ClearIT                  ) ( DrvContextTypeDef* ); 
} GG_Drv_t;

/**
 * @brief  GG data structure definition
 */
typedef struct
{
  void *pComponentData; /* Component specific data. */
  void *pExtData;       /* Other data. */
} GG_Data_t;

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */
 
#ifdef __cplusplus
}
#endif
 
#endif /* __GASGAUGE_H */
 


/******************* (C) COPYRIGHT 2010 STMicroelectronics *****END OF FILE****/

