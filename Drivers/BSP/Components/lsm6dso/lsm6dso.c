/**
 ******************************************************************************
 * @file    lsm6dso.c
 * @author  MEMS Software Solutions Team
 * @brief   LSM6DSO driver file
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
#include "lsm6dso.h"

/** @addtogroup BSP BSP
 * @{
 */

/** @addtogroup Component Component
 * @{
 */

/** @defgroup LSM6DSO LSM6DSO
 * @{
 */

/** @defgroup LSM6DSO_Exported_Variables LSM6DSO Exported Variables
 * @{
 */

LSM6DSO_CommonDrv_t LSM6DSO_COMMON_Driver =
{
  LSM6DSO_Init,
  LSM6DSO_DeInit,
  LSM6DSO_ReadID,
  LSM6DSO_GetCapabilities,
};

LSM6DSO_ACC_Drv_t LSM6DSO_ACC_Driver =
{
  LSM6DSO_ACC_Enable,
  LSM6DSO_ACC_Disable,
  LSM6DSO_ACC_GetSensitivity,
  LSM6DSO_ACC_GetOutputDataRate,
  LSM6DSO_ACC_SetOutputDataRate,
  LSM6DSO_ACC_GetFullScale,
  LSM6DSO_ACC_SetFullScale,
  LSM6DSO_ACC_GetAxes,
  LSM6DSO_ACC_GetAxesRaw,
};

LSM6DSO_GYRO_Drv_t LSM6DSO_GYRO_Driver =
{
  LSM6DSO_GYRO_Enable,
  LSM6DSO_GYRO_Disable,
  LSM6DSO_GYRO_GetSensitivity,
  LSM6DSO_GYRO_GetOutputDataRate,
  LSM6DSO_GYRO_SetOutputDataRate,
  LSM6DSO_GYRO_GetFullScale,
  LSM6DSO_GYRO_SetFullScale,
  LSM6DSO_GYRO_GetAxes,
  LSM6DSO_GYRO_GetAxesRaw,
};

/**
 * @}
 */

/** @defgroup LSM6DSO_Private_Function_Prototypes LSM6DSO Private Function Prototypes
 * @{
 */

static int32_t LSM6DSO_ACC_SetOutputDataRate_When_Enabled(LSM6DSO_Object_t *pObj, float Odr);
static int32_t LSM6DSO_ACC_SetOutputDataRate_When_Disabled(LSM6DSO_Object_t *pObj, float Odr);
static int32_t LSM6DSO_GYRO_SetOutputDataRate_When_Enabled(LSM6DSO_Object_t *pObj, float Odr);
static int32_t LSM6DSO_GYRO_SetOutputDataRate_When_Disabled(LSM6DSO_Object_t *pObj, float Odr);
static int32_t ReadRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length);
static int32_t WriteRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length);

/**
 * @}
 */

/** @defgroup LSM6DSO_Exported_Functions LSM6DSO Exported Functions
 * @{
 */

/**
 * @brief  Register Component Bus IO operations
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_RegisterBusIO(LSM6DSO_Object_t *pObj, LSM6DSO_IO_t *pIO)
{
  int32_t ret = LSM6DSO_OK;

  if (pObj == NULL)
  {
    ret = LSM6DSO_ERROR;
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

    pObj->Ctx.read_reg  = ReadRegWrap;
    pObj->Ctx.write_reg = WriteRegWrap;
    pObj->Ctx.handle   = pObj;

    if (pObj->IO.Init == NULL)
    {
      ret = LSM6DSO_ERROR;
    }
    else if (pObj->IO.Init() != LSM6DSO_OK)
    {
      ret = LSM6DSO_ERROR;
    }
    else
    {
      if (pObj->IO.BusType == LSM6DSO_SPI_3WIRES_BUS) /* SPI 3-Wires */
      {
        /* Enable the SPI 3-Wires support only the first time */
        if (pObj->is_initialized == 0U)
        {
          /* Enable SPI 3-Wires on the component */
          uint8_t data = 0x0C;

          if (LSM6DSO_Write_Reg(pObj, LSM6DSO_CTRL3_C, data) != LSM6DSO_OK)
          {
            ret = LSM6DSO_ERROR;
          }
        }
      }
    }
  }

  return ret;
}

/**
 * @brief  Initialize the LSM6DSO sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_Init(LSM6DSO_Object_t *pObj)
{
  /* Disable I3C */
  if (lsm6dso_i3c_disable_set(&(pObj->Ctx), LSM6DSO_I3C_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable register address automatically incremented during a multiple byte
  access with a serial interface. */
  if (lsm6dso_auto_increment_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable BDU */
  if (lsm6dso_block_data_update_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* FIFO mode selection */
  if (lsm6dso_fifo_mode_set(&(pObj->Ctx), LSM6DSO_BYPASS_MODE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Select default output data rate. */
  pObj->acc_odr = LSM6DSO_XL_ODR_104Hz;

  /* Output data rate selection - power down. */
  if (lsm6dso_xl_data_rate_set(&(pObj->Ctx), LSM6DSO_XL_ODR_OFF) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection. */
  if (lsm6dso_xl_full_scale_set(&(pObj->Ctx), LSM6DSO_2g) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Select default output data rate. */
  pObj->gyro_odr = LSM6DSO_GY_ODR_104Hz;

  /* Output data rate selection - power down. */
  if (lsm6dso_gy_data_rate_set(&(pObj->Ctx), LSM6DSO_GY_ODR_OFF) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection. */
  if (lsm6dso_gy_full_scale_set(&(pObj->Ctx), LSM6DSO_2000dps) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  pObj->is_initialized = 1;

  return LSM6DSO_OK;
}

/**
 * @brief  Deinitialize the LSM6DSO sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_DeInit(LSM6DSO_Object_t *pObj)
{
  /* Disable the component */
  if (LSM6DSO_ACC_Disable(pObj) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (LSM6DSO_GYRO_Disable(pObj) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset output data rate. */
  pObj->acc_odr = LSM6DSO_XL_ODR_OFF;
  pObj->gyro_odr = LSM6DSO_GY_ODR_OFF;

  pObj->is_initialized = 0;

  return LSM6DSO_OK;
}

/**
 * @brief  Read component ID
 * @param  pObj the device pObj
 * @param  Id the WHO_AM_I value
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ReadID(LSM6DSO_Object_t *pObj, uint8_t *Id)
{
  if (lsm6dso_device_id_get(&(pObj->Ctx), Id) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get LSM6DSO sensor capabilities
 * @param  pObj Component object pointer
 * @param  Capabilities pointer to LSM6DSO sensor capabilities
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GetCapabilities(LSM6DSO_Object_t *pObj, LSM6DSO_Capabilities_t *Capabilities)
{
  /* Prevent unused argument(s) compilation warning */
  (void)(pObj);

  Capabilities->Acc          = 1;
  Capabilities->Gyro         = 1;
  Capabilities->Magneto      = 0;
  Capabilities->LowPower     = 0;
  Capabilities->GyroMaxFS    = 2000;
  Capabilities->AccMaxFS     = 16;
  Capabilities->MagMaxFS     = 0;
  Capabilities->GyroMaxOdr   = 6660.0f;
  Capabilities->AccMaxOdr    = 6660.0f;
  Capabilities->MagMaxOdr    = 0.0f;
  return LSM6DSO_OK;
}

/**
 * @brief  Enable the LSM6DSO accelerometer sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable(LSM6DSO_Object_t *pObj)
{
  /* Check if the component is already enabled */
  if (pObj->acc_is_enabled == 1U)
  {
    return LSM6DSO_OK;
  }

  /* Output data rate selection. */
  if (lsm6dso_xl_data_rate_set(&(pObj->Ctx), pObj->acc_odr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  pObj->acc_is_enabled = 1;

  return LSM6DSO_OK;
}

/**
 * @brief  Disable the LSM6DSO accelerometer sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable(LSM6DSO_Object_t *pObj)
{
  /* Check if the component is already disabled */
  if (pObj->acc_is_enabled == 0U)
  {
    return LSM6DSO_OK;
  }

  /* Get current output data rate. */
  if (lsm6dso_xl_data_rate_get(&(pObj->Ctx), &pObj->acc_odr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Output data rate selection - power down. */
  if (lsm6dso_xl_data_rate_set(&(pObj->Ctx), LSM6DSO_XL_ODR_OFF) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  pObj->acc_is_enabled = 0;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO accelerometer sensor sensitivity
 * @param  pObj the device pObj
 * @param  Sensitivity pointer
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_GetSensitivity(LSM6DSO_Object_t *pObj, float *Sensitivity)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_fs_xl_t full_scale;

  /* Read actual full scale selection from sensor. */
  if (lsm6dso_xl_full_scale_get(&(pObj->Ctx), &full_scale) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Store the Sensitivity based on actual full scale. */
  switch (full_scale)
  {
    case LSM6DSO_2g:
      *Sensitivity = LSM6DSO_ACC_SENSITIVITY_FS_2G;
      break;

    case LSM6DSO_4g:
      *Sensitivity = LSM6DSO_ACC_SENSITIVITY_FS_4G;
      break;

    case LSM6DSO_8g:
      *Sensitivity = LSM6DSO_ACC_SENSITIVITY_FS_8G;
      break;

    case LSM6DSO_16g:
      *Sensitivity = LSM6DSO_ACC_SENSITIVITY_FS_16G;
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Get the LSM6DSO accelerometer sensor output data rate
 * @param  pObj the device pObj
 * @param  Odr pointer where the output data rate is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_GetOutputDataRate(LSM6DSO_Object_t *pObj, float *Odr)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_odr_xl_t odr_low_level;

  /* Get current output data rate. */
  if (lsm6dso_xl_data_rate_get(&(pObj->Ctx), &odr_low_level) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  switch (odr_low_level)
  {
    case LSM6DSO_XL_ODR_OFF:
      *Odr = 0.0f;
      break;

    case LSM6DSO_XL_ODR_6Hz5:
      *Odr = 6.5f;
      break;

    case LSM6DSO_XL_ODR_12Hz5:
      *Odr = 12.5f;
      break;

    case LSM6DSO_XL_ODR_26Hz:
      *Odr = 26.0f;
      break;

    case LSM6DSO_XL_ODR_52Hz:
      *Odr = 52.0f;
      break;

    case LSM6DSO_XL_ODR_104Hz:
      *Odr = 104.0f;
      break;

    case LSM6DSO_XL_ODR_208Hz:
      *Odr = 208.0f;
      break;

    case LSM6DSO_XL_ODR_417Hz:
      *Odr = 417.0f;
      break;

    case LSM6DSO_XL_ODR_833Hz:
      *Odr = 833.0f;
      break;

    case LSM6DSO_XL_ODR_1667Hz:
      *Odr = 1667.0f;
      break;

    case LSM6DSO_XL_ODR_3333Hz:
      *Odr = 3333.0f;
      break;

    case LSM6DSO_XL_ODR_6667Hz:
      *Odr = 6667.0f;
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Set the LSM6DSO accelerometer sensor output data rate
 * @param  pObj the device pObj
 * @param  Odr the output data rate value to be set
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_SetOutputDataRate(LSM6DSO_Object_t *pObj, float Odr)
{
  /* Check if the component is enabled */
  if (pObj->acc_is_enabled == 1U)
  {
    return LSM6DSO_ACC_SetOutputDataRate_When_Enabled(pObj, Odr);
  }
  else
  {
    return LSM6DSO_ACC_SetOutputDataRate_When_Disabled(pObj, Odr);
  }
}

/**
 * @brief  Get the LSM6DSO accelerometer sensor full scale
 * @param  pObj the device pObj
 * @param  FullScale pointer where the full scale is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_GetFullScale(LSM6DSO_Object_t *pObj, int32_t *FullScale)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_fs_xl_t fs_low_level;

  /* Read actual full scale selection from sensor. */
  if (lsm6dso_xl_full_scale_get(&(pObj->Ctx), &fs_low_level) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  switch (fs_low_level)
  {
    case LSM6DSO_2g:
      *FullScale =  2;
      break;

    case LSM6DSO_4g:
      *FullScale =  4;
      break;

    case LSM6DSO_8g:
      *FullScale =  8;
      break;

    case LSM6DSO_16g:
      *FullScale = 16;
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Set the LSM6DSO accelerometer sensor full scale
 * @param  pObj the device pObj
 * @param  FullScale the functional full scale to be set
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_SetFullScale(LSM6DSO_Object_t *pObj, int32_t FullScale)
{
  lsm6dso_fs_xl_t new_fs;

  /* Seems like MISRA C-2012 rule 14.3a violation but only from single file statical analysis point of view because
     the parameter passed to the function is not known at the moment of analysis */
  new_fs = (FullScale <= 2) ? LSM6DSO_2g
           : (FullScale <= 4) ? LSM6DSO_4g
           : (FullScale <= 8) ? LSM6DSO_8g
           :                    LSM6DSO_16g;

  if (lsm6dso_xl_full_scale_set(&(pObj->Ctx), new_fs) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO accelerometer sensor raw axes
 * @param  pObj the device pObj
 * @param  Value pointer where the raw values of the axes are written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_GetAxesRaw(LSM6DSO_Object_t *pObj, LSM6DSO_AxesRaw_t *Value)
{
  lsm6dso_axis3bit16_t data_raw;

  /* Read raw data values. */
  if (lsm6dso_acceleration_raw_get(&(pObj->Ctx), data_raw.u8bit) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Format the data. */
  Value->x = data_raw.i16bit[0];
  Value->y = data_raw.i16bit[1];
  Value->z = data_raw.i16bit[2];

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO accelerometer sensor axes
 * @param  pObj the device pObj
 * @param  Acceleration pointer where the values of the axes are written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_GetAxes(LSM6DSO_Object_t *pObj, LSM6DSO_Axes_t *Acceleration)
{
  lsm6dso_axis3bit16_t data_raw;
  float sensitivity = 0.0f;

  /* Read raw data values. */
  if (lsm6dso_acceleration_raw_get(&(pObj->Ctx), data_raw.u8bit) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Get LSM6DSO actual sensitivity. */
  if (LSM6DSO_ACC_GetSensitivity(pObj, &sensitivity) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Calculate the data. */
  Acceleration->x = (int32_t)((float)((float)data_raw.i16bit[0] * sensitivity));
  Acceleration->y = (int32_t)((float)((float)data_raw.i16bit[1] * sensitivity));
  Acceleration->z = (int32_t)((float)((float)data_raw.i16bit[2] * sensitivity));

  return LSM6DSO_OK;
}

/**
 * @brief  Enable the LSM6DSO gyroscope sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Enable(LSM6DSO_Object_t *pObj)
{
  /* Check if the component is already enabled */
  if (pObj->gyro_is_enabled == 1U)
  {
    return LSM6DSO_OK;
  }

  /* Output data rate selection. */
  if (lsm6dso_gy_data_rate_set(&(pObj->Ctx), pObj->gyro_odr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  pObj->gyro_is_enabled = 1;

  return LSM6DSO_OK;
}

/**
 * @brief  Disable the LSM6DSO gyroscope sensor
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Disable(LSM6DSO_Object_t *pObj)
{
  /* Check if the component is already disabled */
  if (pObj->gyro_is_enabled == 0U)
  {
    return LSM6DSO_OK;
  }

  /* Get current output data rate. */
  if (lsm6dso_gy_data_rate_get(&(pObj->Ctx), &pObj->gyro_odr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Output data rate selection - power down. */
  if (lsm6dso_gy_data_rate_set(&(pObj->Ctx), LSM6DSO_GY_ODR_OFF) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  pObj->gyro_is_enabled = 0;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO gyroscope sensor sensitivity
 * @param  pObj the device pObj
 * @param  Sensitivity pointer
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_GetSensitivity(LSM6DSO_Object_t *pObj, float *Sensitivity)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_fs_g_t full_scale;

  /* Read actual full scale selection from sensor. */
  if (lsm6dso_gy_full_scale_get(&(pObj->Ctx), &full_scale) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Store the sensitivity based on actual full scale. */
  switch (full_scale)
  {
    case LSM6DSO_125dps:
      *Sensitivity = LSM6DSO_GYRO_SENSITIVITY_FS_125DPS;
      break;

    case LSM6DSO_250dps:
      *Sensitivity = LSM6DSO_GYRO_SENSITIVITY_FS_250DPS;
      break;

    case LSM6DSO_500dps:
      *Sensitivity = LSM6DSO_GYRO_SENSITIVITY_FS_500DPS;
      break;

    case LSM6DSO_1000dps:
      *Sensitivity = LSM6DSO_GYRO_SENSITIVITY_FS_1000DPS;
      break;

    case LSM6DSO_2000dps:
      *Sensitivity = LSM6DSO_GYRO_SENSITIVITY_FS_2000DPS;
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Get the LSM6DSO gyroscope sensor output data rate
 * @param  pObj the device pObj
 * @param  Odr pointer where the output data rate is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_GetOutputDataRate(LSM6DSO_Object_t *pObj, float *Odr)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_odr_g_t odr_low_level;

  /* Get current output data rate. */
  if (lsm6dso_gy_data_rate_get(&(pObj->Ctx), &odr_low_level) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  switch (odr_low_level)
  {
    case LSM6DSO_GY_ODR_OFF:
      *Odr = 0.0f;
      break;

    case LSM6DSO_GY_ODR_12Hz5:
      *Odr = 12.5f;
      break;

    case LSM6DSO_GY_ODR_26Hz:
      *Odr = 26.0f;
      break;

    case LSM6DSO_GY_ODR_52Hz:
      *Odr = 52.0f;
      break;

    case LSM6DSO_GY_ODR_104Hz:
      *Odr = 104.0f;
      break;

    case LSM6DSO_GY_ODR_208Hz:
      *Odr = 208.0f;
      break;

    case LSM6DSO_GY_ODR_417Hz:
      *Odr = 417.0f;
      break;

    case LSM6DSO_GY_ODR_833Hz:
      *Odr = 833.0f;
      break;

    case LSM6DSO_GY_ODR_1667Hz:
      *Odr = 1667.0f;
      break;

    case LSM6DSO_GY_ODR_3333Hz:
      *Odr = 3333.0f;
      break;

    case LSM6DSO_GY_ODR_6667Hz:
      *Odr = 6667.0f;
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Set the LSM6DSO gyroscope sensor output data rate
 * @param  pObj the device pObj
 * @param  Odr the output data rate value to be set
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_SetOutputDataRate(LSM6DSO_Object_t *pObj, float Odr)
{
  /* Check if the component is enabled */
  if (pObj->gyro_is_enabled == 1U)
  {
    return LSM6DSO_GYRO_SetOutputDataRate_When_Enabled(pObj, Odr);
  }
  else
  {
    return LSM6DSO_GYRO_SetOutputDataRate_When_Disabled(pObj, Odr);
  }
}

/**
 * @brief  Get the LSM6DSO gyroscope sensor full scale
 * @param  pObj the device pObj
 * @param  FullScale pointer where the full scale is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_GetFullScale(LSM6DSO_Object_t *pObj, int32_t  *FullScale)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_fs_g_t fs_low_level;

  /* Read actual full scale selection from sensor. */
  if (lsm6dso_gy_full_scale_get(&(pObj->Ctx), &fs_low_level) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  switch (fs_low_level)
  {
    case LSM6DSO_125dps:
      *FullScale =  125;
      break;

    case LSM6DSO_250dps:
      *FullScale =  250;
      break;

    case LSM6DSO_500dps:
      *FullScale =  500;
      break;

    case LSM6DSO_1000dps:
      *FullScale = 1000;
      break;

    case LSM6DSO_2000dps:
      *FullScale = 2000;
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Set the LSM6DSO gyroscope sensor full scale
 * @param  pObj the device pObj
 * @param  FullScale the functional full scale to be set
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_SetFullScale(LSM6DSO_Object_t *pObj, int32_t FullScale)
{
  lsm6dso_fs_g_t new_fs;

  new_fs = (FullScale <= 125)  ? LSM6DSO_125dps
           : (FullScale <= 250)  ? LSM6DSO_250dps
           : (FullScale <= 500)  ? LSM6DSO_500dps
           : (FullScale <= 1000) ? LSM6DSO_1000dps
           :                       LSM6DSO_2000dps;

  if (lsm6dso_gy_full_scale_set(&(pObj->Ctx), new_fs) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO gyroscope sensor raw axes
 * @param  pObj the device pObj
 * @param  Value pointer where the raw values of the axes are written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_GetAxesRaw(LSM6DSO_Object_t *pObj, LSM6DSO_AxesRaw_t *Value)
{
  lsm6dso_axis3bit16_t data_raw;

  /* Read raw data values. */
  if (lsm6dso_angular_rate_raw_get(&(pObj->Ctx), data_raw.u8bit) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Format the data. */
  Value->x = data_raw.i16bit[0];
  Value->y = data_raw.i16bit[1];
  Value->z = data_raw.i16bit[2];

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO gyroscope sensor axes
 * @param  pObj the device pObj
 * @param  AngularRate pointer where the values of the axes are written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_GetAxes(LSM6DSO_Object_t *pObj, LSM6DSO_Axes_t *AngularRate)
{
  lsm6dso_axis3bit16_t data_raw;
  float sensitivity;

  /* Read raw data values. */
  if (lsm6dso_angular_rate_raw_get(&(pObj->Ctx), data_raw.u8bit) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Get LSM6DSO actual sensitivity. */
  if (LSM6DSO_GYRO_GetSensitivity(pObj, &sensitivity) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Calculate the data. */
  AngularRate->x = (int32_t)((float)((float)data_raw.i16bit[0] * sensitivity));
  AngularRate->y = (int32_t)((float)((float)data_raw.i16bit[1] * sensitivity));
  AngularRate->z = (int32_t)((float)((float)data_raw.i16bit[2] * sensitivity));

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO register value
 * @param  pObj the device pObj
 * @param  Reg address to be read
 * @param  Data pointer where the value is written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_Read_Reg(LSM6DSO_Object_t *pObj, uint8_t Reg, uint8_t *Data)
{
  if (lsm6dso_read_reg(&(pObj->Ctx), Reg, Data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO register value
 * @param  pObj the device pObj
 * @param  Reg address to be written
 * @param  Data value to be written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_Write_Reg(LSM6DSO_Object_t *pObj, uint8_t Reg, uint8_t Data)
{
  if (lsm6dso_write_reg(&(pObj->Ctx), Reg, &Data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the interrupt latch
 * @param  pObj the device pObj
 * @param  Status value to be written
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_Set_Interrupt_Latch(LSM6DSO_Object_t *pObj, uint8_t Status)
{
  if (Status > 1U)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_int_notification_set(&(pObj->Ctx), (lsm6dso_lir_t)Status) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable free fall detection
 * @param  pObj the device pObj
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Free_Fall_Detection(LSM6DSO_Object_t *pObj, LSM6DSO_SensorIntPin_t IntPin)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 417.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* FF_DUR setting */
  if (lsm6dso_ff_dur_set(&(pObj->Ctx), 0x06) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* WAKE_DUR setting */
  if (lsm6dso_wkup_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* SLEEP_DUR setting */
  if (lsm6dso_act_sleep_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* FF_THS setting */
  if (lsm6dso_ff_threshold_set(&(pObj->Ctx), LSM6DSO_FF_TSH_312mg) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable free fall event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.md1_cfg.int1_ff = PROPERTY_ENABLE;

      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.md2_cfg.int2_ff = PROPERTY_ENABLE;

      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Disable free fall detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Free_Fall_Detection(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Disable free fall event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.md1_cfg.int1_ff = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.md2_cfg.int2_ff = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* FF_DUR setting */
  if (lsm6dso_ff_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* FF_THS setting */
  if (lsm6dso_ff_threshold_set(&(pObj->Ctx), LSM6DSO_FF_TSH_156mg) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set free fall threshold
 * @param  pObj the device pObj
 * @param  Threshold free fall detection threshold
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Free_Fall_Threshold(LSM6DSO_Object_t *pObj, uint8_t Threshold)
{
  if (lsm6dso_ff_threshold_set(&(pObj->Ctx), (lsm6dso_ff_ths_t)Threshold) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set free fall duration
 * @param  pObj the device pObj
 * @param  Duration free fall detection duration
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Free_Fall_Duration(LSM6DSO_Object_t *pObj, uint8_t Duration)
{
  if (lsm6dso_ff_dur_set(&(pObj->Ctx), Duration) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable pedometer
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Pedometer(LSM6DSO_Object_t *pObj)
{
	lsm6dso_pin_int1_route_t val;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 26.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable pedometer algorithm. */
  if (lsm6dso_pedo_sens_set(&(pObj->Ctx), LSM6DSO_PEDO_BASE_MODE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable step detector on INT1 pin */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val.emb_func_int1.int1_step_detector = PROPERTY_ENABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Disable pedometer
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Pedometer(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t val1;

  /* Disable step detector on INT1 pin */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.emb_func_int1.int1_step_detector = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable pedometer algorithm. */
  if (lsm6dso_pedo_sens_set(&(pObj->Ctx), LSM6DSO_PEDO_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get step count
 * @param  pObj the device pObj
 * @param  StepCount step counter
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_Step_Count(LSM6DSO_Object_t *pObj, uint16_t *StepCount)
{
  if (lsm6dso_number_of_steps_get(&(pObj->Ctx), (uint8_t *)StepCount) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable step counter reset
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Step_Counter_Reset(LSM6DSO_Object_t *pObj)
{
  if (lsm6dso_steps_reset(&(pObj->Ctx)) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable tilt detection
 * @param  pObj the device pObj
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Tilt_Detection(LSM6DSO_Object_t *pObj, LSM6DSO_SensorIntPin_t IntPin)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 26.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable tilt calculation. */
  if (lsm6dso_tilt_sens_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable tilt event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.emb_func_int1.int1_tilt = PROPERTY_ENABLE;

      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.emb_func_int2.int2_tilt = PROPERTY_ENABLE;

      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Disable tilt detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Tilt_Detection(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Disable tilt event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.emb_func_int1.int1_tilt = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.emb_func_int2.int2_tilt = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable tilt calculation. */
  if (lsm6dso_tilt_sens_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable wake up detection
 * @param  pObj the device pObj
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Wake_Up_Detection(LSM6DSO_Object_t *pObj, LSM6DSO_SensorIntPin_t IntPin)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 417.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* WAKE_DUR setting */
  if (lsm6dso_wkup_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set wake up threshold. */
  if (lsm6dso_wkup_threshold_set(&(pObj->Ctx), 0x02) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable wake up event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.md1_cfg.int1_wu = PROPERTY_ENABLE;

      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.md2_cfg.int2_wu = PROPERTY_ENABLE;

      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Disable wake up detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Wake_Up_Detection(LSM6DSO_Object_t *pObj)
{
	lsm6dso_pin_int1_route_t val1;
	lsm6dso_pin_int2_route_t val2;

  /* Disable wake up event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.md1_cfg.int1_wu = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.md2_cfg.int2_wu = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset wake up threshold. */
  if (lsm6dso_wkup_threshold_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* WAKE_DUR setting */
  if (lsm6dso_wkup_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set wake up threshold
 * @param  pObj the device pObj
 * @param  Threshold wake up detection threshold
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Wake_Up_Threshold(LSM6DSO_Object_t *pObj, uint8_t Threshold)
{
  /* Set wake up threshold. */
  if (lsm6dso_wkup_threshold_set(&(pObj->Ctx), Threshold) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set wake up duration
 * @param  pObj the device pObj
 * @param  Duration wake up detection duration
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Wake_Up_Duration(LSM6DSO_Object_t *pObj, uint8_t Duration)
{
  /* Set wake up duration. */
  if (lsm6dso_wkup_dur_set(&(pObj->Ctx), Duration) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable single tap detection
 * @param  pObj the device pObj
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Single_Tap_Detection(LSM6DSO_Object_t *pObj, LSM6DSO_SensorIntPin_t IntPin)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 417.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable X direction in tap recognition. */
  if (lsm6dso_tap_detection_on_x_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable Y direction in tap recognition. */
  if (lsm6dso_tap_detection_on_y_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable Z direction in tap recognition. */
  if (lsm6dso_tap_detection_on_z_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap threshold. */
  if (lsm6dso_tap_threshold_x_set(&(pObj->Ctx), 0x08) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap shock time window. */
  if (lsm6dso_tap_shock_set(&(pObj->Ctx), 0x02) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap quiet time window. */
  if (lsm6dso_tap_quiet_set(&(pObj->Ctx), 0x01) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* _NOTE_: Tap duration time window - don't care for single tap. */

  /* _NOTE_: Single/Double Tap event - don't care of this flag for single tap. */

  /* Enable single tap event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.md1_cfg.int1_single_tap = PROPERTY_ENABLE;

      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.md2_cfg.int2_single_tap = PROPERTY_ENABLE;

      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Disable single tap detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Single_Tap_Detection(LSM6DSO_Object_t *pObj)
{
	lsm6dso_pin_int1_route_t val1;
	lsm6dso_pin_int2_route_t val2;

  /* Disable single tap event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.md1_cfg.int1_single_tap = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.md2_cfg.int2_single_tap = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap quiet time window. */
  if (lsm6dso_tap_quiet_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap shock time window. */
  if (lsm6dso_tap_shock_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap threshold. */
  if (lsm6dso_tap_threshold_x_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable Z direction in tap recognition. */
  if (lsm6dso_tap_detection_on_z_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable Y direction in tap recognition. */
  if (lsm6dso_tap_detection_on_y_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable X direction in tap recognition. */
  if (lsm6dso_tap_detection_on_x_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable double tap detection
 * @param  pObj the device pObj
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Double_Tap_Detection(LSM6DSO_Object_t *pObj, LSM6DSO_SensorIntPin_t IntPin)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 417.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable X direction in tap recognition. */
  if (lsm6dso_tap_detection_on_x_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable Y direction in tap recognition. */
  if (lsm6dso_tap_detection_on_y_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable Z direction in tap recognition. */
  if (lsm6dso_tap_detection_on_z_set(&(pObj->Ctx), PROPERTY_ENABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap threshold. */
  if (lsm6dso_tap_threshold_x_set(&(pObj->Ctx), 0x08) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap shock time window. */
  if (lsm6dso_tap_shock_set(&(pObj->Ctx), 0x03) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap quiet time window. */
  if (lsm6dso_tap_quiet_set(&(pObj->Ctx), 0x03) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Set tap duration time window. */
  if (lsm6dso_tap_dur_set(&(pObj->Ctx), 0x08) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Single and double tap enabled. */
  if (lsm6dso_tap_mode_set(&(pObj->Ctx), LSM6DSO_BOTH_SINGLE_DOUBLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable double tap event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.md1_cfg.int1_double_tap = PROPERTY_ENABLE;

      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.md2_cfg.int2_double_tap = PROPERTY_ENABLE;

      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Disable double tap detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Double_Tap_Detection(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Disable double tap event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.md1_cfg.int1_double_tap = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.md2_cfg.int2_double_tap = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Only single tap enabled. */
  if (lsm6dso_tap_mode_set(&(pObj->Ctx), LSM6DSO_ONLY_SINGLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap duration time window. */
  if (lsm6dso_tap_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap quiet time window. */
  if (lsm6dso_tap_quiet_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap shock time window. */
  if (lsm6dso_tap_shock_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset tap threshold. */
  if (lsm6dso_tap_threshold_x_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable Z direction in tap recognition. */
  if (lsm6dso_tap_detection_on_z_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable Y direction in tap recognition. */
  if (lsm6dso_tap_detection_on_y_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable X direction in tap recognition. */
  if (lsm6dso_tap_detection_on_x_set(&(pObj->Ctx), PROPERTY_DISABLE) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set tap threshold
 * @param  pObj the device pObj
 * @param  Threshold tap threshold
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Tap_Threshold(LSM6DSO_Object_t *pObj, uint8_t Threshold)
{
  /* Set tap threshold. */
  if (lsm6dso_tap_threshold_x_set(&(pObj->Ctx), Threshold) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set tap shock time
 * @param  pObj the device pObj
 * @param  Time tap shock time
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Tap_Shock_Time(LSM6DSO_Object_t *pObj, uint8_t Time)
{
  /* Set tap shock time window. */
  if (lsm6dso_tap_shock_set(&(pObj->Ctx), Time) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set tap quiet time
 * @param  pObj the device pObj
 * @param  Time tap quiet time
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Tap_Quiet_Time(LSM6DSO_Object_t *pObj, uint8_t Time)
{
  /* Set tap quiet time window. */
  if (lsm6dso_tap_quiet_set(&(pObj->Ctx), Time) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set tap duration time
 * @param  pObj the device pObj
 * @param  Time tap duration time
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Tap_Duration_Time(LSM6DSO_Object_t *pObj, uint8_t Time)
{
  /* Set tap duration time window. */
  if (lsm6dso_tap_dur_set(&(pObj->Ctx), Time) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable 6D orientation detection
 * @param  pObj the device pObj
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_6D_Orientation(LSM6DSO_Object_t *pObj, LSM6DSO_SensorIntPin_t IntPin)
{
  int32_t ret = LSM6DSO_OK;
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Output Data Rate selection */
  if (LSM6DSO_ACC_SetOutputDataRate(pObj, 417.0f) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* 6D orientation enabled. */
  if (lsm6dso_6d_threshold_set(&(pObj->Ctx), LSM6DSO_DEG_60) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable 6D orientation event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.md1_cfg.int1_6d = PROPERTY_ENABLE;

      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.md2_cfg.int2_6d = PROPERTY_ENABLE;

      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  return ret;
}

/**
 * @brief  Disable 6D orientation detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_6D_Orientation(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Disable 6D orientation event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.md1_cfg.int1_6d = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.md2_cfg.int2_6d = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Reset 6D orientation. */
  if (lsm6dso_6d_threshold_set(&(pObj->Ctx), LSM6DSO_DEG_80) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set 6D orientation threshold
 * @param  pObj the device pObj
 * @param  Threshold 6D Orientation detection threshold
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_6D_Orientation_Threshold(LSM6DSO_Object_t *pObj, uint8_t Threshold)
{
  if (lsm6dso_6d_threshold_set(&(pObj->Ctx), (lsm6dso_sixd_ths_t)Threshold) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of XLow orientation
 * @param  pObj the device pObj
 * @param  XLow the status of XLow orientation
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_6D_Orientation_XL(LSM6DSO_Object_t *pObj, uint8_t *XLow)
{
  lsm6dso_d6d_src_t data;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *XLow = data.xl;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of XHigh orientation
 * @param  pObj the device pObj
 * @param  XHigh the status of XHigh orientation
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_6D_Orientation_XH(LSM6DSO_Object_t *pObj, uint8_t *XHigh)
{
  lsm6dso_d6d_src_t data;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *XHigh = data.xh;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of YLow orientation
 * @param  pObj the device pObj
 * @param  YLow the status of YLow orientation
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_6D_Orientation_YL(LSM6DSO_Object_t *pObj, uint8_t *YLow)
{
  lsm6dso_d6d_src_t data;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *YLow = data.yl;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of YHigh orientation
 * @param  pObj the device pObj
 * @param  YHigh the status of YHigh orientation
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_6D_Orientation_YH(LSM6DSO_Object_t *pObj, uint8_t *YHigh)
{
  lsm6dso_d6d_src_t data;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *YHigh = data.yh;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of ZLow orientation
 * @param  pObj the device pObj
 * @param  ZLow the status of ZLow orientation
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_6D_Orientation_ZL(LSM6DSO_Object_t *pObj, uint8_t *ZLow)
{
  lsm6dso_d6d_src_t data;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *ZLow = data.zl;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of ZHigh orientation
 * @param  pObj the device pObj
 * @param  ZHigh the status of ZHigh orientation
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_6D_Orientation_ZH(LSM6DSO_Object_t *pObj, uint8_t *ZHigh)
{
  lsm6dso_d6d_src_t data;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&data, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *ZHigh = data.zh;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO ACC data ready bit value
 * @param  pObj the device pObj
 * @param  Status the status of data ready bit
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_DRDY_Status(LSM6DSO_Object_t *pObj, uint8_t *Status)
{
  if (lsm6dso_xl_flag_data_ready_get(&(pObj->Ctx), Status) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the status of all hardware events
 * @param  pObj the device pObj
 * @param  Status the status of all hardware events
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Get_Event_Status(LSM6DSO_Object_t *pObj, LSM6DSO_Event_Status_t *Status)
{
  uint8_t tilt_ia;
  lsm6dso_wake_up_src_t wake_up_src;
  lsm6dso_tap_src_t tap_src;
  lsm6dso_d6d_src_t d6d_src;
  lsm6dso_emb_func_src_t func_src;
  lsm6dso_md1_cfg_t md1_cfg;
  lsm6dso_md2_cfg_t md2_cfg;
  lsm6dso_emb_func_int1_t int1_ctrl;
  lsm6dso_emb_func_int2_t int2_ctrl;

  (void)memset((void *)Status, 0x0, sizeof(LSM6DSO_Event_Status_t));

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_WAKE_UP_SRC, (uint8_t *)&wake_up_src, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_TAP_SRC, (uint8_t *)&tap_src, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_D6D_SRC, (uint8_t *)&d6d_src, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_mem_bank_set(&(pObj->Ctx), LSM6DSO_EMBEDDED_FUNC_BANK) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_EMB_FUNC_SRC, (uint8_t *)&func_src, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_EMB_FUNC_INT1, (uint8_t *)&int1_ctrl, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_EMB_FUNC_INT2, (uint8_t *)&int2_ctrl, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_mem_bank_set(&(pObj->Ctx), LSM6DSO_USER_BANK) != 0)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_MD1_CFG, (uint8_t *)&md1_cfg, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_MD2_CFG, (uint8_t *)&md2_cfg, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_tilt_flag_data_ready_get(&(pObj->Ctx), &tilt_ia) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if ((md1_cfg.int1_ff == 1U) || (md2_cfg.int2_ff == 1U))
  {
    if (wake_up_src.ff_ia == 1U)
    {
      Status->FreeFallStatus = 1;
    }
  }

  if ((md1_cfg.int1_wu == 1U) || (md2_cfg.int2_wu == 1U))
  {
    if (wake_up_src.wu_ia == 1U)
    {
      Status->WakeUpStatus = 1;
    }
  }

  if ((md1_cfg.int1_single_tap == 1U) || (md2_cfg.int2_single_tap == 1U))
  {
    if (tap_src.single_tap == 1U)
    {
      Status->TapStatus = 1;
    }
  }

  if ((md1_cfg.int1_double_tap == 1U) || (md2_cfg.int2_double_tap == 1U))
  {
    if (tap_src.double_tap == 1U)
    {
      Status->DoubleTapStatus = 1;
    }
  }

  if ((md1_cfg.int1_6d == 1U) || (md2_cfg.int2_6d == 1U))
  {
    if (d6d_src.d6d_ia == 1U)
    {
      Status->D6DOrientationStatus = 1;
    }
  }

  if (int1_ctrl.int1_step_detector == 1U)
  {
    if (func_src.step_detected == 1U)
    {
      Status->StepStatus = 1;
    }
  }

  if ((int1_ctrl.int1_tilt == 1U) || (int2_ctrl.int2_tilt == 1U))
  {
    if (tilt_ia == 1U)
    {
      Status->TiltStatus = 1;
    }
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set self test
 * @param  pObj the device pObj
 * @param  val the value of st_xl in reg CTRL5_C
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_SelfTest(LSM6DSO_Object_t *pObj, uint8_t val)
{
  lsm6dso_st_xl_t reg;

  reg = (val == 0U)  ? LSM6DSO_XL_ST_DISABLE
        : (val == 1U)  ? LSM6DSO_XL_ST_POSITIVE
        : (val == 2U)  ? LSM6DSO_XL_ST_NEGATIVE
        :                LSM6DSO_XL_ST_DISABLE;

  if (lsm6dso_xl_self_test_set(&(pObj->Ctx), reg) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO GYRO data ready bit value
 * @param  pObj the device pObj
 * @param  Status the status of data ready bit
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Get_DRDY_Status(LSM6DSO_Object_t *pObj, uint8_t *Status)
{
  if (lsm6dso_gy_flag_data_ready_get(&(pObj->Ctx), Status) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set self test
 * @param  pObj the device pObj
 * @param  val the value of st_xl in reg CTRL5_C
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Set_SelfTest(LSM6DSO_Object_t *pObj, uint8_t val)
{
  lsm6dso_st_g_t reg;

  reg = (val == 0U)  ? LSM6DSO_GY_ST_DISABLE
        : (val == 1U)  ? LSM6DSO_GY_ST_POSITIVE
        : (val == 2U)  ? LSM6DSO_GY_ST_NEGATIVE
        :                LSM6DSO_GY_ST_DISABLE;


  if (lsm6dso_gy_self_test_set(&(pObj->Ctx), reg) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO FIFO number of samples
 * @param  pObj the device pObj
 * @param  NumSamples number of samples
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Get_Num_Samples(LSM6DSO_Object_t *pObj, uint16_t *NumSamples)
{
  if (lsm6dso_fifo_data_level_get(&(pObj->Ctx), NumSamples) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO FIFO full status
 * @param  pObj the device pObj
 * @param  Status FIFO full status
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Get_Full_Status(LSM6DSO_Object_t *pObj, uint8_t *Status)
{
  lsm6dso_reg_t reg;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_FIFO_STATUS2, &reg.byte, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  *Status = reg.fifo_status2.fifo_full_ia;

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO FIFO full interrupt on INT1 pin
 * @param  pObj the device pObj
 * @param  Status FIFO full interrupt on INT1 pin status
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Set_INT1_FIFO_Full(LSM6DSO_Object_t *pObj, uint8_t Status)
{
  lsm6dso_reg_t reg;

  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_INT1_CTRL, &reg.byte, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  reg.int1_ctrl.int1_fifo_full = Status;

  if (lsm6dso_write_reg(&(pObj->Ctx), LSM6DSO_INT1_CTRL, &reg.byte, 1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO FIFO watermark level
 * @param  pObj the device pObj
 * @param  Watermark FIFO watermark level
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Set_Watermark_Level(LSM6DSO_Object_t *pObj, uint16_t Watermark)
{
  if (lsm6dso_fifo_watermark_set(&(pObj->Ctx), Watermark) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO FIFO stop on watermark
 * @param  pObj the device pObj
 * @param  Status FIFO stop on watermark status
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Set_Stop_On_Fth(LSM6DSO_Object_t *pObj, uint8_t Status)
{
  if (lsm6dso_fifo_stop_on_wtm_set(&(pObj->Ctx), Status) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO FIFO mode
 * @param  pObj the device pObj
 * @param  Mode FIFO mode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Set_Mode(LSM6DSO_Object_t *pObj, uint8_t Mode)
{
  int32_t ret = LSM6DSO_OK;

  /* Verify that the passed parameter contains one of the valid values. */
  switch ((lsm6dso_fifo_mode_t)Mode)
  {
    case LSM6DSO_BYPASS_MODE:
    case LSM6DSO_FIFO_MODE:
    case LSM6DSO_STREAM_TO_FIFO_MODE:
    case LSM6DSO_BYPASS_TO_STREAM_MODE:
    case LSM6DSO_STREAM_MODE:
      break;

    default:
      ret = LSM6DSO_ERROR;
      break;
  }

  if (ret == LSM6DSO_ERROR)
  {
    return ret;
  }

  if (lsm6dso_fifo_mode_set(&(pObj->Ctx), (lsm6dso_fifo_mode_t)Mode) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return ret;
}

/**
 * @brief  Get the LSM6DSO FIFO tag
 * @param  pObj the device pObj
 * @param  Tag FIFO tag
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Get_Tag(LSM6DSO_Object_t *pObj, uint8_t *Tag)
{
  lsm6dso_fifo_tag_t tag_local;

	if (lsm6dso_fifo_sensor_tag_get(&(pObj->Ctx), &tag_local) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

	*Tag = (uint8_t)tag_local;

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO FIFO raw data
 * @param  pObj the device pObj
 * @param  Data FIFO raw data array [6]
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_Get_Data(LSM6DSO_Object_t *pObj, uint8_t *Data)
{
  if (lsm6dso_read_reg(&(pObj->Ctx), LSM6DSO_FIFO_DATA_OUT_X_L, Data, 6) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO FIFO accelero single sample (16-bit data per 3 axes) and calculate acceleration [mg]
 * @param  pObj the device pObj
 * @param  Acceleration FIFO accelero axes [mg]
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_ACC_Get_Axes(LSM6DSO_Object_t *pObj, LSM6DSO_Axes_t *Acceleration)
{
  uint8_t data[6];
  int16_t data_raw[3];
  float sensitivity = 0.0f;
  float acceleration_float[3];

  if (LSM6DSO_FIFO_Get_Data(pObj, data) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  data_raw[0] = ((int16_t)data[1] << 8) | data[0];
  data_raw[1] = ((int16_t)data[3] << 8) | data[2];
  data_raw[2] = ((int16_t)data[5] << 8) | data[4];

  if (LSM6DSO_ACC_GetSensitivity(pObj, &sensitivity) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  acceleration_float[0] = (float)data_raw[0] * sensitivity;
  acceleration_float[1] = (float)data_raw[1] * sensitivity;
  acceleration_float[2] = (float)data_raw[2] * sensitivity;

  Acceleration->x = (int32_t)acceleration_float[0];
  Acceleration->y = (int32_t)acceleration_float[1];
  Acceleration->z = (int32_t)acceleration_float[2];

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO FIFO accelero BDR value
 * @param  pObj the device pObj
 * @param  Bdr FIFO accelero BDR value
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_ACC_Set_BDR(LSM6DSO_Object_t *pObj, float Bdr)
{
  lsm6dso_bdr_xl_t new_bdr;

  new_bdr = (Bdr <=    0.0f) ? LSM6DSO_XL_NOT_BATCHED
          : (Bdr <=   12.5f) ? LSM6DSO_XL_BATCHED_AT_12Hz5
          : (Bdr <=   26.0f) ? LSM6DSO_XL_BATCHED_AT_26Hz
          : (Bdr <=   52.0f) ? LSM6DSO_XL_BATCHED_AT_52Hz
          : (Bdr <=  104.0f) ? LSM6DSO_XL_BATCHED_AT_104Hz
          : (Bdr <=  208.0f) ? LSM6DSO_XL_BATCHED_AT_208Hz
          : (Bdr <=  417.0f) ? LSM6DSO_XL_BATCHED_AT_417Hz
          : (Bdr <=  833.0f) ? LSM6DSO_XL_BATCHED_AT_833Hz
          : (Bdr <= 1667.0f) ? LSM6DSO_XL_BATCHED_AT_1667Hz
          : (Bdr <= 3333.0f) ? LSM6DSO_XL_BATCHED_AT_3333Hz
          :                    LSM6DSO_XL_BATCHED_AT_6667Hz;

  if (lsm6dso_fifo_xl_batch_set(&(pObj->Ctx), new_bdr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Get the LSM6DSO FIFO gyro single sample (16-bit data per 3 axes) and calculate angular velocity [mDPS]
 * @param  pObj the device pObj
 * @param  AngularVelocity FIFO gyro axes [mDPS]
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_GYRO_Get_Axes(LSM6DSO_Object_t *pObj, LSM6DSO_Axes_t *AngularVelocity)
{
  uint8_t data[6];
  int16_t data_raw[3];
  float sensitivity = 0.0f;
  float angular_velocity_float[3];

  if (LSM6DSO_FIFO_Get_Data(pObj, data) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  data_raw[0] = ((int16_t)data[1] << 8) | data[0];
  data_raw[1] = ((int16_t)data[3] << 8) | data[2];
  data_raw[2] = ((int16_t)data[5] << 8) | data[4];

  if (LSM6DSO_GYRO_GetSensitivity(pObj, &sensitivity) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  angular_velocity_float[0] = (float)data_raw[0] * sensitivity;
  angular_velocity_float[1] = (float)data_raw[1] * sensitivity;
  angular_velocity_float[2] = (float)data_raw[2] * sensitivity;

  AngularVelocity->x = (int32_t)angular_velocity_float[0];
  AngularVelocity->y = (int32_t)angular_velocity_float[1];
  AngularVelocity->z = (int32_t)angular_velocity_float[2];

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO FIFO gyro BDR value
 * @param  pObj the device pObj
 * @param  Bdr FIFO gyro BDR value
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_FIFO_GYRO_Set_BDR(LSM6DSO_Object_t *pObj, float Bdr)
{
  lsm6dso_bdr_gy_t new_bdr;

  new_bdr = (Bdr <=    0.0f) ? LSM6DSO_GY_NOT_BATCHED
          : (Bdr <=   12.5f) ? LSM6DSO_GY_BATCHED_AT_12Hz5
          : (Bdr <=   26.0f) ? LSM6DSO_GY_BATCHED_AT_26Hz
          : (Bdr <=   52.0f) ? LSM6DSO_GY_BATCHED_AT_52Hz
          : (Bdr <=  104.0f) ? LSM6DSO_GY_BATCHED_AT_104Hz
          : (Bdr <=  208.0f) ? LSM6DSO_GY_BATCHED_AT_208Hz
          : (Bdr <=  417.0f) ? LSM6DSO_GY_BATCHED_AT_417Hz
          : (Bdr <=  833.0f) ? LSM6DSO_GY_BATCHED_AT_833Hz
          : (Bdr <= 1667.0f) ? LSM6DSO_GY_BATCHED_AT_1667Hz
          : (Bdr <= 3333.0f) ? LSM6DSO_GY_BATCHED_AT_3333Hz
          :                    LSM6DSO_GY_BATCHED_AT_6667Hz;

  if (lsm6dso_fifo_gy_batch_set(&(pObj->Ctx), new_bdr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable LSM6DSO accelerometer DRDY interrupt on INT1
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_DRDY_On_INT1(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t pin_int1_route;

  /* Enable accelerometer DRDY Interrupt on INT1 */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &pin_int1_route) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }
  pin_int1_route.int1_ctrl.int1_drdy_xl = 1;
  pin_int1_route.int1_ctrl.int1_drdy_g = 0;
  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &pin_int1_route) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Disable LSM6DSO accelerometer DRDY interrupt on INT1
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_DRDY_On_INT1(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t pin_int1_route;

  /* Disable accelerometer DRDY Interrupt on INT1 */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &pin_int1_route) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }
  pin_int1_route.int1_ctrl.int1_drdy_xl = 0;
  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &pin_int1_route) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO accelerometer power mode
 * @param  pObj the device pObj
 * @param  PowerMode Value of the powerMode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Power_Mode(LSM6DSO_Object_t *pObj, uint8_t PowerMode)
{
  if (lsm6dso_xl_power_mode_set(&(pObj->Ctx), (lsm6dso_xl_hm_mode_t)PowerMode) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO accelerometer filter mode
 * @param  pObj the device pObj
 * @param  LowHighPassFlag 0/1 for setting low-pass/high-pass filter mode
 * @param  FilterMode Value of the filter Mode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Filter_Mode(LSM6DSO_Object_t *pObj, uint8_t LowHighPassFlag, uint8_t FilterMode)
{
  if (LowHighPassFlag == 0)
  {
    /*Set accelerometer low_pass filter-mode*/

    /*Set to 1 LPF2 bit (CTRL8_XL)*/
    if (lsm6dso_xl_filter_lp2_set(&(pObj->Ctx), 1) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
    if (lsm6dso_xl_hp_path_on_out_set(&(pObj->Ctx), (lsm6dso_hp_slope_xl_en_t)FilterMode) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
  }
  else
  {
    /*Set accelerometer high_pass filter-mode*/
    if (lsm6dso_xl_hp_path_on_out_set(&(pObj->Ctx), (lsm6dso_hp_slope_xl_en_t)FilterMode) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
  }
  return LSM6DSO_OK;
}

/**
 * @brief  Enable LSM6DSO accelerometer inactivity detection
 * @param  pObj the device pObj
 * @param  InactMode inactivity detection mode
 * @param  IntPin interrupt pin line to be used
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Enable_Inactivity_Detection(LSM6DSO_Object_t *pObj, lsm6dso_inact_en_t InactMode, LSM6DSO_SensorIntPin_t IntPin)
{
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Full scale selection */
  if (LSM6DSO_ACC_SetFullScale(pObj, 2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }
  if (LSM6DSO_GYRO_SetFullScale(pObj, 250) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* SLEEP_DUR setting */
  if (lsm6dso_act_sleep_dur_set(&(pObj->Ctx), 0x01) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Enable inactivity detection. */
  switch(InactMode)
  {
  case LSM6DSO_XL_AND_GY_NOT_AFFECTED:
    if (lsm6dso_act_mode_set(&(pObj->Ctx), LSM6DSO_XL_AND_GY_NOT_AFFECTED) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
    break;
  case LSM6DSO_XL_12Hz5_GY_NOT_AFFECTED:
    if (lsm6dso_act_mode_set(&(pObj->Ctx), LSM6DSO_XL_12Hz5_GY_NOT_AFFECTED) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
    break;
  case LSM6DSO_XL_12Hz5_GY_SLEEP:
    if (lsm6dso_act_mode_set(&(pObj->Ctx), LSM6DSO_XL_12Hz5_GY_SLEEP) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
    break;
  case LSM6DSO_XL_12Hz5_GY_PD:
    if (lsm6dso_act_mode_set(&(pObj->Ctx), LSM6DSO_XL_12Hz5_GY_PD) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
    break;
  }

  /* Enable Inactivity event on either INT1 or INT2 pin */
  switch (IntPin)
  {
    case LSM6DSO_INT1_PIN:
      if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val1.md1_cfg.int1_sleep_change = PROPERTY_ENABLE;
      if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    case LSM6DSO_INT2_PIN:
      if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }

      val2.md2_cfg.int2_sleep_change = PROPERTY_ENABLE;
      if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
      {
        return LSM6DSO_ERROR;
      }
      break;

    default:
      return LSM6DSO_ERROR;
  }
  return LSM6DSO_OK;;
}

/**
 * @brief  Disable LSM6DSO accelerometer inactivity detection
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Disable_Inactivity_Detection(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int1_route_t val1;
  lsm6dso_pin_int2_route_t val2;

  /* Disable inactivity event on both INT1 and INT2 pins */
  if (lsm6dso_pin_int1_route_get(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val1.md1_cfg.int1_sleep_change = PROPERTY_DISABLE;

  if (lsm6dso_pin_int1_route_set(&(pObj->Ctx), &val1) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  val2.md2_cfg.int2_sleep_change = PROPERTY_DISABLE;

  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &val2) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* Disable inactivity detection. */
  if (lsm6dso_act_mode_set(&(pObj->Ctx), LSM6DSO_XL_AND_GY_NOT_AFFECTED) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  /* SLEEP_DUR reset */
  if (lsm6dso_act_sleep_dur_set(&(pObj->Ctx), 0x00) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }
  return LSM6DSO_OK;
}

/**
 * @brief  Set LSM6DSO accelerometer sleep duration
 * @param  pObj the device pObj
 * @param  Duration wake up detection duration
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_ACC_Set_Sleep_Duration(LSM6DSO_Object_t *pObj, uint8_t Duration)
{
  /* Set wake up duration. */
  if (lsm6dso_act_sleep_dur_set(&(pObj->Ctx), Duration) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Enable LSM6DSO gyroscope DRDY interrupt on INT2
 * @param  pObj the device pObj
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Enable_DRDY_On_INT2(LSM6DSO_Object_t *pObj)
{
  lsm6dso_pin_int2_route_t pin_int2_route;

  /* Enable gyroscope DRDY Interrupts on INT2 */
  if (lsm6dso_pin_int2_route_get(&(pObj->Ctx), &pin_int2_route) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }
  pin_int2_route.int2_ctrl.int2_drdy_xl = 0;
  pin_int2_route.int2_ctrl.int2_drdy_g = 1;
  if (lsm6dso_pin_int2_route_set(&(pObj->Ctx), &pin_int2_route) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO gyroscope power mode
 * @param  pObj the device pObj
 * @param  PowerMode Value of the powerMode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Set_Power_Mode(LSM6DSO_Object_t *pObj, uint8_t PowerMode)
{
  if (lsm6dso_gy_power_mode_set(&(pObj->Ctx), (lsm6dso_g_hm_mode_t)PowerMode) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO gyroscope filter mode
 * @param  pObj the device pObj
 * @param  LowHighPassFlag 0/1 for setting low-pass/high-pass filter mode
 * @param  FilterMode Value of the filter Mode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_GYRO_Set_Filter_Mode(LSM6DSO_Object_t *pObj, uint8_t LowHighPassFlag, uint8_t FilterMode)
{
  if (LowHighPassFlag == 0)
  {
    /*Set gyroscope low_pass 1 filter-mode*/
    /* Enable low-pass filter */
    if (lsm6dso_gy_filter_lp1_set(&(pObj->Ctx), 1) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
    if (lsm6dso_gy_lp1_bandwidth_set(&(pObj->Ctx), (lsm6dso_ftype_t)FilterMode) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
  }
  else
  {
    /*Set gyroscope high_pass filter-mode*/
    /* Enable high-pass filter */
    if (lsm6dso_gy_hp_path_internal_set(&(pObj->Ctx), (lsm6dso_hpm_g_t)FilterMode) != LSM6DSO_OK)
    {
      return LSM6DSO_ERROR;
    }
  }
  return LSM6DSO_OK;
}

/**
 * @brief  Set LSM6DSO DRDY mode
 * @param  pObj the device pObj
 * @param  Mode DRDY mode
 * @retval 0 in case of success, an error code otherwise
 */
int32_t LSM6DSO_DRDY_Set_Mode(LSM6DSO_Object_t *pObj, uint8_t Mode)
{
  /* Set DRDY mode */
  if (lsm6dso_data_ready_mode_set(&(pObj->Ctx), (lsm6dso_dataready_pulsed_t)Mode) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @}
 */

/** @defgroup LSM6DSO_Private_Functions LSM6DSO Private Functions
 * @{
 */

/**
 * @brief  Set the LSM6DSO accelerometer sensor output data rate when enabled
 * @param  pObj the device pObj
 * @param  Odr the functional output data rate to be set
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t LSM6DSO_ACC_SetOutputDataRate_When_Enabled(LSM6DSO_Object_t *pObj, float Odr)
{
  lsm6dso_odr_xl_t new_odr;

  new_odr = (Odr <=   12.5f) ? LSM6DSO_XL_ODR_12Hz5
          : (Odr <=   26.0f) ? LSM6DSO_XL_ODR_26Hz
          : (Odr <=   52.0f) ? LSM6DSO_XL_ODR_52Hz
          : (Odr <=  104.0f) ? LSM6DSO_XL_ODR_104Hz
          : (Odr <=  208.0f) ? LSM6DSO_XL_ODR_208Hz
          : (Odr <=  417.0f) ? LSM6DSO_XL_ODR_417Hz
          : (Odr <=  833.0f) ? LSM6DSO_XL_ODR_833Hz
          : (Odr <= 1667.0f) ? LSM6DSO_XL_ODR_1667Hz
          : (Odr <= 3333.0f) ? LSM6DSO_XL_ODR_3333Hz
          :                    LSM6DSO_XL_ODR_6667Hz;

  /* Output data rate selection. */
  if (lsm6dso_xl_data_rate_set(&(pObj->Ctx), new_odr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO accelerometer sensor output data rate when disabled
 * @param  pObj the device pObj
 * @param  Odr the functional output data rate to be set
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t LSM6DSO_ACC_SetOutputDataRate_When_Disabled(LSM6DSO_Object_t *pObj, float Odr)
{
  pObj->acc_odr = (Odr <=   12.5f) ? LSM6DSO_XL_ODR_12Hz5
                : (Odr <=   26.0f) ? LSM6DSO_XL_ODR_26Hz
                : (Odr <=   52.0f) ? LSM6DSO_XL_ODR_52Hz
                : (Odr <=  104.0f) ? LSM6DSO_XL_ODR_104Hz
                : (Odr <=  208.0f) ? LSM6DSO_XL_ODR_208Hz
                : (Odr <=  417.0f) ? LSM6DSO_XL_ODR_417Hz
                : (Odr <=  833.0f) ? LSM6DSO_XL_ODR_833Hz
                : (Odr <= 1667.0f) ? LSM6DSO_XL_ODR_1667Hz
                : (Odr <= 3333.0f) ? LSM6DSO_XL_ODR_3333Hz
                :                    LSM6DSO_XL_ODR_6667Hz;

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO gyroscope sensor output data rate when enabled
 * @param  pObj the device pObj
 * @param  Odr the functional output data rate to be set
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t LSM6DSO_GYRO_SetOutputDataRate_When_Enabled(LSM6DSO_Object_t *pObj, float Odr)
{
  lsm6dso_odr_g_t new_odr;

  new_odr = (Odr <=   12.5f) ? LSM6DSO_GY_ODR_12Hz5
          : (Odr <=   26.0f) ? LSM6DSO_GY_ODR_26Hz
          : (Odr <=   52.0f) ? LSM6DSO_GY_ODR_52Hz
          : (Odr <=  104.0f) ? LSM6DSO_GY_ODR_104Hz
          : (Odr <=  208.0f) ? LSM6DSO_GY_ODR_208Hz
          : (Odr <=  417.0f) ? LSM6DSO_GY_ODR_417Hz
          : (Odr <=  833.0f) ? LSM6DSO_GY_ODR_833Hz
          : (Odr <= 1667.0f) ? LSM6DSO_GY_ODR_1667Hz
          : (Odr <= 3333.0f) ? LSM6DSO_GY_ODR_3333Hz
          :                    LSM6DSO_GY_ODR_6667Hz;

  /* Output data rate selection. */
  if (lsm6dso_gy_data_rate_set(&(pObj->Ctx), new_odr) != LSM6DSO_OK)
  {
    return LSM6DSO_ERROR;
  }

  return LSM6DSO_OK;
}

/**
 * @brief  Set the LSM6DSO gyroscope sensor output data rate when disabled
 * @param  pObj the device pObj
 * @param  Odr the functional output data rate to be set
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t LSM6DSO_GYRO_SetOutputDataRate_When_Disabled(LSM6DSO_Object_t *pObj, float Odr)
{
  pObj->gyro_odr = (Odr <=   12.5f) ? LSM6DSO_GY_ODR_12Hz5
                 : (Odr <=   26.0f) ? LSM6DSO_GY_ODR_26Hz
                 : (Odr <=   52.0f) ? LSM6DSO_GY_ODR_52Hz
                 : (Odr <=  104.0f) ? LSM6DSO_GY_ODR_104Hz
                 : (Odr <=  208.0f) ? LSM6DSO_GY_ODR_208Hz
                 : (Odr <=  417.0f) ? LSM6DSO_GY_ODR_417Hz
                 : (Odr <=  833.0f) ? LSM6DSO_GY_ODR_833Hz
                 : (Odr <= 1667.0f) ? LSM6DSO_GY_ODR_1667Hz
                 : (Odr <= 3333.0f) ? LSM6DSO_GY_ODR_3333Hz
                 :                    LSM6DSO_GY_ODR_6667Hz;

  return LSM6DSO_OK;
}

/**
 * @brief  Wrap Read register component function to Bus IO function
 * @param  Handle the device handler
 * @param  Reg the register address
 * @param  pData the stored data pointer
 * @param  Length the length
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t ReadRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length)
{
  LSM6DSO_Object_t *pObj = (LSM6DSO_Object_t *)Handle;

  return pObj->IO.ReadReg(pObj->IO.Address, Reg, pData, Length);
}

/**
 * @brief  Wrap Write register component function to Bus IO function
 * @param  Handle the device handler
 * @param  Reg the register address
 * @param  pData the stored data pointer
 * @param  Length the length
 * @retval 0 in case of success, an error code otherwise
 */
static int32_t WriteRegWrap(void *Handle, uint8_t Reg, uint8_t *pData, uint16_t Length)
{
  LSM6DSO_Object_t *pObj = (LSM6DSO_Object_t *)Handle;

  return pObj->IO.WriteReg(pObj->IO.Address, Reg, pData, Length);
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
