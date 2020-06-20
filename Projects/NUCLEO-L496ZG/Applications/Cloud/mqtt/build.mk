C_SOURCES  += $(PROJ_DIR)/Src/stm32l4xx_hal_msp.c \
              $(PROJ_DIR)/Src/system_stm32l4xx.c \
              $(PROJ_DIR)/Src/stm32l4xx_nucleo_bus.c \
              $(PROJ_DIR)/Src/app_x-cube-mems1.c \
              $(PROJ_DIR)/board.c \
              $(PROJ_DIR)/oc_demo.c \
              $(DRIVERLIB_DIR)/BSP/Components/hts221/hts221.c         \
              $(DRIVERLIB_DIR)/BSP/Components/hts221/hts221_reg.c     \
              $(DRIVERLIB_DIR)/BSP/Components/lis2dw12/lis2dw12.c     \
              $(DRIVERLIB_DIR)/BSP/Components/lis2dw12/lis2dw12_reg.c \
              $(DRIVERLIB_DIR)/BSP/Components/lis2mdl/lis2mdl.c       \
              $(DRIVERLIB_DIR)/BSP/Components/lis2mdl/lis2mdl_reg.c   \
              $(DRIVERLIB_DIR)/BSP/Components/lps22hh/lps22hh.c       \
              $(DRIVERLIB_DIR)/BSP/Components/lps22hh/lps22hh_reg.c   \
              $(DRIVERLIB_DIR)/BSP/Components/lsm6dso/lsm6dso.c       \
              $(DRIVERLIB_DIR)/BSP/Components/lsm6dso/lsm6dso_reg.c   \
              $(DRIVERLIB_DIR)/BSP/Components/stts751/stts751.c       \
              $(DRIVERLIB_DIR)/BSP/Components/stts751/stts751_reg.c   \
              ${wildcard $(DRIVERLIB_DIR)/BSP/IKS01A3/*.c}

C_INCLUDES += -I$(PROJ_DIR)/  \
              -I$(PROJ_DIR)/Inc/ \
              -I$(DRIVERLIB_DIR)/BSP/IKS01A3 \
              -I$(DRIVERLIB_DIR)/BSP/Components/Common \
              -I$(DRIVERLIB_DIR)/BSP/Components/hts221 \
              -I$(DRIVERLIB_DIR)/BSP/Components/lis2dw12 \
              -I$(DRIVERLIB_DIR)/BSP/Components/lis2mdl \
              -I$(DRIVERLIB_DIR)/BSP/Components/lps22hh \
              -I$(DRIVERLIB_DIR)/BSP/Components/lsm6dso \
              -I$(DRIVERLIB_DIR)/BSP/Components/stts751
C_DEFS     +=
