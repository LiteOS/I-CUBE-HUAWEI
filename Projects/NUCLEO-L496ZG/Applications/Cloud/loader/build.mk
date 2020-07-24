C_SOURCES  += $(PROJ_DIR)/Src/stm32l4xx_hal_msp.c \
              $(PROJ_DIR)/Src/system_stm32l4xx.c \
              $(PROJ_DIR)/Src/stm32l4xx_nucleo_bus.c \
              $(PROJ_DIR)/Src/ota_flash/flash_ota_img.c \
              $(PROJ_DIR)/Src/ota_flash/hal_flash.c \
              $(PROJ_DIR)/loader.c \
              $(PROJ_DIR)/board.c

C_INCLUDES += -I$(PROJ_DIR)/  \
              -I$(PROJ_DIR)/Inc/ \
              -I$(PROJ_DIR)/Src/ota_flash
C_DEFS     +=
