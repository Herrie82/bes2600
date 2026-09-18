KERN_DIR = /lib/modules/$(shell uname -r)/build
# feature option
BES2600                                 ?= m

CONFIG_BES2600_TESTMODE                 ?= n

CONFIG_BES2600_ENABLE_DEVEL_LOGS        ?= n

CONFIG_BES2600_5GHZ_SUPPORT             ?= y

CONFIG_BES2600_WAPI_SUPPORT             ?= n
CONFIG_BES2600_STA_DEBUG                ?= y
CONFIG_BES2600_STATIC_SDD               ?= y
P2P_MULTIVIF                            ?= y
AP_AGGREGATE_FW_FIX                     ?= y
CONFIG_BES2600_BT                       ?= n
WIFI_BT_COEXIST_EPTA_ENABLE             ?= y
WIFI_BT_COEXIST_EPTA_FDD                ?= n
CONFIG_BES2600_WOWLAN                   ?= y
CONFIG_BES2600_LISTEN_INTERVAL          ?= 3
BSS_LOSS_CHECK                          ?= y

CONFIG_BES2600_DEBUGFS                  ?= y
CONFIG_BES2600_ITP                      ?= n

CONFIG_BES2600_NON_POWER_OF_TWO_BLOCKSIZES ?= n
CONFIG_BES2600_CALIB_FROM_LINUX         ?= y

CONFIG_BES2600_WIFI_BOOT_ON             ?= y
CONFIG_BES2600_BT_BOOT_ON               ?= n

BES2600_GPIO_WAKEUP_AP                  ?= n
BES2600_WRITE_DPD_TO_FILE               ?= n
BES2600_TX_MORE_RETRY                   ?= n

# bes evb
BES2600_INDEPENDENT_EVB                 ?= n
# wifi module for allwinner made by bes
BES2600_INTEGRATED_MODULE_V1            ?= n
# xiaomi R329 wifi module
BES2600_INTEGRATED_MODULE_V2            ?= n
# sicun QM215 wifi module
BES2600_INTEGRATED_MODULE_V3            ?= n

# 0: use dynamic_ps_timeout value
# other: override dynamic_ps_timeout value
BES2600_FASTPS_IDLE_TIME		?= 8

#1.rock api(fixed macaddr)
#2.read from file(macaddr of customers)
#3.read from file(macaddr template)
#4.random macaddr
GET_MAC_ADDR_METHOD                     ?= 4

ifeq ($(CONFIG_BES2600_DEBUGFS),y)
BES2600_DUMP_FW_DPD_LOG                 ?= n
endif

ifeq ($(CONFIG_BES2600_TESTMODE),y)
CONFIG_BES2600_KEEP_ALIVE               ?= n
ifeq ($(CONFIG_BES2600_KEEP_ALIVE),y)
ccflags-y += -DVENDOR_XM_KEEPALIVE
endif
endif

BES2600_DRV_VERSION := bes2600_0.3.5_2024.0116

ifeq ($(CONFIG_BES2600_CALIB_FROM_LINUX),y)
FACTORY_CRC_CHECK                    ?= n
STANDARD_FACTORY_EFUSE_FLAG          ?= y
# Where the calibration blob lives is a packaging decision.  The vendor default
# was a flat /lib/firmware/bes2600_factory.txt, but the PineTab2 firmware
# repository - and so every distribution packaging it - installs the file next
# to the rest of the BES2600 blobs in /lib/firmware/bes2600/.  The driver tries
# FACTORY_PATH first and falls back to FACTORY_ALT_PATH, so either layout works
# without a build-time override.
FACTORY_PATH                 ?= /lib/firmware/bes2600/bes2600_factory.txt
FACTORY_ALT_PATH             ?= /lib/firmware/bes2600_factory.txt
endif

# basic function
define boolen_flag
$(strip $(if $(findstring $($(1)),$(2)),-D$(1)))
endef

define string_flag
$(strip $(if $($(1)),-D$(1)=\"$($(1))\"))
endef

define value_flag
$(strip $(if $($(1)),-D$(1)=$($(1))))
endef


# ---------------------------------------------------------------------------
# Kernel API feature probes
#
# The driver is built out-of-tree against anything from v6.1 to v7.x, and
# distributions freely backport mac80211 changes into their stable trees.
# Deriving the API shape from LINUX_VERSION_CODE therefore gets it wrong on
# exactly the kernels people actually run.  Instead, probe the headers we are
# about to compile against.  Each probe defines a BES2600_HAVE_* macro that
# bes2600_compat.h consumes; when the headers cannot be found the compat
# header falls back to version checks.
#
# Note: the grep patterns deliberately avoid parentheses -- GNU make counts
# them inside $(shell ...) and an unbalanced one truncates the call.
# ---------------------------------------------------------------------------
BES_MAC80211_H := $(srctree)/include/net/mac80211.h
BES_TIMER_H    := $(srctree)/include/linux/timer.h

# $(call bes_probe,<file>,<pattern>,<macro>).  Patterns must spell any comma
# as $(bes_comma) -- make splits $(call) arguments on literal commas.
bes_comma := ,
bes_probe = $(if $(shell grep -q $(2) $(1) && echo y),-D$(3))

ifneq ($(wildcard $(BES_MAC80211_H)),)
ccflags-y += -DBES2600_HAVE_CONFTEST

# v6.11: ieee80211_ops::stop gained a 'bool suspend' argument.
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),'stop.*hw$(bes_comma) bool suspend',BES2600_HAVE_STOP_SUSPEND)

# v6.18: ieee80211_ops::config / ::set_rts_threshold gained 'int radio_idx'.
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),'config.*int radio_idx',BES2600_HAVE_CONFIG_RADIO_IDX)
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),'set_rts_threshold.*int radio_idx',BES2600_HAVE_RTS_RADIO_IDX)

# v6.11: ieee80211_bss_conf::chandef became ::chanreq.oper.
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),'struct ieee80211_chan_req.*chanreq;',BES2600_HAVE_BSS_CHANREQ)

# v6.3: legacy drivers hand their software queues to mac80211 with this helper.
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),ieee80211_handle_wake_tx_queue,BES2600_HAVE_HANDLE_WAKE_TX_QUEUE)

# v6.11: non-chanctx drivers must wire up the emulation helpers themselves.
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),ieee80211_emulate_add_chanctx,BES2600_HAVE_EMULATE_CHANCTX)

# v6.10: ieee80211_tx_status was renamed to ieee80211_tx_status_skb.
ccflags-y += $(call bes_probe,$(BES_MAC80211_H),ieee80211_tx_status_skb,BES2600_HAVE_TX_STATUS_SKB)
endif

BES_WORKQUEUE_H := $(srctree)/include/linux/workqueue.h
ifneq ($(wildcard $(BES_WORKQUEUE_H)),)
# v7.0: alloc_workqueue() callers must say WQ_PERCPU or WQ_UNBOUND explicitly.
ccflags-y += $(call bes_probe,$(BES_WORKQUEUE_H),WQ_PERCPU,BES2600_HAVE_WQ_PERCPU)
endif

ifneq ($(wildcard $(BES_TIMER_H)),)
# v6.2: del_timer_sync -> timer_delete_sync; the old name died in v6.16.
ccflags-y += $(call bes_probe,$(BES_TIMER_H),timer_delete_sync,BES2600_HAVE_TIMER_DELETE_SYNC)
# v6.16: from_timer -> timer_container_of.
ccflags-y += $(call bes_probe,$(BES_TIMER_H),timer_container_of,BES2600_HAVE_TIMER_CONTAINER_OF)
endif

ccflags-y += -Werror

SDIO_HOST_ADMA_SUPPORT                  ?= y
ccflags-y += -DCONFIG_BES2600_WLAN_BES
ccflags-y += -DBES_SDIO_RXTX_TOGGLE
ccflags-y += -DBES_SDIO_TX_MULTIPLE_ENABLE
ccflags-y += -DBES_SDIO_RX_MULTIPLE_ENABLE
ccflags-y += -DBES_UNIFIED_PM
ccflags-y += -DBES_SDIO_OPTIMIZED_LEN
ccflags-y += -DBES2600_HOST_TIMESTAMP_DEBUG

ifeq ($(BES2600_WRITE_DPD_TO_FILE),y)
BES2600_DPD_PATH ?= /data/cfg/bes2600_dpd.bin
BES2600_DEFAULT_DPD_PATH ?= /lib/firmware/bes2600_dpd.bin
BES2600_DPD_GOLDEN_PATH ?= /data/cfg/bes2600_dpd_golden.bin
endif

ifeq ($(BES2600_DUMP_FW_DPD_LOG),y)
BES2600_DPD_LOG_PATH ?= /data/applog/bes2600_dpd_log.log
endif

# compilation macros setting
ccflags-y += $(call boolen_flag,CONFIG_BES2600_VENDOR_CMD,y)

ccflags-y += $(call boolen_flag,CONFIG_BES2600_5GHZ_SUPPORT,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_STATIC_SDD,y)
ccflags-y += $(call boolen_flag,P2P_MULTIVIF,y)
ccflags-y += $(call boolen_flag,AP_AGGREGATE_FW_FIX,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_BT,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_WAPI_SUPPORT,y)
ccflags-y += $(call boolen_flag,WIFI_BT_COEXIST_EPTA_ENABLE,y)
ccflags-y += $(call boolen_flag,WIFI_BT_COEXIST_EPTA_FDD,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_DISABLE_BEACON_HINTS,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_NON_POWER_OF_TWO_BLOCKSIZES,y)
ccflags-y += $(call boolen_flag,BES2600_TX_MORE_RETRY,y)

ccflags-y += $(call boolen_flag,CONFIG_BES2600_ITP,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_DEBUGFS,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_BH_DEBUG,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_WSM_DEBUG,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_WSM_DUMPS,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_WSM_DUMPS_SHORT,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_TXRX_DEBUG,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_TX_POLICY_DEBUG,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_STA_DEBUG,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_ENABLE_DEVEL_LOGS,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_DUMP_ON_ERROR,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_WOWLAN,y)
ccflags-y += $(call value_flag,CONFIG_BES2600_LISTEN_INTERVAL)
ccflags-y += $(call value_flag,BES2600_FASTPS_IDLE_TIME)
ccflags-y += $(call boolen_flag,BSS_LOSS_CHECK,y)

ccflags-y += $(call string_flag,BES2600_LOAD_FW_TOOL_PATH)
ccflags-y += $(call string_flag,BES2600_LOAD_FW_TOOL_DEVICE)
ccflags-y += $(call string_flag,BES2600_DRV_VERSION)
ccflags-y += $(call string_flag,BES2600_DPD_PATH)
ccflags-y += $(call string_flag,BES2600_DEFAULT_DPD_PATH)
ccflags-y += $(call string_flag,BES2600_DPD_GOLDEN_PATH)

ccflags-y += $(call boolen_flag,BES2600_INDEPENDENT_EVB,y)
ccflags-y += $(call boolen_flag,BES2600_INTEGRATED_MODULE_V1,y)
ccflags-y += $(call boolen_flag,BES2600_INTEGRATED_MODULE_V2,y)
ccflags-y += $(call boolen_flag,BES2600_INTEGRATED_MODULE_V3,y)
ccflags-y += $(call boolen_flag,SDIO_HOST_ADMA_SUPPORT,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_TESTMODE,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_KEEP_ALIVE,y)

ccflags-y += $(call boolen_flag,CONFIG_BES2600_WIFI_BOOT_ON,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_BT_BOOT_ON,y)
ccflags-y += $(call boolen_flag,CONFIG_BES2600_CALIB_FROM_LINUX,y)

ccflags-y += $(call value_flag,GET_MAC_ADDR_METHOD)
ccflags-y += $(call string_flag,PATH_WIFI_MACADDR)
ccflags-y += $(call string_flag,FACTORY_PATH)
ccflags-y += $(call string_flag,FACTORY_ALT_PATH)
ccflags-y += $(call string_flag,FACTORY_DEFAULT_PATH)
ccflags-y += $(call boolen_flag,FACTORY_SAVE_MULTI_PATH,y)
ccflags-y += $(call boolen_flag,FACTORY_CRC_CHECK,y)

ccflags-y += $(call boolen_flag,BES2600_GPIO_WAKEUP_AP,y)
ccflags-y += $(call boolen_flag,BES2600_WRITE_DPD_TO_FILE,y)

ccflags-y += $(call boolen_flag,BES2600_DUMP_FW_DPD_LOG,y)
ccflags-y += $(call string_flag,BES2600_DPD_LOG_PATH)

ccflags-y += $(call boolen_flag,STANDARD_FACTORY_EFUSE_FLAG,y)

# internal feature options
ccflags-y += -DAP_HT_CAP_UPDATE
ccflags-y += -DBES2600_RX_IN_BH
ccflags-y += -DDCW1260_DETECTION_LOGIC

# sdd options
ccflags-y += -DTEST_11B=0
ccflags-y += -DDPD_CALI=0
ccflags-y += -DDPD_CALI=0
ccflags-y += -DALI_CONFG=0
ccflags-y += -DCHIP_WIFI_ROM_VER=1
ccflags-y += -DWIFI_OUT_FEM=0
ccflags-y += -DRF_TX_CONTROL_IO=16

# stbc rx option
ccflags-y += -DSTBC_RX_24G=0
ccflags-y += -DSTBC_RX_5G=0

#ccflags-y += -DP2P_STA_COEX
#ccflags-y += -DMCAST_FWDING
#ccflags-y += -DAP_AGGREGATE_FW_FIX
# Extra IE for probe response from upper layer is needed in P2P GO
# For offloading probe response to FW, the extra IE must be included
# in the probe response template
#ccflags-y += -DPROBE_RESP_EXTRA_IE
ccflags-y += -DIPV6_FILTERING

# basic files for building module
bes2600-y := \
	fwio.o \
	txrx.o \
	main.o \
	queue.o \
	hwio.o \
	bh.o \
	wsm.o \
	sta.o \
	ap.o \
	scan.o \
	txrx_opt.o \
	bes_chardev.o \
	tx_loop.o \
	bes_fw.o \
	bes_fw_common.o \
	bes2600_factory.o \
	bes2600_sdio.o

# compilation files select
bes2600-$(CONFIG_BES2600_DEBUGFS)       += debug.o
bes2600-$(CONFIG_BES2600_ITP)           += itp.o
bes2600-$(CONFIG_PM)                    += pm.o
bes2600-$(CONFIG_BES2600_BT)            += bes2600_btusb.o
bes2600-$(CONFIG_BES2600_TESTMODE)      += wifi_testmode_cmd.o

bes2600-$(WIFI_BT_COEXIST_EPTA_ENABLE)  += epta_coex.o epta_request.o
bes2600-$(CONFIG_BES2600_WOWLAN)        += bes_pwr.o

obj-$(BES2600) += bes2600.o

all: modules

modules clean:
	$(MAKE) -C $(KERN_DIR) M=$(shell pwd) $@
