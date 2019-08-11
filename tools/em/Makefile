#
# Makefile for SNSC EM
#

obj-$(CONFIG_SNSC_EM)	+= exception_monitor.o

exception_monitor-y	:= exception.o

exception_monitor-$(CONFIG_SNSC_EM_NOTIFY)	+= emnotify.o
exception_monitor-$(CONFIG_SNSC_EM_DEMANGLE)	+= cp-demangle.o

ifdef CONFIG_ARM
include tools/em/arm/Makefile
endif
ifdef CONFIG_X86
include tools/em/x86/Makefile
endif
