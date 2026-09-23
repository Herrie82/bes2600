/*
 * Mac80211 driver for BES2600 device
 *
 * Copyright (c) 2022, Bestechnic
 * Author:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "bes2600.h"
#include "debug.h"
#ifdef CONFIG_BES2600_DEBUGFS
/* join_status */
static const char * const bes2600_debug_join_status[] = {
	"passive",
	"monitor",
	"station",
	"access point",
};

/* WSM_JOIN_PREAMBLE_... */
static const char * const bes2600_debug_preamble[] = {
	"long",
	"short",
	"long on 1 and 2 Mbps",
};

static const char * const bes2600_debug_fw_types[] = {
	"ETF",
	"WFM",
	"WSM",
	"HI test",
	"Platform test",
};

static const char * const bes2600_debug_link_id[] = {
	"OFF",
	"REQ",
	"SOFT",
	"HARD",
};

static const char *bes2600_debug_mode(int mode)
{
	switch (mode) {
	case NL80211_IFTYPE_UNSPECIFIED:
		return "unspecified";
	case NL80211_IFTYPE_MONITOR:
		return "monitor";
	case NL80211_IFTYPE_STATION:
		return "station";
	case NL80211_IFTYPE_ADHOC:
		return "ad-hok";
	case NL80211_IFTYPE_MESH_POINT:
		return "mesh point";
	case NL80211_IFTYPE_AP:
		return "access point";
	case NL80211_IFTYPE_P2P_CLIENT:
		return "p2p client";
	case NL80211_IFTYPE_P2P_GO:
		return "p2p go";
	default:
		return "unsupported";
	}
}

static void bes2600_queue_status_show(struct seq_file *seq,
				     struct bes2600_queue *q)
{
	int i, if_id;
	seq_printf(seq, "Queue       %d:\n", (int)q->queue_id);
	seq_printf(seq, "  capacity: %ld\n", (long)q->capacity);
	seq_printf(seq, "  queued:   %ld\n", (long)q->num_queued);
	seq_printf(seq, "  pending:  %ld\n", (long)q->num_pending);
	seq_printf(seq, "  sent:     %ld\n", (long)q->num_sent);
	seq_printf(seq, "  locked:   %s\n", q->tx_locked_cnt ? "yes" : "no");
	seq_printf(seq, "  overfull: %s\n", q->overfull ? "yes" : "no");
	seq_puts(seq,   "  link map: 0-> ");
	for (if_id = 0; if_id < CW12XX_MAX_VIFS; if_id++) {
		for (i = 0; i < q->stats->map_capacity; ++i)
			seq_printf(seq, "%.2d ", q->link_map_cache[if_id][i]);
		seq_printf(seq, "<-%ld\n", (long)q->stats->map_capacity);
	}
}

static void bes2600_debug_print_map(struct seq_file *seq,
				   struct bes2600_vif *priv,
				   const char *label,
				   u32 map)
{
	int i;
	seq_printf(seq, "%s0-> ", label);
	for (i = 0; i < priv->hw_priv->tx_queue_stats.map_capacity; ++i)
		seq_printf(seq, "%s ", (map & BIT(i)) ? "**" : "..");
	seq_printf(seq, "<-%ld\n",
		   (long)priv->hw_priv->tx_queue_stats.map_capacity - 1);
}

static int bes2600_status_show_common(struct seq_file *seq, void *v)
{
	int i;
	struct list_head *item;
	struct bes2600_common *hw_priv = seq->private;
	struct bes2600_debug_common *d = hw_priv->debug;
	int ba_cnt, ba_acc, ba_cnt_rx, ba_acc_rx, ba_avg = 0, ba_avg_rx = 0;
	bool ba_ena;

	spin_lock_bh(&hw_priv->ba_lock);
	ba_cnt = hw_priv->debug->ba_cnt;
	ba_acc = hw_priv->debug->ba_acc;
	ba_cnt_rx = hw_priv->debug->ba_cnt_rx;
	ba_acc_rx = hw_priv->debug->ba_acc_rx;
	ba_ena = hw_priv->ba_ena;
	if (ba_cnt)
		ba_avg = ba_acc / ba_cnt;
	if (ba_cnt_rx)
		ba_avg_rx = ba_acc_rx / ba_cnt_rx;
	spin_unlock_bh(&hw_priv->ba_lock);

	seq_puts(seq,   "BES2600 Wireless LAN driver status\n");
	seq_printf(seq, "Hardware:   %d.%d\n",
		hw_priv->wsm_caps.hardwareId,
		hw_priv->wsm_caps.hardwareSubId);
	seq_printf(seq, "Firmware:   %s %d.%d\n",
		bes2600_debug_fw_types[hw_priv->wsm_caps.firmwareType],
		hw_priv->wsm_caps.firmwareVersion,
		hw_priv->wsm_caps.firmwareBuildNumber);
	seq_printf(seq, "FW API:     %d\n",
		hw_priv->wsm_caps.firmwareApiVer);
	seq_printf(seq, "FW caps:    0x%.4X\n",
		hw_priv->wsm_caps.firmwareCap);
	if (hw_priv->channel)
		seq_printf(seq, "Channel:    %d%s\n",
			hw_priv->channel->hw_value,
			hw_priv->channel_switch_in_progress ?
			" (switching)" : "");
	seq_printf(seq, "HT:         %s\n",
		bes2600_is_ht(&hw_priv->ht_info) ? "on" : "off");
	if (bes2600_is_ht(&hw_priv->ht_info)) {
		seq_printf(seq, "Greenfield: %s\n",
			bes2600_ht_greenfield(&hw_priv->ht_info) ? "yes" : "no");
		seq_printf(seq, "AMPDU dens: %d\n",
			bes2600_ht_ampdu_density(&hw_priv->ht_info));
	}
	spin_lock_bh(&hw_priv->tx_policy_cache.lock);
	i = 0;
	list_for_each(item, &hw_priv->tx_policy_cache.used)
		++i;
	spin_unlock_bh(&hw_priv->tx_policy_cache.lock);
	seq_printf(seq, "RC in use:  %d\n", i);
	seq_printf(seq, "BA stat:    %d, %d (%d)\n",
		ba_cnt, ba_acc, ba_avg);
	seq_printf(seq, "BA RX stat:    %d, %d (%d)\n",
		ba_cnt_rx, ba_acc_rx, ba_avg_rx);
	seq_printf(seq, "Block ACK:  %s\n", ba_ena ? "on" : "off");

	seq_puts(seq, "\n");
	for (i = 0; i < 4; ++i) {
		bes2600_queue_status_show(seq, &hw_priv->tx_queue[i]);
		seq_puts(seq, "\n");
	}
	seq_printf(seq, "TX burst:   %d\n",
		d->tx_burst);
	seq_printf(seq, "RX burst:   %d\n",
		d->rx_burst);
	seq_printf(seq, "TX miss:    %d\n",
		d->tx_cache_miss);
	seq_printf(seq, "Long retr:  %d\n",
		hw_priv->long_frame_max_tx_count);
	seq_printf(seq, "Short retr: %d\n",
		hw_priv->short_frame_max_tx_count);

	seq_printf(seq, "BH status:  %s\n",
		atomic_read(&hw_priv->bh_term) ? "terminated" : "alive");
	seq_printf(seq, "Pending RX: %d\n",
		atomic_read(&hw_priv->bh_rx));
	seq_printf(seq, "Pending TX: %d\n",
		atomic_read(&hw_priv->bh_tx));
	if (hw_priv->bh_error)
		seq_printf(seq, "BH errcode: %d\n",
			hw_priv->bh_error);
	seq_printf(seq, "TX bufs:    %d x %d bytes\n",
		hw_priv->wsm_caps.numInpChBufs,
		hw_priv->wsm_caps.sizeInpChBuf);
	seq_printf(seq, "Used bufs:  %d\n",
		hw_priv->hw_bufs_used);
	seq_printf(seq, "Device:     %s\n",
		bes2600_pwr_device_is_idle(hw_priv) ? "alseep" : "awake");

	spin_lock(&hw_priv->wsm_cmd.lock);
	seq_printf(seq, "WSM status: %s\n",
		hw_priv->wsm_cmd.done ? "idle" : "active");
	seq_printf(seq, "WSM cmd:    0x%.4X (%ld bytes)\n",
		hw_priv->wsm_cmd.cmd, (long)hw_priv->wsm_cmd.len);
	seq_printf(seq, "WSM retval: %d\n",
		hw_priv->wsm_cmd.ret);
	spin_unlock(&hw_priv->wsm_cmd.lock);

	seq_printf(seq, "Datapath:   %s\n",
		atomic_read(&hw_priv->tx_lock) ? "locked" : "unlocked");
	if (atomic_read(&hw_priv->tx_lock))
		seq_printf(seq, "TXlock cnt: %d\n",
			atomic_read(&hw_priv->tx_lock));

	seq_printf(seq, "Scan:       %s\n",
		atomic_read(&hw_priv->scan.in_progress) ? "active" : "idle");

	return 0;
}

static int bes2600_status_open_common(struct inode *inode, struct file *file)
{
	return single_open(file, &bes2600_status_show_common,
		inode->i_private);
}

static const struct file_operations fops_status_common = {
	.open = bes2600_status_open_common,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int bes2600_counters_show(struct seq_file *seq, void *v)
{
	int ret;
	struct bes2600_common *hw_priv = seq->private;
	struct wsm_counters_table counters;

	ret = wsm_get_counters_table(hw_priv, &counters);
	if (ret)
		return ret;

#define CAT_STR(x, y) x ## y
#define PUT_COUNTER(tab, name) \
	seq_printf(seq, "%s:" tab "%d\n", #name, \
		__le32_to_cpu(counters.CAT_STR(count, name)))

	PUT_COUNTER("\t\t", PlcpErrors);
	PUT_COUNTER("\t\t", FcsErrors);
	PUT_COUNTER("\t\t", TxPackets);
	PUT_COUNTER("\t\t", RxPackets);
	PUT_COUNTER("\t\t", RxPacketErrors);
	PUT_COUNTER("\t",   RxDecryptionFailures);
	PUT_COUNTER("\t\t", RxMicFailures);
	PUT_COUNTER("\t",   RxNoKeyFailures);
	PUT_COUNTER("\t",   TxMulticastFrames);
	PUT_COUNTER("\t",   TxFramesSuccess);
	PUT_COUNTER("\t",   TxFrameFailures);
	PUT_COUNTER("\t",   TxFramesRetried);
	PUT_COUNTER("\t",   TxFramesMultiRetried);
	PUT_COUNTER("\t",   RxFrameDuplicates);
	PUT_COUNTER("\t\t", RtsSuccess);
	PUT_COUNTER("\t\t", RtsFailures);
	PUT_COUNTER("\t\t", AckFailures);
	PUT_COUNTER("\t",   RxMulticastFrames);
	PUT_COUNTER("\t",   RxFramesSuccess);
	PUT_COUNTER("\t",   RxCMACICVErrors);
	PUT_COUNTER("\t\t", RxCMACReplays);
	PUT_COUNTER("\t",   RxMgmtCCMPReplays);

#undef PUT_COUNTER
#undef CAT_STR

	return 0;
}

static int bes2600_counters_open(struct inode *inode, struct file *file)
{
	return single_open(file, &bes2600_counters_show,
		inode->i_private);
}

static const struct file_operations fops_counters = {
	.open = bes2600_counters_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int bes2600_power_busy_event_show(struct seq_file *seq, void *v)
{
	struct bes2600_common *hw_priv = seq->private;
	char *buffer = NULL;

	buffer = kmalloc(8192, GFP_KERNEL);
	if(!buffer)
		return -ENOMEM;

	if(bes2600_pwr_busy_event_dump(hw_priv, buffer, 8192) == 0) {
		seq_printf(seq, "%s", buffer);
	} else {
		return -EFBIG;
	}

	kfree(buffer);

	return 0;
}

static int bes2600_power_busy_open(struct inode *inode, struct file *file)
{
	return single_open(file, &bes2600_power_busy_event_show,
		inode->i_private);
}

static const struct file_operations fops_power_busy_events = {
	.open = bes2600_power_busy_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int bes2600_generic_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t bes2600_11n_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_common *hw_priv = file->private_data;
	struct ieee80211_supported_band *band =
		hw_priv->hw->wiphy->bands[NL80211_BAND_2GHZ];
	return simple_read_from_buffer(user_buf, count, ppos,
		band->ht_cap.ht_supported ? "1\n" : "0\n", 2);
}

static ssize_t bes2600_11n_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_common *hw_priv = file->private_data;
	struct ieee80211_supported_band *band[2] = {
		hw_priv->hw->wiphy->bands[NL80211_BAND_2GHZ],
		hw_priv->hw->wiphy->bands[NL80211_BAND_5GHZ],
	};
	char buf[1];
	int ena = 0;

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, 1))
		return -EFAULT;
	if (buf[0] == 1)
		ena = 1;

	band[0]->ht_cap.ht_supported = ena;
	band[1]->ht_cap.ht_supported = ena;

	return count;
}

static const struct file_operations fops_11n = {
	.open = bes2600_generic_open,
	.read = bes2600_11n_read,
	.write = bes2600_11n_write,
	.llseek = default_llseek,
};

static ssize_t bes2600_wsm_dumps(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_common *hw_priv = file->private_data;
	char buf[1];

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, 1))
		return -EFAULT;

	if (buf[0] == '1')
		hw_priv->wsm_enable_wsm_dumps = 1;
	else
		hw_priv->wsm_enable_wsm_dumps = 0;

	return count;
}

static const struct file_operations fops_wsm_dumps = {
	.open = bes2600_generic_open,
	.write = bes2600_wsm_dumps,
	.llseek = default_llseek,
};

#if defined(CONFIG_BES2600_WSM_DUMPS_SHORT)
static ssize_t bes2600_short_dump_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_common *hw_priv = file->private_data;
	char buf[20];
	size_t size = 0;

	sprintf(buf, "Size: %u\n", hw_priv->wsm_dump_max_size);
	size = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos,
					buf, size);
}

static ssize_t bes2600_short_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_common *priv = file->private_data;
	char buf[20];
	unsigned long dump_size = 0;

	if (!count || count > 20)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	if (kstrtoul(buf, 10, &dump_size))
		return -EINVAL;

	priv->wsm_dump_max_size = dump_size;

	return count;
}

static const struct file_operations fops_short_dump = {
	.open = bes2600_generic_open,
	.write = bes2600_short_dump_write,
	.read = bes2600_short_dump_read,
	.llseek = default_llseek,
};
#endif /* CONFIG_BES2600_WSM_DUMPS_SHORT */

#ifdef BES2600_DUMP_FW_DPD_LOG
extern void bes2600_get_dpd_log(char **data, size_t *len);
static ssize_t bes2600_dpd_log_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char empty[] = {'\n', '\0'};
	char *data;
	size_t size;

	bes2600_get_dpd_log(&data, &size);
	if (!data) {
		data = empty;
		size = sizeof(empty);
	}

	return simple_read_from_buffer(user_buf, count, ppos,
	                               data, size);
}

static const struct file_operations dpd_log_dump = {
	.open = bes2600_generic_open,
	.read = bes2600_dpd_log_read,
	.llseek = default_llseek,
};
#endif /* BES2600_DUMP_FW_DPD_LOG */


/* Generous: the largest MIB this driver reads today is well under this. */
#define BES2600_MIB_PROBE_LEN	256

/*
 * mib_probe - ask the firmware to describe an undocumented MIB.
 *
 * Several MIB IDs in wsm.h were inherited wholesale from the ST-Ericsson
 * CW1200 driver this one descends from, with only a section number as a
 * comment and no payload struct -- SetMagicDataFrameFilter (0x101C), needed
 * for WIPHY_WOWLAN_MAGIC_PKT, is one of them.  No driver in that lineage
 * implements them (mainline cw1200 and both the mainline and upstream Silicon
 * Labs wfx drivers all name the ID and stop there), and the firmware image is
 * packed, so there is nothing to read statically.
 *
 * The firmware will answer though: a ReadMIB confirm carries the payload
 * length it considers correct for that ID, and its current contents.  An
 * unsupported ID fails the confirm instead.  That is enough to tell whether a
 * MIB exists at all, and to pin down its size before anyone writes to it.
 *
 *   echo 101c > /sys/kernel/debug/ieee80211/phy0/bes2600/mib_probe
 *   dmesg | tail
 *
 * Probe the two implemented neighbours (101a ethertype, 101b UDP port) first:
 * they are known-good controls for the method.
 */
static ssize_t bes2600_mib_probe_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_common *hw_priv = file->private_data;
	char buf[32];
	unsigned int mib_id;
	size_t got = 0;
	u8 *data;
	char *p;
	int ret;

	if (count == 0 || count >= sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;
	buf[count] = '\0';

	/*
	 * "<hexid>"          read the MIB
	 * "w <hexid> <len>"  write <len> zero bytes to it
	 *
	 * The filter MIBs in this family are write-only -- reads of 0x101A and
	 * 0x101B fail even though the driver writes both successfully -- so a
	 * read cannot tell us whether 0x101C exists.  A write can: the confirm
	 * carries a status, and wsm_generic_confirm() logs the firmware's own
	 * code on rejection.  Sweeping the length and watching which one is
	 * accepted is what pins the payload size down.
	 *
	 * Zeros are the benign payload here: for the two documented siblings a
	 * zeroed header means nrFilters = 0, i.e. "no filters", which is what
	 * the driver already writes to disable them.
	 */
	p = strim(buf);
	if (p[0] == 'w' && (p[1] == ' ' || p[1] == '\t')) {
		unsigned int wlen;

		if (sscanf(p + 1, "%x %u", &mib_id, &wlen) != 2)
			return -EINVAL;
		if (mib_id > 0xFFFF || wlen == 0 || wlen > BES2600_MIB_PROBE_LEN)
			return -EINVAL;

		data = kzalloc(wlen, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		ret = wsm_write_mib(hw_priv, (u16)mib_id, data, wlen, 0);
		bes_info("mib_probe: WRITE 0x%04x len=%u -> %s (%d)\n",
			 mib_id, wlen, ret ? "REJECTED" : "ACCEPTED", ret);
		kfree(data);
		return count;
	}

	if (kstrtouint(p, 16, &mib_id) || mib_id > 0xFFFF)
		return -EINVAL;

	data = kzalloc(BES2600_MIB_PROBE_LEN, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = wsm_read_mib_sized(hw_priv, (u16)mib_id, data,
				 BES2600_MIB_PROBE_LEN, &got);
	if (ret) {
		bes_info("mib_probe: 0x%04x not readable (read failed: %d)\n",
			 mib_id, ret);
	} else {
		bes_info("mib_probe: 0x%04x readable, firmware reports %zu byte payload\n",
			 mib_id, got);
		if (got)
			print_hex_dump(KERN_INFO, "mib_probe: ", DUMP_PREFIX_OFFSET,
				       16, 1, data, min(got, (size_t)BES2600_MIB_PROBE_LEN),
				       false);
	}

	kfree(data);
	return count;
}

/*
 * The chip's ROM is not reachable from the host, and this is where that was
 * established so nobody spends the effort again.
 *
 * The JOIN handler that refuses our requests lives in ROM: best2002_fw_sdio.bin
 * is a patch image, all four shipped images carry the same fourteen patches,
 * neither published firmware revision changes that set, and none of them patch
 * JOIN.  Reading the ROM would settle what it objects to.  Taken from the patch
 * image itself, the address space is
 *
 *   0x20000168   patch code, i.e. the firmware file as loaded
 *   0x60000000   ROM, ~192KB (710 of 725 distinct Thumb function pointers land
 *                in 0x60000000-0x6002ffff, a second cluster of 133 near
 *                0x60110000)
 *   0x8001xxxx   RAM globals shared between patch code and ROM code
 *
 * bes2600_indirect_read() in hwio.c looks like the way to read it: set
 * ST90TDS_SRAM_BASE_ADDR_REG_ID, pulse the prefetch bit, drain
 * ST90TDS_SRAM_DPORT_REG_ID.  It is inherited from cw1200 and nothing in this
 * driver has ever called it.
 *
 * It does not work here, and it is not harmless.  A debugfs probe that called
 * bes2600_apb_read() blocked inside sdio_claim_host() without printing
 * anything at all -- not the success path, not either error path -- and on the
 * second attempt took the device off both USB and WiFi until it was power
 * cycled.  Nothing reached the log before it went.  The conclusion is that
 * those ST90TDS registers are not decoded on BES2600 silicon and addressing
 * them wedges the SDIO bus.
 *
 * So the probe is gone rather than left behind a warning.  If the ROM is ever
 * worth another attempt, the remaining route is the bootloader: it takes a
 * RUN_CODE command (FRAME_HEADER_RUN_CODE, 0xB4) and the firmware is
 * downloaded to RAM on every boot, so a stub that copies ROM somewhere the
 * host already reads back costs a power cycle if it is wrong.
 *
 * The chip's UART is not a way in either, though it looks like one.  The
 * PineTab2 DTS wires uart1 "for wifi chip comm" with RX on a real uart pin and
 * TX left as plain GPIO, which is the shape of a debug log tap, and the
 * firmware carries hal_trace_open, hal_trace_output and
 * hal_trace_crash_dump_register.  It is silent: /dev/ttyS1 is fe650000.serial,
 * the pinmux is applied -- "pin 75 (gpio2-11): fe650000.serial (GPIO
 * UNCLAIMED) function uart1 group uart1m0-xfer" -- and reading it produced
 * zero bytes at 115200, 460800, 921600, 1500000, 2000000 and 3000000 baud,
 * idle and during a scan.  Either trace output is not enabled in this build of
 * the firmware or it goes somewhere else entirely.
 *
 * What the firmware would tell us, if we could ask, is now known.
 * open-vela/vendor_bes ships prebuilt NuttX libraries -- libnx_bestbsp_ap.a on
 * best1700_ep and best2003_ep -- built from BES's own copy of this driver at
 * net/wifi_stack/v1/drv/cw1260/, with full DWARF.  wsm.o, sta.o, txrx.o, bh.o,
 * fwio.o, queue.o and cw1200_sdio.o are all there, and their struct layouts
 * match ours byte for byte, struct wsm_join included at 68 bytes.  That build
 * runs on the chip's own MCU rather than across SDIO, so it reads LMAC state
 * straight off the internal bus.  Its print_lmac_status() names the addresses:
 *
 *   0x82c00600   PAC_RXC_RX_CONTROL
 *   0x82c00604   PAC_RXC_RX_BUFFER_IN_POINTER
 *   0x82c00608   PAC_RXC_RX_BUFFER_OUT_POINTER
 *   0x82c00624   PAC_RXC_RX_ERROR_COUNTERS_0
 *   0x82c00628   PAC_RXC_RX_ERROR_COUNTERS_1
 *   0x82c0062c   PAC_RXC_RX_ERROR_COUNTERS_2
 *   0x82c00638   PAC_RXC_RX_STATE
 *   0x80000678   sLmcEptaGlobal.u8ReqForMediumFlags
 *   0x80000990   SchedulerEvents
 *   0x80000d00   outptr
 *   0x80000d04   out2ptr
 *   0x80001520   pTxRequestFifo->FifoGet
 *   0x80001524   pTxRequestFifo->FifoPut
 *   0x800042b0   sLmcLocal.sTxQueue.head
 *   0x800042b4   sLmcLocal.sTxQueue.Tail
 *   0x8000b5c8   SuspendRxTaskMask
 *   0x90000144   LMAC pc
 *   0x90000148   LMAC lr
 *
 * This is what BES_IW_PRIV_CMD_ID_SET_LMAC_LOG_MODE in their bes_wl_priv.h
 * switches on, and it is a memory read on a bus we do not sit on -- not a WSM
 * command, and not a MIB.  Sweeping the unclaimed 0x10xx MIB IDs for a logging
 * control would have found nothing, because there is nothing there to find.
 *
 * The same library has cw1200_cca_soft_reset(), which we do not:
 *
 *	val = readl(0x90100c00);
 *	writel(val & ~1, 0x90100c00);
 *	hal_sys_timer_delay_us(10);
 *	writel(val | 1, 0x90100c00);
 *
 * cw1200_bss_loss_work() calls it before probing the AP, on the beacon-loss
 * path only.  A CCA block that has latched busy is exactly what this link
 * looks like from outside -- see the retry figures in txrx.c -- so it is worth
 * knowing that BES's own driver kicks it.  Reaching 0x90100c00 from the host
 * needs the same ST90TDS window that wedges the bus above, so it stays a note.
 */

static const struct file_operations fops_mib_probe = {
	.open = bes2600_generic_open,
	.write = bes2600_mib_probe_write,
	.llseek = default_llseek,
};

int bes2600_debug_init_common(struct bes2600_common *hw_priv)
{
	int ret = -ENOMEM;
	struct bes2600_debug_common *d =
		kzalloc(sizeof(struct bes2600_debug_common), GFP_KERNEL);
	hw_priv->debug = d;
	if (!d)
		return ret;

	d->debugfs_phy = debugfs_create_dir("bes2600",
			hw_priv->hw->wiphy->debugfsdir);
	if (!d->debugfs_phy)
		goto err;

	if (!debugfs_create_file("status", S_IRUSR, d->debugfs_phy,
			hw_priv, &fops_status_common))
		goto err;

	if (!debugfs_create_file("counters", S_IRUSR, d->debugfs_phy,
			hw_priv, &fops_counters))
		goto err;

	if (!debugfs_create_file("11n", S_IRUSR | S_IWUSR,
			d->debugfs_phy, hw_priv, &fops_11n))
		goto err;

	if (!debugfs_create_file("mib_probe", S_IWUSR, d->debugfs_phy,
			hw_priv, &fops_mib_probe))
		goto err;

	if (!debugfs_create_file("wsm_dumps", S_IWUSR, d->debugfs_phy,
			hw_priv, &fops_wsm_dumps))
		goto err;

#if defined(CONFIG_BES2600_WSM_DUMPS_SHORT)
	if (!debugfs_create_file("wsm_dump_size", S_IRUSR | S_IWUSR,
			d->debugfs_phy, hw_priv, &fops_short_dump))
		goto err;
#endif /* CONFIG_BES2600_WSM_DUMPS_SHORT */

#ifdef CONFIG_BES2600_WOWLAN
	if (!debugfs_create_file("power_events", S_IRUSR, d->debugfs_phy,
			hw_priv, &fops_power_busy_events))
		goto err;
#endif

#ifdef BES2600_DUMP_FW_DPD_LOG
	if (!debugfs_create_file("dpd_log", S_IRUSR, d->debugfs_phy,
			hw_priv, &dpd_log_dump))
		goto err;
#endif

	ret = bes2600_itp_init(hw_priv);
	if (ret)
		goto err;

	return 0;

err:
	hw_priv->debug = NULL;
	debugfs_remove_recursive(d->debugfs_phy);
	kfree(d);
	return ret;
}

void bes2600_debug_release_common(struct bes2600_common *hw_priv)
{
	struct bes2600_debug_common *d = hw_priv->debug;
	if (d) {
		bes2600_itp_release(hw_priv);
		hw_priv->debug = NULL;
		kfree(d);
	}
}

static int bes2600_status_show_priv(struct seq_file *seq, void *v)
{
	int i;
	struct bes2600_vif *priv = seq->private;
	struct bes2600_debug_priv *d = priv->debug;

	seq_printf(seq, "Mode:       %s%s\n",
		bes2600_debug_mode(priv->mode),
		priv->listening ? " (listening)" : "");
	seq_printf(seq, "Assoc:      %s\n",
		bes2600_debug_join_status[priv->join_status]);
	if (priv->rx_filter.promiscuous)
		seq_puts(seq,   "Filter:     promisc\n");
	else if (priv->rx_filter.fcs)
		seq_puts(seq,   "Filter:     fcs\n");
	if (priv->rx_filter.bssid)
		seq_puts(seq,   "Filter:     bssid\n");
	if (priv->bf_control.bcn_count)
		seq_puts(seq,   "Filter:     beacons\n");

	if (priv->enable_beacon ||
			priv->mode == NL80211_IFTYPE_AP ||
			priv->mode == NL80211_IFTYPE_ADHOC ||
			priv->mode == NL80211_IFTYPE_MESH_POINT ||
			priv->mode == NL80211_IFTYPE_P2P_GO)
		seq_printf(seq, "Beaconing:  %s\n",
			priv->enable_beacon ?
			"enabled" : "disabled");
	if (priv->ssid_length ||
			priv->mode == NL80211_IFTYPE_AP ||
			priv->mode == NL80211_IFTYPE_ADHOC ||
			priv->mode == NL80211_IFTYPE_MESH_POINT ||
			priv->mode == NL80211_IFTYPE_P2P_GO)
		seq_printf(seq, "SSID:       %.*s\n",
			(int)priv->ssid_length, priv->ssid);

	for (i = 0; i < 4; ++i) {
		seq_printf(seq, "EDCA(%d):    %d, %d, %d, %d, %d\n", i,
			priv->edca.params[i].cwMin,
			priv->edca.params[i].cwMax,
			priv->edca.params[i].aifns,
			priv->edca.params[i].txOpLimit,
			priv->edca.params[i].maxReceiveLifetime);
	}
	if (priv->join_status == BES2600_JOIN_STATUS_STA) {
		static const char *pmMode = "unknown";
		switch (priv->powersave_mode.pmMode) {
		case WSM_PSM_ACTIVE:
			pmMode = "off";
			break;
		case WSM_PSM_PS:
			pmMode = "on";
			break;
		case WSM_PSM_FAST_PS:
			pmMode = "dynamic";
			break;
		}
		seq_printf(seq, "Preamble:   %s\n",
			bes2600_debug_preamble[
			priv->association_mode.preambleType]);
		seq_printf(seq, "AMPDU spcn: %d\n",
			priv->association_mode.mpduStartSpacing);
		seq_printf(seq, "Basic rate: 0x%.8X\n",
			le32_to_cpu(priv->association_mode.basicRateSet));
		seq_printf(seq, "Bss lost:   %d beacons\n",
			priv->bss_params.beaconLostCount);
		seq_printf(seq, "AID:        %d\n",
			priv->bss_params.aid);
		seq_printf(seq, "Rates:      0x%.8X\n",
			priv->bss_params.operationalRateSet);
		seq_printf(seq, "Powersave:  %s\n", pmMode);
	}
	seq_printf(seq, "RSSI thold: %d\n",
		priv->cqm_rssi_thold);
	seq_printf(seq, "RSSI hyst:  %d\n",
		priv->cqm_rssi_hyst);
	seq_printf(seq, "TXFL thold: %d\n",
		priv->cqm_tx_failure_thold);
	seq_printf(seq, "Linkloss:   %d\n",
		priv->cqm_link_loss_count);
	seq_printf(seq, "Bcnloss:    %d\n",
		priv->cqm_beacon_loss_count);

	bes2600_debug_print_map(seq, priv, "Link map:   ",
		priv->link_id_map);
	bes2600_debug_print_map(seq, priv, "Asleep map: ",
		priv->sta_asleep_mask);
	bes2600_debug_print_map(seq, priv, "PSPOLL map: ",
		priv->pspoll_mask);

	seq_puts(seq, "\n");

	for (i = 0; i < CW1250_MAX_STA_IN_AP_MODE; ++i) {
		if (priv->link_id_db[i].status) {
			seq_printf(seq, "Link %d:     %s, %pM\n",
				i + 1, bes2600_debug_link_id[
				priv->link_id_db[i].status],
				priv->link_id_db[i].mac);
		}
	}

	seq_puts(seq, "\n");

	seq_printf(seq, "Powermgmt:  %s\n",
		priv->powersave_enabled ? "on" : "off");

	seq_printf(seq, "TXed:       %d\n",
		d->tx);
	seq_printf(seq, "AGG TXed:   %d\n",
		d->tx_agg);
	seq_printf(seq, "MULTI TXed: %d (%d)\n",
		d->tx_multi, d->tx_multi_frames);
	seq_printf(seq, "RXed:       %d\n",
		d->rx);
	seq_printf(seq, "AGG RXed:   %d\n",
		d->rx_agg);
	seq_printf(seq, "TX align:   %d\n",
		d->tx_align);
	seq_printf(seq, "TX TTL:     %d\n",
		d->tx_ttl);
	return 0;
}

static int bes2600_status_open_priv(struct inode *inode, struct file *file)
{
	return single_open(file, &bes2600_status_show_priv,
		inode->i_private);
}

static const struct file_operations fops_status_priv = {
	.open = bes2600_status_open_priv,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

#if defined(CONFIG_BES2600_USE_STE_EXTENSIONS)

static ssize_t bes2600_hang_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bes2600_vif *priv = file->private_data;
	struct bes2600_common *hw_priv = cw12xx_vifpriv_to_hwpriv(priv);
	char buf[1];

	if (!count)
		return -EINVAL;
	if (copy_from_user(buf, user_buf, 1))
		return -EFAULT;

	if (priv->vif) {
		bes2600_pm_stay_awake(&hw_priv->pm_state, 3*HZ);
		ieee80211_driver_hang_notify(priv->vif, GFP_KERNEL);
	} else
		return -ENODEV;

	return count;
}

static const struct file_operations fops_hang = {
	.open = bes2600_generic_open,
	.write = bes2600_hang_write,
	.llseek = default_llseek,
};
#endif

#define VIF_DEBUGFS_NAME_S 10
int bes2600_debug_init_priv(struct bes2600_common *hw_priv,
			   struct bes2600_vif *priv)
{
	int ret = -ENOMEM;
	struct bes2600_debug_priv *d;
	char name[VIF_DEBUGFS_NAME_S];

	if (WARN_ON(!hw_priv))
		return ret;

	if (WARN_ON(!hw_priv->debug))
		return ret;

	d = kzalloc(sizeof(struct bes2600_debug_priv), GFP_KERNEL);
	priv->debug = d;
	if (WARN_ON(!d))
		return ret;

	memset(name, 0, VIF_DEBUGFS_NAME_S);
	ret = snprintf(name, VIF_DEBUGFS_NAME_S, "vif_%d", priv->if_id);
	if (WARN_ON(ret < 0))
		goto err;

	d->debugfs_phy = debugfs_create_dir(name,
					    hw_priv->debug->debugfs_phy);
	if (WARN_ON(!d->debugfs_phy))
		goto err;

#if defined(CONFIG_BES2600_USE_STE_EXTENSIONS)
	if (WARN_ON(!debugfs_create_file("hang", S_IWUSR, d->debugfs_phy,
			priv, &fops_hang)))
		goto err;
#endif

	if (!debugfs_create_file("status", S_IRUSR, d->debugfs_phy,
			priv, &fops_status_priv))
		goto err;

	return 0;
err:
	priv->debug = NULL;
	debugfs_remove_recursive(d->debugfs_phy);
	kfree(d);
	return ret;

}

void bes2600_debug_release_priv(struct bes2600_vif *priv)
{
	struct bes2600_debug_priv *d = priv->debug;
	if (d) {
		debugfs_remove_recursive(priv->debug->debugfs_phy);
		priv->debug = NULL;
		kfree(d);
	}
}

int bes2600_print_fw_version(struct bes2600_common *hw_priv, u8* buf, size_t len)
{
	return snprintf(buf, len, "%s %d.%d",
			bes2600_debug_fw_types[hw_priv->wsm_caps.firmwareType],
			hw_priv->wsm_caps.firmwareVersion,
			hw_priv->wsm_caps.firmwareBuildNumber);
}
#endif
