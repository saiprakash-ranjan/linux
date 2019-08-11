#
# Makefile for CXD5602	HostIFC
#

obj-$(CONFIG_HIFC_DRIVER) += hifc.o

HIFC_SRC_PATH := .

hifc-objs := $(HIFC_SRC_PATH)/hifc_prot_stack.o
hifc-objs += $(HIFC_SRC_PATH)/hifc_prot_task_mgr.o
hifc-objs += $(HIFC_SRC_PATH)/hifc_msg_disp.o
hifc-objs += $(HIFC_SRC_PATH)/hi_system_handler.o

ifdef CONFIG_HIFC_DRVIF_SPI
obj-$(CONFIG_HIFC_DRIVER) += hifc_drvif_spi.o
#hifc_drvif_spi-objs += $(HIFC_SRC_PATH)/hifc_drvif_spi.o
else
obj-$(CONFIG_HIFC_DRIVER) += hifc_drvif_i2c.o
#hifc_drvif_i2c-objs += $(HIFC_SRC_PATH)/hifc_drvif_i2c.o
endif
