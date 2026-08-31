/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Kernel API compatibility shims for the BES2600 driver.
 *
 * The driver is built out-of-tree against anything from v6.1 to v7.x.
 * Everything that papers over an upstream API change lives here so the rest
 * of the driver can be written against the most recent API.
 *
 * The Makefile probes $(srctree)/include/... for each API it cares about and
 * defines BES2600_HAVE_* accordingly, which is exact even on distribution
 * kernels that backport mac80211 changes into an older version number.  The
 * LINUX_VERSION_CODE fallbacks below are only used when that probe could not
 * run (BES2600_HAVE_CONFTEST undefined), e.g. an in-tree build.
 */
#ifndef BES2600_COMPAT_H_INCLUDED
#define BES2600_COMPAT_H_INCLUDED

#include <linux/version.h>
#include <linux/timer.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
#error "bes2600 requires Linux 6.1 or newer"
#endif

/* ---------------------------------------------------------------------- */
/* Version-based fallbacks, only when the Makefile probe did not run.	  */
/* ---------------------------------------------------------------------- */
#ifndef BES2600_HAVE_CONFTEST

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 2, 0)
#define BES2600_HAVE_TIMER_DELETE_SYNC		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 16, 0)
#define BES2600_HAVE_TIMER_CONTAINER_OF		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
#define BES2600_HAVE_HANDLE_WAKE_TX_QUEUE	1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
#define BES2600_HAVE_TX_STATUS_SKB		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
#define BES2600_HAVE_STOP_SUSPEND		1
#define BES2600_HAVE_BSS_CHANREQ		1
#define BES2600_HAVE_EMULATE_CHANCTX		1
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
#define BES2600_HAVE_CONFIG_RADIO_IDX		1
#define BES2600_HAVE_RTS_RADIO_IDX		1
#endif
/* conf_tx() has carried a link_id argument for the whole supported range. */
#define BES2600_HAVE_CONF_TX_LINK_ID		1

#endif /* !BES2600_HAVE_CONFTEST */

/* ---------------------------------------------------------------------- */
/* Timers								  */
/* ---------------------------------------------------------------------- */

/*
 * v6.2 renamed del_timer_sync()/del_timer() to timer_delete_sync()/
 * timer_delete(); the old names were removed outright in v6.16.
 */
#ifndef BES2600_HAVE_TIMER_DELETE_SYNC
#define timer_delete_sync(t)	del_timer_sync(t)
#define timer_delete(t)		del_timer(t)
#endif

/* v6.16 renamed from_timer() to timer_container_of(). */
#ifndef BES2600_HAVE_TIMER_CONTAINER_OF
#define timer_container_of(var, callback_timer, timer_fieldname) \
	from_timer(var, callback_timer, timer_fieldname)
#endif

/* ---------------------------------------------------------------------- */
/* mac80211								  */
/* ---------------------------------------------------------------------- */

/*
 * v6.11 replaced ieee80211_bss_conf::chandef with ::chanreq, whose .oper
 * member is the operating chandef the driver used to read directly.
 */
#ifdef BES2600_HAVE_BSS_CHANREQ
#define bes2600_bss_chandef(conf)	(&(conf)->chanreq.oper)
#else
#define bes2600_bss_chandef(conf)	(&(conf)->chandef)
#endif

/*
 * v6.11 removed mac80211's internal channel-context emulation.  Drivers that
 * do not implement chanctx must now point the four ops at the emulation
 * helpers themselves; leaving them NULL makes drv_add_chanctx() call through
 * a NULL pointer.
 */
#ifdef BES2600_HAVE_EMULATE_CHANCTX
#define BES2600_EMULATE_CHANCTX_OPS					\
	.add_chanctx		= ieee80211_emulate_add_chanctx,	\
	.remove_chanctx		= ieee80211_emulate_remove_chanctx,	\
	.change_chanctx		= ieee80211_emulate_change_chanctx,	\
	.switch_vif_chanctx	= ieee80211_emulate_switch_vif_chanctx,
#else
#define BES2600_EMULATE_CHANCTX_OPS
#endif

/* v6.10 renamed ieee80211_tx_status() to ieee80211_tx_status_skb(). */
#ifndef BES2600_HAVE_TX_STATUS_SKB
#define ieee80211_tx_status_skb(hw, skb)	ieee80211_tx_status(hw, skb)
#endif

#endif /* BES2600_COMPAT_H_INCLUDED */
