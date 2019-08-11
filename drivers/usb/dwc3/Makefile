ccflags-$(CONFIG_USB_DWC3_DEBUG)	:= -DDEBUG
ccflags-$(CONFIG_USB_DWC3_VERBOSE)	+= -DVERBOSE_DEBUG

ifneq ($(CONFIG_USB_VXR10_MODE),)
	obj-$(CONFIG_USB_DWC3)		+= dwc3.o
	dwc3-y				:= core.o

	ifneq ($(filter y,$(CONFIG_USB_DWC3_HOST) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
		dwc3-y				+= host.o
	endif

	ifneq ($(filter y,$(CONFIG_USB_DWC3_GADGET) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
		dwc3-y				+= gadget.o ep0.o
	endif

	ifneq ($(CONFIG_DEBUG_FS),)
		dwc3-y				+= debugfs.o
	endif

	##
	# Platform-specific glue layers go here
	#
	# NOTICE: Make sure your glue layer doesn't depend on anything
	# which is arch-specific and that it compiles on all situations.
	#
	# We want to keep this requirement in order to be able to compile
	# the entire driver (with all its glue layers) on several architectures
	# and make sure it compiles fine. This will also help with allmodconfig
	# and allyesconfig builds.
	#
	# The only exception is the PCI glue layer, but that's only because
	# PCI doesn't provide nops if CONFIG_PCI isn't enabled.
	##

	obj-$(CONFIG_USB_DWC3)		+= dwc3-omap.o
	obj-$(CONFIG_USB_DWC3)		+= dwc3-exynos.o

	ifneq ($(CONFIG_PCI),)
		obj-$(CONFIG_USB_DWC3)		+= dwc3-pci.o
	endif
else
	ifeq ($(CONFIG_USB_DWC3_VXR10_USBH),m)
		obj-m += vxr10-usbh.o
		vxr10-usbh-y := dwc3-vxr10-usbh.o dwc3-vxr10-common.o host.o
	endif
	ifeq ($(CONFIG_USB_DWC3_VXR10_USBD),m)
		obj-m += vxr10-usbd.o
		vxr10-usbd-y  := dwc3-vxr10-usbd.o dwc3-vxr10-common.o gadget.o ep0.o
	endif
	ifeq ($(CONFIG_USB_DWC3_VXR10_MODE),y)
		obj-y += dwc3-vxr10-common.o
	endif
	ifeq ($(CONFIG_USB_DWC3_VXR10_USBH),y)
		obj-y += dwc3-vxr10-usbh.o host.o
	endif
	ifeq ($(CONFIG_USB_DWC3_VXR10_USBD),y)
		obj-y += dwc3-vxr10-usbd.o gadget.o ep0.o
	endif
endif
