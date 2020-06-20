/**
 ******************************************************************************
 * @file    lis2mdl.c
 * @author  MEMS Software Solutions Team
 * @brief   LIS2MDL driver file
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lis2mdl.h"

/** @addtogroup BSP BSP
 * @{
 */

/** @addtogroup Component Component
 * @{
 */

/** @defgroup LIS2MDL LIS2MDL
 * @{
 */

/** @defgroup LIS2MDL_Exported_Variables LIS2MDL Exported Variables
 * @{
 */

LIS2MDL_CommonDrv_t LIS2MDL_COMMON_Driver =
{
  LIS2MDL_Init,
  LIS2MDL_DeInit,
  LIS2MDL_ReadID,
  LIS2MDL_GetCapabilities,
};

LIS2MDL_MAG_Drv_t LIS2MDL_MAG_Driver =
{
  LIS2MDL_MAG_Enable,
  LIS2MDL_MAG_Disable,
  LIS2MDL_MAG_GetSensitivity,
  LIS2MDL_MAG_GetOutputDataRate,
  LIS2MDL_MAG_SetOutputDataRate,
  LIS2MDL_MAG_GetFullScale,
  LIS2MDL_MAG_SetFullScale,
  LIS2MDL_MAG_GetAxes,
  LIS2MDL_MAG_GetAxesRaw,
};

/**
 * @}
 */

/** @defgroup LIS2MDL_Private_Function_Prototypes LIS2MDL Private Function Prototypes
 * @{
 */
static int32_t ReadMagRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length);
static int32_t WriteMagRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length);

/**
 * @}
 */

/** @defgroup LIS2MDL_Exported_Functions LIS2MDL Exported Functions
 * @{
 */

/**
 * @brief  Register Component Bus IO operations
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_RegisterBusIO(LIS2MDL_Object_t *pObj, LIS2MDL_IO_t *pIO)
{
  int32_t ret = LIS2MDL_OK;

  if (pObj == NULL)
  {
    ret = LIS2MDL_ERROR;
  }
  else
  {
    pObj->IO.Init      = pIO->Init;
    pObj->IO.DeInit    = pIO->DeInit;
    pObj->IO.BusType   = pIO->BusType;
    pObj->IO.Address   = pIO->Address;
    pObj->IO.WriteReg  = pIO->WriteReg;
    pObj->IO.ReadReg   = pIO->ReadReg;
    pObj->IO.GetTick   = pIO->GetTick;

    pObj->Ctx.read_reg  = ReadMagRegWrap;
    pObj->Ctx.write_reg = WriteMagRegWrap;
    pObj->Ctx.handle    = pObj;

    if (pObj->IO.Init == NULL)
    {
      ret = LIS2MDL_ERROR;
    }
    else if (pObj->IO.Init() != LIS2MDL_OK)
    {
      ret = LIS2MDL_ERROR;
    }
    else
    {
      if (pObj->IO.BusType != LIS2MDL_I2C_BUS) /* If the bus type is not I2C */
      {
        /* Disable I2C interface support and enable eventually SPI 4-Wires only the first time */
        if (pObj->is_initialized == 0U)
        {
          if (pObj->IO.BusType == LIS2MDL_SPI_4WIRES_BUS) /* SPI 4-Wires */
          {
            /* Enable SPI 4-Wires and disable I2C support on the component */
            uint8_t data = 0x34;

            if (LIS2MDL_Write_Reg(pObj, LIS2MDL_CFG_REG_C, data) != LIS2MDL_OK)
            {
              return LIS2MDL_ERROR;
            }
          }
          else
          {
            /* Disable I2C interface on the component */
            if (lis2mdl_i2c_interface_set(&(pObj->Ctx), LIS2MDL_I2C_DISABLE) != LIS2MDL_OK)
            {
              return LIS2MDL_ERROR;
            }
          }
        }
      }
    }
  }

  return ret;
}

/**
 * @brief  Initialize the LIS2MDL sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_Init(LIS2MDL_Object_t *pObj)
{
  /* Enable BDU */
  if (lis2mdl_block_data_update_set(&(pObj->Ctx), PROPERTY_ENABLE) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  /* Operating mode selection - power down */
  if (lis2mdl_operating_mode_set(&(pObj->Ctx), LIS2MDL_POWER_DOWN) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  /* Output data rate selection */
  if (lis2mdl_data_rate_set(&(pObj->Ctx), LIS2MDL_ODR_100Hz) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  /* Self Test disabled. */
  if (lis2mdl_self_test_set(&(pObj->Ctx), PROPERTY_DISABLE) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  pObj->is_initialized = 1;

  return LIS2MDL_OK;
}

/**
 * @brief  Deinitialize the LIS2MDL magnetometer sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_DeInit(LIS2MDL_Object_t *pObj)
{
  /* Disable the component */
  if (LIS2MDL_MAG_Disable(pObj) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  pObj->is_initialized = 0;

  return LIS2MDL_OK;
}

/**
 * @brief  Read component ID
 * @param  pObj the device pObj
 * @param  Id the WHO_AM_I value
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_ReadID(LIS2MDL_Object_t *pObj, uint8_t *Id)
{
  if (lis2mdl_device_id_get(&(pObj->Ctx), Id) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @brief  Get LIS2MDL magnetometer sensor capabilities
 * @param  pObj Component object pointer
 * @param  Capabilities pointer to LIS2MDL magnetometer sensor capabilities
 * @retval Component status
 */
int32_t LIS2MDL_GetCapabilities(LIS2MDL_Object_t *pObj, LIS2MDL_Capabilities_t *Capabilities)
{
  /* Prevent unused argument(s) compilation warning */
  (void)(pObj);

  Capabilities->Acc          = 0;
  Capabilities->Gyro         = 0;
  Capabilities->Magneto      = 1;
  Capabilities->LowPower     = 0;
  Capabilities->GyroMaxFS    = 0;
  Capabilities->AccMaxFS     = 0;
  Capabilities->MagMaxFS     = 50;
  Capabilities->GyroMaxOdr   = 0.0f;
  Capabilities->AccMaxOdr    = 0.0f;
  Capabilities->MagMaxOdr    = 100.0f;
  return LIS2MDL_OK;
}

/**
 * @brief Enable the LIS2MDL magnetometer sensor
 * @param pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Enable(LIS2MDL_Object_t *pObj)
{
  /* Check if the component is already enabled */
  if (pObj->mag_is_enabled == 1U)
  {
    return LIS2MDL_OK;
  }

  /* Output data rate selection. */
  if (lis2mdl_operating_mode_set(&(pObj->Ctx), LIS2MDL_CONTINUOUS_MODE) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  pObj->mag_is_enabled = 1;

  return LIS2MDL_OK;
}

/**
 * @brief Disable the LIS2MDL magnetometer sensor
 * @param pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Disable(LIS2MDL_Object_t *pObj)
{
  /* Check if the component is already disabled */
  if (pObj->mag_is_enabled == 0U)
  {
    return LIS2MDL_OK;
  }

  /* Output data rate selection - power down. */
  if (lis2mdl_operating_mode_set(&(pObj->Ctx), LIS2MDL_POWER_DOWN) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  pObj->mag_is_enabled = 0;

  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL magnetometer sensor sensitivity
 * @param  pObj the device pObj
 * @param  Sensitivity pointer
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_GetSensitivity(LIS2MDL_Object_t *pObj, float *Sensitivity)
{
  *Sensitivity = LIS2MDL_MAG_SENSITIVITY_FS_50GAUSS;

  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL magnetometer sensor output data rate
 * @param  pObj the device pObj
 * @param  Odr pointer where the output data rate is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_GetOutputDataRate(LIS2MDL_Object_t *pObj, float *Odr)
{
  int32_t ret = LIS2MDL_OK;
  lis2mdl_odr_t odr_low_level;

  /* Get current output data rate. */
  if (lis2mdl_data_rate_get(&(pObj->Ctx), &odr_low_level) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  switch (odr_low_level)
  {
    case LIS2MDL_ODR_10Hz:
      *Odr = 10.0f;
      break;

    case LIS2MDL_ODR_20Hz:
      *Odr = 20.0f;
      break;

    case LIS2MDL_ODR_50Hz:
      *Odr = 50.0f;
      break;

    case LIS2MDL_ODR_100Hz:
      *Odr = 100.0f;
      break;

    default:
      ret = LIS2MDL_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Set the LIS2MDL magnetometer sensor output data rate
 * @param  pObj the device pObj
 * @param  Odr the output data rate value to be set
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_SetOutputDataRate(LIS2MDL_Object_t *pObj, float Odr)
{
  lis2mdl_odr_t new_odr;

  new_odr = (Odr <= 10.000f) ? LIS2MDL_ODR_10Hz
            : (Odr <= 20.000f) ? LIS2MDL_ODR_20Hz
            : (Odr <= 50.000f) ? LIS2MDL_ODR_50Hz
            :                    LIS2MDL_ODR_100Hz;

  if (lis2mdl_data_rate_set(&(pObj->Ctx), new_odr) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}


/**
 * @brief  Get the LIS2MDL magnetometer sensor full scale
 * @param  pObj the device pObj
 * @param  FullScale pointer where the full scale is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_GetFullScale(LIS2MDL_Object_t *pObj, int32_t *FullScale)
{
  *FullScale = 50;

  return LIS2MDL_OK;
}

/**
 * @brief  Set the LIS2MDL magnetometer sensor full scale
 * @param  pObj the device pObj
 * @param  FullScale the functional full scale to be set
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_SetFullScale(LIS2MDL_Object_t *pObj, int32_t FullScale)
{
  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL magnetometer sensor raw axes
 * @param  pObj the device pObj
 * @param  Value pointer where the raw values of the axes are written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_GetAxesRaw(LIS2MDL_Object_t *pObj, LIS2MDL_AxesRaw_t *Value)
{
  lis2mdl_axis3bit16_t data_raw;

  /* Read raw data values. */
  if (lis2mdl_magnetic_raw_get(&(pObj->Ctx), data_raw.u8bit) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  /* Format the data. */
  Value->x = data_raw.i16bit[0];
  Value->y = data_raw.i16bit[1];
  Value->z = data_raw.i16bit[2];

  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL magnetometer sensor axes
 * @param  pObj the device pObj
 * @param  MagneticField pointer where the values of the axes are written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_GetAxes(LIS2MDL_Object_t *pObj, LIS2MDL_Axes_t *MagneticField)
{
  lis2mdl_axis3bit16_t data_raw;
  float sensitivity;

  /* Read raw data values. */
  if (lis2mdl_magnetic_raw_get(&(pObj->Ctx), data_raw.u8bit) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  /* Get LIS2MDL actual sensitivity. */
  (void)LIS2MDL_MAG_GetSensitivity(pObj, &sensitivity);

  /* Calculate the data. */
  MagneticField->x = (int32_t)((float)((float)data_raw.i16bit[0] * sensitivity));
  MagneticField->y = (int32_t)((float)((float)data_raw.i16bit[1] * sensitivity));
  MagneticField->z = (int32_t)((float)((float)data_raw.i16bit[2] * sensitivity));

  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL register value for magnetic sensor
 * @param  pObj the device pObj
 * @param  Reg address to be read
 * @param  Data pointer where the value is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_Read_Reg(LIS2MDL_Object_t *pObj, uint8_t Reg, uint8_t *Data)
{
  if (lis2mdl_read_reg(&(pObj->Ctx), Reg, Data, 1) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @brief  Set the LIS2MDL register value for magnetic sensor
 * @param  pObj the device pObj
 * @param  Reg address to be written
 * @param  Data value to be written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_Write_Reg(LIS2MDL_Object_t *pObj, uint8_t Reg, uint8_t Data)
{
  if (lis2mdl_write_reg(&(pObj->Ctx), Reg, &Data, 1) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @brief  Set self test
 * @param  pObj the device pObj
 * @param  val the value of self_test in reg CFG_REG_C
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Set_SelfTest(LIS2MDL_Object_t *pObj, uint8_t val)
{
  if (lis2mdl_self_test_set(&(pObj->Ctx), val) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL MAG data ready bit value
 * @param  pObj the device pObj
 * @param  Status the status of data ready bit
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Get_DRDY_Status(LIS2MDL_Object_t *pObj, uint8_t *Status)
{
  if (lis2mdl_mag_data_ready_get(&(pObj->Ctx), Status) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @brief  Get the LIS2MDL MAG initialization status
 * @param  pObj the device pObj
 * @param  Status 1 if initialized, 0 otherwise
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Get_Init_Status(LIS2MDL_Object_t *pObj, uint8_t *Status)
{
  if (pObj == NULL)
  {
    return LIS2MDL_ERROR;
  }

  *Status = pObj->is_initialized;

  return LIS2MDL_OK;
}

/**
 * @brief  Set the LIS2MDL filter mode
 * @param  pObj the device pObj
 * @param  FilterMode Value of the filter mode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Set_Filter_Mode(LIS2MDL_Object_t *pObj, uint8_t FilterMode)
{
  if(lis2mdl_low_pass_bandwidth_set(&(pObj->Ctx), (lis2mdl_lpf_t)FilterMode) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @brief  Set the LIS2MDL power mode
 * @param  pObj the device pObj
 * @param  PowerMode Value of the power mode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LIS2MDL_MAG_Set_Power_Mode(LIS2MDL_Object_t *pObj, uint8_t PowerMode)
{
  if(lis2mdl_power_mode_set(&(pObj->Ctx), (lis2mdl_lp_t)PowerMode) != LIS2MDL_OK)
  {
    return LIS2MDL_ERROR;
  }

  return LIS2MDL_OK;
}

/**
 * @}
 */

/** @defgroup LIS2MDL_Private_Functions LIS2MDL Private Functions
 * @{
 */

/**
 * @brief  Wrap Read register component function to Bus IO function
 * @param  Handle the device handler
 * @param  Reg the register address
 * @param  pData the stored data pointer
 * @param  Length the length
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t ReadMagRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length)
{
  LIS2MDL_Object_t *pObj = (LIS2MDL_Object_t *)Handle;

  if (pObj->IO.BusType == LIS2MDL_I2C_BUS) /* I2C */
  {
    /* Enable Multi-byte read */
    return pObj->IO.ReadReg(pObj->IO.Address, (Reg | 0x80U), pData, Length);
  }
  else   /* SPI 3-Wires or SPI 4-Wires */
  {
    /* Enable Multi-byte read */
    return pObj->IO.ReadReg(pObj->IO.Address, (Reg | 0x40U), pData, Length);
  }
}

/**
 * @brief  Wrap Write register component function to Bus IO function
 * @param  Handle the device handler
 * @param  Reg the register address
 * @param  pData the stored data pointer
 * @param  Length the length
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t WriteMagRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length)
{
  LIS2MDL_Object_t *pObj = (LIS2MDL_Object_t *)Handle;

  if (pObj->IO.BusType == LIS2MDL_I2C_BUS) /* I2C */
  {
    /* Enable Multi-byte write */
    return pObj->IO.WriteReg(pObj->IO.Address, (Reg | 0x80U), pData, Length);
  }
  else   /* SPI 3-Wires or SPI 4-Wires */
  {
    /* Enable Multi-byte write */
    return pObj->IO.WriteReg(pObj->IO.Address, (Reg | 0x40U), pData, Length);
  }
}

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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
