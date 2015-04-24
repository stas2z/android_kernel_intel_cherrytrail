#
# Makefile for the td lock driver.
#

ccflags-y := -Werror -Idrivers/misc/mei

ifeq ($(CONFIG_TCG_TPM), m)
		ccflags-y += -DTCG_TPM_MODULE
endif

MODULE_NAME := tdlock
$(MODULE_NAME)-objs := main.o tpmcmd.o util.o sclock.o mei.o mei_driver.o tdos.o

obj-$(CONFIG_TDLOCK) := tdlock.o
