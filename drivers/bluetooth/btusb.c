/*
 *
 *  Generic Bluetooth USB driver
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/firmware.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#define VERSION "0.6"

static bool ignore_dga;
static bool ignore_csr;
static bool ignore_sniffer;
static bool disable_scofix;
static bool force_scofix;

static bool reset = 1;

static struct usb_driver btusb_driver;

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
static char rom_patch_version[64] = { 0 };
#endif

#define BTUSB_IGNORE		0x01
#define BTUSB_DIGIANSWER	0x02
#define BTUSB_CSR		0x04
#define BTUSB_SNIFFER		0x08
#define BTUSB_BCM92035		0x10
#define BTUSB_BROKEN_ISOC	0x20
#define BTUSB_WRONG_SCO_MTU	0x40
#define BTUSB_ATH3012		0x80
#define BTUSB_INTEL		0x100
#define BTUSB_INTEL_BOOT	0x200
#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
#define BTUSB_MT7662TU		0x400
#endif

static struct usb_device_id btusb_table[] = {
	/* Generic Bluetooth USB device */
	{ USB_DEVICE_INFO(0xe0, 0x01, 0x01) },

	/* Apple-specific (Broadcom) devices */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x05ac, 0xff, 0x01, 0x01) },

	/* MediaTek MT76x0E */
	{ USB_DEVICE(0x0e8d, 0x763f) },

	/* Broadcom SoftSailing reporting vendor specific */
	{ USB_DEVICE(0x0a5c, 0x21e1) },

	/* Apple MacBookPro 7,1 */
	{ USB_DEVICE(0x05ac, 0x8213) },

	/* Apple iMac11,1 */
	{ USB_DEVICE(0x05ac, 0x8215) },

	/* Apple MacBookPro6,2 */
	{ USB_DEVICE(0x05ac, 0x8218) },

	/* Apple MacBookAir3,1, MacBookAir3,2 */
	{ USB_DEVICE(0x05ac, 0x821b) },

	/* Apple MacBookAir4,1 */
	{ USB_DEVICE(0x05ac, 0x821f) },

	/* Apple MacBookPro8,2 */
	{ USB_DEVICE(0x05ac, 0x821a) },

	/* Apple MacMini5,1 */
	{ USB_DEVICE(0x05ac, 0x8281) },

	/* AVM BlueFRITZ! USB v2.0 */
	{ USB_DEVICE(0x057c, 0x3800) },

	/* Bluetooth Ultraport Module from IBM */
	{ USB_DEVICE(0x04bf, 0x030a) },

	/* ALPS Modules with non-standard id */
	{ USB_DEVICE(0x044e, 0x3001) },
	{ USB_DEVICE(0x044e, 0x3002) },

	/* Ericsson with non-standard id */
	{ USB_DEVICE(0x0bdb, 0x1002) },

	/* Canyon CN-BTU1 with HID interfaces */
	{ USB_DEVICE(0x0c10, 0x0000) },

	/* Broadcom BCM20702A0 */
	{ USB_DEVICE(0x0b05, 0x17b5) },
	{ USB_DEVICE(0x0b05, 0x17cb) },
	{ USB_DEVICE(0x04ca, 0x2003) },
	{ USB_DEVICE(0x0489, 0xe042) },
	{ USB_DEVICE(0x413c, 0x8197) },

	/* Foxconn - Hon Hai */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x0489, 0xff, 0x01, 0x01) },

	/*Broadcom devices with vendor specific id */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x0a5c, 0xff, 0x01, 0x01) },

	/* IMC Networks - Broadcom based */
	{ USB_VENDOR_AND_INTERFACE_INFO(0x13d3, 0xff, 0x01, 0x01) },

	/* Intel Bluetooth USB Bootloader (RAM module) */
	{ USB_DEVICE(0x8087, 0x0a5a),
	  .driver_info = BTUSB_INTEL_BOOT | BTUSB_BROKEN_ISOC },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, btusb_table);

static struct usb_device_id blacklist_table[] = {
	/* CSR BlueCore devices */
	{ USB_DEVICE(0x0a12, 0x0001), .driver_info = BTUSB_CSR },

	/* Broadcom BCM2033 without firmware */
	{ USB_DEVICE(0x0a5c, 0x2033), .driver_info = BTUSB_IGNORE },

	/* Atheros 3011 with sflash firmware */
	{ USB_DEVICE(0x0cf3, 0x3002), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0cf3, 0xe019), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x13d3, 0x3304), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0930, 0x0215), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0489, 0xe03d), .driver_info = BTUSB_IGNORE },
	{ USB_DEVICE(0x0489, 0xe027), .driver_info = BTUSB_IGNORE },

	/* Atheros AR9285 Malbec with sflash firmware */
	{ USB_DEVICE(0x03f0, 0x311d), .driver_info = BTUSB_IGNORE },

	/* Atheros 3012 with sflash firmware */
	{ USB_DEVICE(0x0cf3, 0x0036), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3008), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311d), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311e), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x311f), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x817a), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3375), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3005), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3006), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3007), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04ca, 0x3008), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3362), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe004), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe005), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0930, 0x0219), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe057), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3393), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe04e), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe056), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe04d), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x04c5, 0x1330), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x13d3, 0x3402), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0x3121), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0cf3, 0xe003), .driver_info = BTUSB_ATH3012 },

	/* Atheros AR5BBU12 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xe02c), .driver_info = BTUSB_IGNORE },

	/* Atheros AR5BBU12 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xe03c), .driver_info = BTUSB_ATH3012 },
	{ USB_DEVICE(0x0489, 0xe036), .driver_info = BTUSB_ATH3012 },

	/* Broadcom BCM2035 */
	{ USB_DEVICE(0x0a5c, 0x2035), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x200a), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x2009), .driver_info = BTUSB_BCM92035 },

	/* Broadcom BCM2045 */
	{ USB_DEVICE(0x0a5c, 0x2039), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x2101), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* IBM/Lenovo ThinkPad with Broadcom chip */
	{ USB_DEVICE(0x0a5c, 0x201e), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x0a5c, 0x2110), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* HP laptop with Broadcom chip */
	{ USB_DEVICE(0x03f0, 0x171d), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Dell laptop with Broadcom chip */
	{ USB_DEVICE(0x413c, 0x8126), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Dell Wireless 370 and 410 devices */
	{ USB_DEVICE(0x413c, 0x8152), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x413c, 0x8156), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Belkin F8T012 and F8T013 devices */
	{ USB_DEVICE(0x050d, 0x0012), .driver_info = BTUSB_WRONG_SCO_MTU },
	{ USB_DEVICE(0x050d, 0x0013), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Asus WL-BTD202 device */
	{ USB_DEVICE(0x0b05, 0x1715), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* Kensington Bluetooth USB adapter */
	{ USB_DEVICE(0x047d, 0x105e), .driver_info = BTUSB_WRONG_SCO_MTU },

	/* RTX Telecom based adapters with buggy SCO support */
	{ USB_DEVICE(0x0400, 0x0807), .driver_info = BTUSB_BROKEN_ISOC },
	{ USB_DEVICE(0x0400, 0x080a), .driver_info = BTUSB_BROKEN_ISOC },

	/* CONWISE Technology based adapters with buggy SCO support */
	{ USB_DEVICE(0x0e5e, 0x6622), .driver_info = BTUSB_BROKEN_ISOC },

	/* Digianswer devices */
	{ USB_DEVICE(0x08fd, 0x0001), .driver_info = BTUSB_DIGIANSWER },
	{ USB_DEVICE(0x08fd, 0x0002), .driver_info = BTUSB_IGNORE },

	/* CSR BlueCore Bluetooth Sniffer */
	{ USB_DEVICE(0x0a12, 0x0002), .driver_info = BTUSB_SNIFFER },

	/* Frontline ComProbe Bluetooth Sniffer */
	{ USB_DEVICE(0x16d3, 0x0002), .driver_info = BTUSB_SNIFFER },

	/* Intel Bluetooth device */
	{ USB_DEVICE(0x8087, 0x07dc), .driver_info = BTUSB_INTEL },

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
	/* MT7662TU device */
	{ USB_DEVICE(0x0e8d, 0x76a0), .driver_info = BTUSB_MT7662TU },
#endif

	{ }	/* Terminating entry */
};

#define BTUSB_MAX_ISOC_FRAMES	10

#define BTUSB_INTR_RUNNING	0
#define BTUSB_BULK_RUNNING	1
#define BTUSB_ISOC_RUNNING	2
#define BTUSB_SUSPENDING	3
#define BTUSB_DID_ISO_RESUME	4

struct btusb_data {
	struct hci_dev       *hdev;
	struct usb_device    *udev;
	struct usb_interface *intf;
	struct usb_interface *isoc;

	spinlock_t lock;

	unsigned long flags;

	struct work_struct work;
	struct work_struct waker;

	struct usb_anchor tx_anchor;
	struct usb_anchor intr_anchor;
	struct usb_anchor bulk_anchor;
	struct usb_anchor isoc_anchor;
	struct usb_anchor deferred;
	int tx_in_flight;
	spinlock_t txlock;

	struct usb_endpoint_descriptor *intr_ep;
	struct usb_endpoint_descriptor *bulk_tx_ep;
	struct usb_endpoint_descriptor *bulk_rx_ep;
	struct usb_endpoint_descriptor *isoc_tx_ep;
	struct usb_endpoint_descriptor *isoc_rx_ep;

	__u8 cmdreq_type;

	unsigned int sco_num;
	int isoc_altsetting;
	int suspend_count;

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
	/* request for different io operation */
	u8 w_request;
	u8 r_request;

	/* io buffer for usb control transfer */
	unsigned char *io_buf;

	unsigned char *rom_patch;
	unsigned char *rom_patch_bin_file_name;
	u32 chip_id;
	u8 need_load_rom_patch;
	u32 rom_patch_offset;
	u32 rom_patch_len;
#endif
};

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
/* SYS Control */
#define SYSCTL	0x400000

/* WLAN */
#define WLAN	0x410000

#define CONTROL_TIMEOUT_JIFFIES		100
#define DEVICE_VENDOR_REQUEST_OUT	0x40
#define DEVICE_VENDOR_REQUEST_IN	0xc0
#define DEVICE_CLASS_REQUEST_OUT	0x20
#define DEVICE_CLASS_REQUEST_IN		0xa0

/* MCUCTL */
#define CLOCK_CTL		0x0708
#define INT_LEVEL		0x0718
#define COM_REG0		0x0730
#define SEMAPHORE_00	0x07B0
#define SEMAPHORE_01	0x07B4
#define SEMAPHORE_02	0x07B8
#define SEMAPHORE_03	0x07BC

/* ROM Patch */
#define PATCH_HCI_HEADER_SIZE 4
#define PATCH_WMT_HEADER_SIZE 5
#define PATCH_HEADER_SIZE (PATCH_HCI_HEADER_SIZE + PATCH_WMT_HEADER_SIZE)
#define UPLOAD_PATCH_UNIT 2048
#define PATCH_INFO_SIZE 30
#define PATCH_PHASE1 1
#define PATCH_PHASE2 2
#define PATCH_PHASE3 3

static int btusb_io_read32(struct btusb_data *data, u32 reg, u32 *val)
{
	u8 request = data->r_request;
	struct usb_device *udev = data->udev;
	int ret;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), request,
				DEVICE_VENDOR_REQUEST_IN, 0x0, reg,
				data->io_buf, 4, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		*val = 0xffffffff;
		BT_ERR("%s error(%d), reg=%x, value=%x\n",
			__func__, ret, reg, *val);
		return ret;
	}

	memmove(val, data->io_buf, 4);

	*val = le32_to_cpu(*val);

	if (ret > 0)
		ret = 0;

	return ret;
}

static int btusb_io_write32(struct btusb_data *data, u32 reg, u32 val)
{
	u16 value, index;
	u8 request = data->w_request;
	struct usb_device *udev = data->udev;
	int ret;

	index = (u16) reg;
	value = val & 0x0000ffff;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request,
				DEVICE_VENDOR_REQUEST_OUT, value, index,
				NULL, 0, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		BT_ERR("%s error(%d), reg=%x, value=%x\n",
			__func__, ret, reg, val);
		return ret;
	}

	index = (u16) (reg + 2);
	value = (val & 0xffff0000) >> 16;

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), request,
				DEVICE_VENDOR_REQUEST_OUT, value, index,
				NULL, 0, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		BT_ERR("%s error2(%d), reg=%x, value=%x\n",
			__func__, ret, reg, val);
		return ret;
	}

	if (ret > 0)
		ret = 0;

	return ret;
}

static void send_dummy_bulk_out_packet_complete(struct urb *urb)
{
}

static int btusb_send_dummy_bulk_out_packet(struct btusb_data *data)
{
	int ret = 0;
	unsigned char buf[8] = {0};
	struct urb *urb;
	unsigned int pipe;

	BT_DBG("%s\n", __func__);
	pipe = usb_sndbulkpipe(data->udev, data->bulk_tx_ep->bEndpointAddress);

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		BT_ERR("%s: usb_alloc_usb failed!\n", __func__);
		goto error0;
	}
	usb_fill_bulk_urb(urb, data->udev, pipe, buf, sizeof(buf),
				send_dummy_bulk_out_packet_complete, NULL);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret)
		BT_ERR("%s: submit urb failed!\n", __func__);

	usb_free_urb(urb);
error0:
	return ret;
}

static int btusb_send_hci_reset_cmd(struct usb_device *udev)
{
	int retry_counter = 0;
	/* Send HCI Reset */
	{
		int ret = 0;
		char buf[4] = { 0 };
		buf[0] = 0x03;
		buf[1] = 0x0C;
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0,
					DEVICE_CLASS_REQUEST_OUT, 0x00, 0x00,
					buf, 0x03, CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			BT_ERR("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		while (1) {
			int ret = 0;
			char buf[64] = { 0 };
			int actual_length;
			ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
						buf, 64, &actual_length, 2000);

			if (ret < 0) {
				BT_ERR("%s error2(%d)\n", __func__, ret);
				return ret;
			}

			if (actual_length == 6 &&
			    buf[0] == 0x0e &&
			    buf[1] == 0x04 &&
			    buf[2] == 0x01 &&
			    buf[3] == 0x03 &&
			    buf[4] == 0x0c &&
			    buf[5] == 0x00) {
				break;
			} else {
				int i;
				BT_INFO("%s drop unknown event :\n", __func__);
				for (i = 0; i < actual_length && i < 64; i++)
					BT_INFO("%02X ", buf[i]);
				BT_INFO("\n");
				mdelay(10);
				retry_counter++;
			}

			if (retry_counter > 10) {
				BT_ERR("%s retry timeout!\n", __func__);
				return ret;
			}
		}
	}

	BT_INFO("btusb_send_hci_reset_cmd : OK\n");
	return 0;
}

static int btusb_send_hci_set_ce_cmd(struct usb_device *udev)
{
	char result_buf[64] = { 0 };

	/* Send HCI Reset, read 0x41070c */
	{
		int ret = 0;
		char buf[8] = { 0xd1, 0xFC, 0x04, 0x0c, 0x07, 0x41, 0x00 };
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0,
					DEVICE_CLASS_REQUEST_OUT, 0x00, 0x00,
					buf, 0x07, CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			BT_ERR("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		int ret = 0;
		int actual_length;
		ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
					result_buf, 64, &actual_length, 2000);

		if (ret < 0) {
			BT_ERR("%s error2(%d)\n", __func__, ret);
			return ret;
		} else {
			if (result_buf[6] & 0x01)
				BT_INFO("warning, 0x41070c[0] is 1!\n");
		}
	}

	/* Send HCI Reset, write 0x41070c[0] to 1 */
	{
		int ret = 0;
		char buf[12] = { 0xd0, 0xfc, 0x08, 0x0c, 0x07, 0x41, 0x00 };
		buf[7] = result_buf[6] | 0x01;
		buf[8] = result_buf[7];
		buf[9] = result_buf[8];
		buf[10] = result_buf[9];
		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0,
					DEVICE_CLASS_REQUEST_OUT, 0x00, 0x00,
					buf, 0x0b, CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			BT_ERR("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	BT_INFO("btusb_send_hci_set_ce_cmd : OK\n");
	return 0;
}

static int btusb_send_check_rom_patch_result_cmd(struct usb_device *udev)
{
	/* Send HCI Reset */
	{
		int ret = 0;
		unsigned char buf[8] = { 0 };
		buf[0] = 0xD1;
		buf[1] = 0xFC;
		buf[2] = 0x04;
		buf[3] = 0x00;
		buf[4] = 0xE2;
		buf[5] = 0x40;
		buf[6] = 0x00;

		ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x0,
					DEVICE_CLASS_REQUEST_OUT, 0x00, 0x00,
					buf, 0x07, CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			BT_ERR("%s error1(%d)\n", __func__, ret);
			return ret;
		}
	}

	/* Get response of HCI reset */
	{
		int ret = 0;
		unsigned char buf[64] = { 0 };
		int actual_length;
		ret = usb_interrupt_msg(udev, usb_rcvintpipe(udev, 1),
					buf, 64, &actual_length, 2000);

		if (ret < 0) {
			BT_ERR("%s error2(%d)\n", __func__, ret);
			return ret;
		}
		BT_INFO("Check rom patch result : ");

		if (buf[6] == 0 && buf[7] == 0 && buf[8] == 0 && buf[9] == 0)
			BT_INFO("NG\n");
		else
			BT_INFO("OK\n");
	}

	return 0;
}

static int btusb_switch_iobase(struct btusb_data *data, int base)
{
	int ret = 0;

	switch (base) {
	case SYSCTL:
		data->w_request = 0x42;
		data->r_request = 0x47;
		break;
	case WLAN:
		data->w_request = 0x02;
		data->r_request = 0x07;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static void btusb_cap_init(struct btusb_data *data)
{
	btusb_io_read32(data, 0x00, &data->chip_id);

	BT_INFO("%s : chip id = %x\n", __func__, data->chip_id);

	BT_INFO("This is 7662T chip\n");
	data->need_load_rom_patch = 1;

	data->rom_patch_bin_file_name = kmalloc(32, GFP_ATOMIC);
	if (!data->rom_patch_bin_file_name) {
		BT_ERR("%s: Can't allocate memory (32)\n", __func__);
		return;
	}
	memset(data->rom_patch_bin_file_name, 0, 32);

	if ((data->chip_id & 0xf) < 0x2)
		memcpy(data->rom_patch_bin_file_name,
			"mt7662t_patch_e1_hdr.bin", 24);
	else
		memcpy(data->rom_patch_bin_file_name,
			"mt7662t_patch_e3_hdr.bin", 24);

	data->rom_patch_offset = 0xBC000;
	data->rom_patch_len = 0;
}

u16 checksume16(u8 *pdata, int len)
{
	int sum = 0;

	while (len > 1) {
		sum += *((u16 *) pdata);

		pdata = pdata + 2;

		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);

		len -= 2;
	}

	if (len)
		sum += *((u8 *) pdata);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

static int btusb_chk_crc(struct btusb_data *data, u32 checksum_len)
{
	int ret = 0;
	struct usb_device *udev = data->udev;

	BT_DBG("%s", __func__);

	memmove(data->io_buf, &data->rom_patch_offset, 4);
	memmove(&data->io_buf[4], &checksum_len, 4);

	ret = usb_control_msg(udev, usb_sndctrlpipe(udev, 0), 0x1,
				DEVICE_VENDOR_REQUEST_OUT, 0x20, 0x00,
				data->io_buf, 8, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		BT_ERR("%s error(%d)\n", __func__, ret);

	return ret;
}

static u16 btusb_get_crc(struct btusb_data *data)
{
	int ret = 0;
	struct usb_device *udev = data->udev;
	u16 crc, count = 0;

	BT_DBG("%s", __func__);

	while (1) {
		ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
					0x01, DEVICE_VENDOR_REQUEST_IN,
					0x21, 0x00, data->io_buf, 2,
					CONTROL_TIMEOUT_JIFFIES);

		if (ret < 0) {
			crc = 0xFFFF;
			BT_ERR("%s error(%d)\n", __func__, ret);
		}

		memmove(&crc, data->io_buf, 2);

		crc = le16_to_cpu(crc);

		if (crc != 0xFFFF)
			break;

		mdelay(100);

		if (count++ > 100) {
			BT_INFO("Query CRC over %d times\n", count);
			break;
		}
	}

	return crc;
}

static int btusb_reset_wmt(struct btusb_data *data)
{
	int ret = 0;

	/* reset command */
	u8 cmd[9] = { 0x6F, 0xFC, 0x05, 0x01, 0x07, 0x01, 0x00, 0x04 };

	memmove(data->io_buf, cmd, 8);

	ret = usb_control_msg(data->udev, usb_sndctrlpipe(data->udev, 0),
				0x01, DEVICE_CLASS_REQUEST_OUT, 0x30, 0x00,
				data->io_buf, 8, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0) {
		BT_ERR("%s:Err1(%d)\n", __func__, ret);
		return ret;
	}

	mdelay(20);

	ret = usb_control_msg(data->udev, usb_rcvctrlpipe(data->udev, 0),
				0x01, DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00,
				data->io_buf, 7, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		BT_ERR("%s Err2(%d)\n", __func__, ret);

	if (data->io_buf[0] == 0xe4 &&
	    data->io_buf[1] == 0x05 &&
	    data->io_buf[2] == 0x02 &&
	    data->io_buf[3] == 0x07 &&
	    data->io_buf[4] == 0x01 &&
	    data->io_buf[5] == 0x00 &&
	    data->io_buf[6] == 0x00) {
		BT_INFO("%s : OK\n", __func__);
	} else {
		BT_INFO("%s : NG\n", __func__);
	}

	return ret;
}

static u16 btusb_get_rom_patch_result(struct btusb_data *data)
{
	int ret = 0;

	ret = usb_control_msg(data->udev, usb_rcvctrlpipe(data->udev, 0),
				0x01, DEVICE_VENDOR_REQUEST_IN, 0x30, 0x00,
				data->io_buf, 7, CONTROL_TIMEOUT_JIFFIES);

	if (ret < 0)
		BT_ERR("%s error(%d)\n", __func__, ret);

	if (data->io_buf[0] == 0xe4 &&
	    data->io_buf[1] == 0x05 &&
	    data->io_buf[2] == 0x02 &&
	    data->io_buf[3] == 0x01 &&
	    data->io_buf[4] == 0x01 &&
	    data->io_buf[5] == 0x00 &&
	    data->io_buf[6] == 0x00) {
		BT_INFO("%s : OK\n", __func__);
	} else {
		BT_INFO("%s : NG\n", __func__);
	}

	return ret;
}

static void load_code_from_bin(unsigned char **image, char *bin_name,
				struct device *dev, u32 *code_len)
{
	const struct firmware *fw_entry;

	if (request_firmware(&fw_entry, bin_name, dev) != 0) {
		*image = NULL;
		return;
	}

	*image = vmalloc(fw_entry->size);
	memcpy(*image, fw_entry->data, fw_entry->size);
	*code_len = fw_entry->size;

	release_firmware(fw_entry);
}

static void load_rom_patch_complete(struct urb *urb)
{

	struct completion *sent_to_mcu_done = (struct completion *)urb->context;

	complete(sent_to_mcu_done);
}

static int btusb_load_rom_patch(struct btusb_data *data)
{
	u32 loop = 0;
	u32 value;
	s32 sent_len;
	int ret = 0, total_checksum = 0;
	struct urb *urb;
	u32 patch_len = 0;
	u32 cur_len = 0;
	dma_addr_t data_dma;
	struct completion sent_to_mcu_done;
	int first_block = 1;
	unsigned char phase;
	void *buf;
	char *pos;
	char *tmp_str;
	unsigned int pipe = usb_sndbulkpipe(data->udev,
					    data->bulk_tx_ep->bEndpointAddress);

	BT_INFO("btusb_load_rom_patch begin\n");
load_patch_protect:
	btusb_switch_iobase(data, WLAN);
	btusb_io_read32(data, SEMAPHORE_03, &value);
	loop++;

	if ((value & 0x01) == 0x00) {
		if (loop < 1000) {
			mdelay(1);
			goto load_patch_protect;
		} else {
			BT_ERR("btusb_load_rom_patch ERR!"
				"Can't get semaphore! Continue\n");
		}
	}

	btusb_switch_iobase(data, SYSCTL);

	btusb_io_write32(data, 0x1c, 0x30);

	btusb_switch_iobase(data, WLAN);

	/* check ROM patch if upgrade */
	btusb_io_read32(data, COM_REG0, &value);
	if ((value & 0x02) == 0x02) {
		BT_INFO("%s : no need to load rom patch\n", __func__);
		if (rom_patch_version[0] != 0)
			BT_INFO("%s : FW version = %s\n",
				__func__, rom_patch_version);

		btusb_send_hci_reset_cmd(data->udev);

		btusb_send_dummy_bulk_out_packet(data);
		btusb_send_dummy_bulk_out_packet(data);
		goto error0;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);

	if (!urb) {
		ret = -ENOMEM;
		goto error0;
	}

	buf = usb_alloc_coherent(data->udev, UPLOAD_PATCH_UNIT,
					GFP_ATOMIC, &data_dma);

	if (!buf) {
		ret = -ENOMEM;
		goto error1;
	}

	pos = buf;

	load_code_from_bin(&data->rom_patch, data->rom_patch_bin_file_name,
				&data->udev->dev, &data->rom_patch_len);

	if (!data->rom_patch) {
		BT_ERR("%s:please assign a rom patch(/etc/firmware/%s)"
			"or(/lib/firmware/%s)\n",
			__func__, data->rom_patch_bin_file_name,
			data->rom_patch_bin_file_name);

		ret = -1;
		goto error2;
	}

	tmp_str = data->rom_patch;
	BT_INFO("%s : FW Version = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
		__func__, tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3],
		tmp_str[4], tmp_str[5], tmp_str[6], tmp_str[7],
		tmp_str[8], tmp_str[9], tmp_str[10], tmp_str[11],
		tmp_str[12], tmp_str[13], tmp_str[14], tmp_str[15]);

	BT_INFO("%s : build Time = %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
		__func__, tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3],
		tmp_str[4], tmp_str[5], tmp_str[6], tmp_str[7],
		tmp_str[8], tmp_str[9], tmp_str[10], tmp_str[11],
		tmp_str[12], tmp_str[13], tmp_str[14], tmp_str[15]);

	memset(rom_patch_version, 0, sizeof(rom_patch_version));
	memcpy(rom_patch_version, tmp_str, 15);

	tmp_str = data->rom_patch + 16;
	BT_INFO("%s : platform = %c%c%c%c\n", __func__,
		tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	tmp_str = data->rom_patch + 20;
	BT_INFO("%s : HW/SW version = %c%c%c%c\n", __func__,
		tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	tmp_str = data->rom_patch + 24;
	//BT_INFO("%s : Patch version = %c%c%c%c\n", __func__,
	//	tmp_str[0], tmp_str[1], tmp_str[2], tmp_str[3]);

	BT_INFO("\nloading rom patch...\n");

	init_completion(&sent_to_mcu_done);

	cur_len = 0x00;
	patch_len = data->rom_patch_len - PATCH_INFO_SIZE;
	BT_INFO("%s : patch_len = %d\n", __func__, patch_len);

	BT_INFO("%s : loading rom patch...\n\n", __func__);
	/* loading rom patch */
	while (1) {
		s32 sent_len_max = UPLOAD_PATCH_UNIT - PATCH_HEADER_SIZE;
		sent_len = (patch_len - cur_len) >= sent_len_max ?
				sent_len_max : (patch_len - cur_len);

		BT_INFO("cur_len = %d\n", cur_len);
		BT_INFO("sent_len = %d\n", sent_len);

		if (sent_len > 0) {
			if (first_block == 1) {
				if (sent_len < sent_len_max)
					phase = PATCH_PHASE3;
				else
					phase = PATCH_PHASE1;
				first_block = 0;
			} else if (sent_len == sent_len_max) {
				phase = PATCH_PHASE2;
			} else {
				phase = PATCH_PHASE3;
			}

			/* prepare HCI header */
			pos[0] = 0x6F;
			pos[1] = 0xFC;
			pos[2] = (sent_len + 5) & 0xFF;
			pos[3] = ((sent_len + 5) >> 8) & 0xFF;

			/* prepare WMT header */
			pos[4] = 0x01;
			pos[5] = 0x01;
			pos[6] = (sent_len + 1) & 0xFF;
			pos[7] = ((sent_len + 1) >> 8) & 0xFF;

			pos[8] = phase;

			memcpy(&pos[9], data->rom_patch +
				PATCH_INFO_SIZE + cur_len, sent_len);

			BT_INFO("sent_len + PATCH_HEADER_SIZE = %d, "
				"phase = %d\n",
				sent_len + PATCH_HEADER_SIZE, phase);

			usb_fill_bulk_urb(urb, data->udev, pipe, buf,
						sent_len + PATCH_HEADER_SIZE,
						load_rom_patch_complete,
						&sent_to_mcu_done);

			urb->transfer_dma = data_dma;
			urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

			ret = usb_submit_urb(urb, GFP_ATOMIC);

			if (ret)
				goto error2;

			if (!wait_for_completion_timeout(&sent_to_mcu_done,
					msecs_to_jiffies(1000))) {
				usb_kill_urb(urb);
				BT_ERR("%s : upload rom_patch timeout\n",
					__func__);
				goto error2;
			}

			mdelay(1);

			cur_len += sent_len;

		} else {
			break;
		}
	}

	mdelay(20);
	ret = btusb_get_rom_patch_result(data);
	mdelay(20);

	/* Send Checksum request */
	total_checksum = checksume16(data->rom_patch + PATCH_INFO_SIZE,
					patch_len);
	btusb_chk_crc(data, patch_len);

	mdelay(20);

	if (total_checksum != btusb_get_crc(data)) {
		BT_ERR("checksum fail!, local(0x%x) <> fw(0x%x)\n",
			total_checksum, btusb_get_crc(data));
		ret = -1;
		goto error2;
	}

	mdelay(20);
	/* send check rom patch result request */
	btusb_send_check_rom_patch_result_cmd(data->udev);
	mdelay(20);
	/* CHIP_RESET */
	ret = btusb_reset_wmt(data);
	mdelay(20);
	/* BT_RESET */
	btusb_send_hci_reset_cmd(data->udev);

	/* for WoBLE/WoW low power */
	btusb_send_hci_set_ce_cmd(data->udev);
error2:
	usb_free_coherent(data->udev, UPLOAD_PATCH_UNIT, buf, data_dma);
error1:
	usb_free_urb(urb);
error0:
	btusb_io_write32(data, SEMAPHORE_03, 0x1);
	BT_INFO("btusb_load_rom_patch end\n");
	return ret;
}
#endif /* CONFIG_BT_MT7662TU_LOAD_ROM_PATCH */

static int inc_tx(struct btusb_data *data)
{
	unsigned long flags;
	int rv;

	spin_lock_irqsave(&data->txlock, flags);
	rv = test_bit(BTUSB_SUSPENDING, &data->flags);
	if (!rv)
		data->tx_in_flight++;
	spin_unlock_irqrestore(&data->txlock, flags);

	return rv;
}

static void btusb_intr_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s urb %p status %d count %d", hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

		if (hci_recv_fragment(hdev, HCI_EVENT_PKT,
						urb->transfer_buffer,
						urb->actual_length) < 0) {
			BT_ERR("%s corrupted event packet", hdev->name);
			hdev->stat.err_rx++;
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BTUSB_INTR_RUNNING, &data->flags))
		return;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btusb_submit_intr_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	BT_DBG("%s", hdev->name);

	if (!data->intr_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->intr_ep->wMaxPacketSize);

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvintpipe(data->udev, data->intr_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size,
						btusb_intr_complete, hdev,
						data->intr_ep->bInterval);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_anchor_urb(urb, &data->intr_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btusb_bulk_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s urb %p status %d count %d", hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		hdev->stat.byte_rx += urb->actual_length;

		if (hci_recv_fragment(hdev, HCI_ACLDATA_PKT,
						urb->transfer_buffer,
						urb->actual_length) < 0) {
			BT_ERR("%s corrupted ACL packet", hdev->name);
			hdev->stat.err_rx++;
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BTUSB_BULK_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->bulk_anchor);
	usb_mark_last_busy(data->udev);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static int btusb_submit_bulk_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size = HCI_MAX_FRAME_SIZE;

	BT_DBG("%s", hdev->name);

	if (!data->bulk_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(0, mem_flags);
	if (!urb)
		return -ENOMEM;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvbulkpipe(data->udev, data->bulk_rx_ep->bEndpointAddress);

	usb_fill_bulk_urb(urb, data->udev, pipe,
					buf, size, btusb_bulk_complete, hdev);

	urb->transfer_flags |= URB_FREE_BUFFER;

	usb_mark_last_busy(data->udev);
	usb_anchor_urb(urb, &data->bulk_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btusb_isoc_complete(struct urb *urb)
{
	struct hci_dev *hdev = urb->context;
	struct btusb_data *data = hci_get_drvdata(hdev);
	int i, err;

	BT_DBG("%s urb %p status %d count %d", hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return;

	if (urb->status == 0) {
		for (i = 0; i < urb->number_of_packets; i++) {
			unsigned int offset = urb->iso_frame_desc[i].offset;
			unsigned int length = urb->iso_frame_desc[i].actual_length;

			if (urb->iso_frame_desc[i].status)
				continue;

			hdev->stat.byte_rx += length;

			if (hci_recv_fragment(hdev, HCI_SCODATA_PKT,
						urb->transfer_buffer + offset,
								length) < 0) {
				BT_ERR("%s corrupted SCO packet", hdev->name);
				hdev->stat.err_rx++;
			}
		}
	} else if (urb->status == -ENOENT) {
		/* Avoid suspend failed when usb_kill_urb */
		return;
	}

	if (!test_bit(BTUSB_ISOC_RUNNING, &data->flags))
		return;

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		/* -EPERM: urb is being killed;
		 * -ENODEV: device got disconnected */
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p failed to resubmit (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}
}

static inline void __fill_isoc_descriptor(struct urb *urb, int len, int mtu)
{
	int i, offset = 0;

	BT_DBG("len %d mtu %d", len, mtu);

	for (i = 0; i < BTUSB_MAX_ISOC_FRAMES && len >= mtu;
					i++, offset += mtu, len -= mtu) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = mtu;
	}

	if (len && i < BTUSB_MAX_ISOC_FRAMES) {
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = len;
		i++;
	}

	urb->number_of_packets = i;
}

static int btusb_submit_isoc_urb(struct hci_dev *hdev, gfp_t mem_flags)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct urb *urb;
	unsigned char *buf;
	unsigned int pipe;
	int err, size;

	BT_DBG("%s", hdev->name);

	if (!data->isoc_rx_ep)
		return -ENODEV;

	urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, mem_flags);
	if (!urb)
		return -ENOMEM;

	size = le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize) *
						BTUSB_MAX_ISOC_FRAMES;

	buf = kmalloc(size, mem_flags);
	if (!buf) {
		usb_free_urb(urb);
		return -ENOMEM;
	}

	pipe = usb_rcvisocpipe(data->udev, data->isoc_rx_ep->bEndpointAddress);

	usb_fill_int_urb(urb, data->udev, pipe, buf, size, btusb_isoc_complete,
				hdev, data->isoc_rx_ep->bInterval);

	urb->transfer_flags  = URB_FREE_BUFFER | URB_ISO_ASAP;

	__fill_isoc_descriptor(urb, size,
			le16_to_cpu(data->isoc_rx_ep->wMaxPacketSize));

	usb_anchor_urb(urb, &data->isoc_anchor);

	err = usb_submit_urb(urb, mem_flags);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		usb_unanchor_urb(urb);
	}

	usb_free_urb(urb);

	return err;
}

static void btusb_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;
	struct btusb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s urb %p status %d count %d", hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

done:
	spin_lock(&data->txlock);
	data->tx_in_flight--;
	spin_unlock(&data->txlock);

	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static void btusb_isoc_tx_complete(struct urb *urb)
{
	struct sk_buff *skb = urb->context;
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;

	BT_DBG("%s urb %p status %d count %d", hdev->name,
					urb, urb->status, urb->actual_length);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (!urb->status)
		hdev->stat.byte_tx += urb->transfer_buffer_length;
	else
		hdev->stat.err_tx++;

done:
	kfree(urb->setup_packet);

	kfree_skb(skb);
}

static int btusb_open(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s", hdev->name);

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return err;

	data->intf->needs_remote_wakeup = 1;

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_and_set_bit(BTUSB_INTR_RUNNING, &data->flags))
		goto done;

	err = btusb_submit_intr_urb(hdev, GFP_KERNEL);
	if (err < 0)
		goto failed;

	err = btusb_submit_bulk_urb(hdev, GFP_KERNEL);
	if (err < 0) {
		usb_kill_anchored_urbs(&data->intr_anchor);
		goto failed;
	}

	set_bit(BTUSB_BULK_RUNNING, &data->flags);
	btusb_submit_bulk_urb(hdev, GFP_KERNEL);

done:
	usb_autopm_put_interface(data->intf);
	return 0;

failed:
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);
	clear_bit(HCI_RUNNING, &hdev->flags);
	usb_autopm_put_interface(data->intf);
	return err;
}

static void btusb_stop_traffic(struct btusb_data *data)
{
	usb_kill_anchored_urbs(&data->intr_anchor);
	usb_kill_anchored_urbs(&data->bulk_anchor);
	usb_kill_anchored_urbs(&data->isoc_anchor);
}

static int btusb_close(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	int err;

	BT_DBG("%s", hdev->name);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	cancel_work_sync(&data->work);
	cancel_work_sync(&data->waker);

	clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
	clear_bit(BTUSB_BULK_RUNNING, &data->flags);
	clear_bit(BTUSB_INTR_RUNNING, &data->flags);

	btusb_stop_traffic(data);
	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		goto failed;

	data->intf->needs_remote_wakeup = 0;
	usb_autopm_put_interface(data->intf);

failed:
	usb_scuttle_anchored_urbs(&data->deferred);
	return 0;
}

static int btusb_flush(struct hci_dev *hdev)
{
	struct btusb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s", hdev->name);

	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static int btusb_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	unsigned int pipe;
	int err;

	BT_DBG("%s", hdev->name);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		dr = kmalloc(sizeof(*dr), GFP_ATOMIC);
		if (!dr) {
			usb_free_urb(urb);
			return -ENOMEM;
		}

		dr->bRequestType = data->cmdreq_type;
		dr->bRequest     = 0;
		dr->wIndex       = 0;
		dr->wValue       = 0;
		dr->wLength      = __cpu_to_le16(skb->len);

		pipe = usb_sndctrlpipe(data->udev, 0x00);

		usb_fill_control_urb(urb, data->udev, pipe, (void *) dr,
				skb->data, skb->len, btusb_tx_complete, skb);

		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		if (!data->bulk_tx_ep)
			return -ENODEV;

		urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndbulkpipe(data->udev,
					data->bulk_tx_ep->bEndpointAddress);

		usb_fill_bulk_urb(urb, data->udev, pipe,
				skb->data, skb->len, btusb_tx_complete, skb);

		hdev->stat.acl_tx++;
		break;

	case HCI_SCODATA_PKT:
		if (!data->isoc_tx_ep || hdev->conn_hash.sco_num < 1)
			return -ENODEV;

		urb = usb_alloc_urb(BTUSB_MAX_ISOC_FRAMES, GFP_ATOMIC);
		if (!urb)
			return -ENOMEM;

		pipe = usb_sndisocpipe(data->udev,
					data->isoc_tx_ep->bEndpointAddress);

		usb_fill_int_urb(urb, data->udev, pipe,
				skb->data, skb->len, btusb_isoc_tx_complete,
				skb, data->isoc_tx_ep->bInterval);

		urb->transfer_flags  = URB_ISO_ASAP;

		__fill_isoc_descriptor(urb, skb->len,
				le16_to_cpu(data->isoc_tx_ep->wMaxPacketSize));

		hdev->stat.sco_tx++;
		goto skip_waking;

	default:
		return -EILSEQ;
	}

	err = inc_tx(data);
	if (err) {
		usb_anchor_urb(urb, &data->deferred);
		schedule_work(&data->waker);
		err = 0;
		goto done;
	}

skip_waking:
	usb_anchor_urb(urb, &data->tx_anchor);

	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err < 0) {
		if (err != -EPERM && err != -ENODEV)
			BT_ERR("%s urb %p submission failed (%d)",
						hdev->name, urb, -err);
		kfree(urb->setup_packet);
		usb_unanchor_urb(urb);
	} else {
		usb_mark_last_busy(data->udev);
	}

done:
	usb_free_urb(urb);
	return err;
}

static void btusb_notify(struct hci_dev *hdev, unsigned int evt)
{
	struct btusb_data *data = hci_get_drvdata(hdev);

	BT_DBG("%s evt %d", hdev->name, evt);

	if (hdev->conn_hash.sco_num != data->sco_num) {
		data->sco_num = hdev->conn_hash.sco_num;
		schedule_work(&data->work);
	}
}

static inline int __set_isoc_interface(struct hci_dev *hdev, int altsetting)
{
	struct btusb_data *data = hci_get_drvdata(hdev);
	struct usb_interface *intf = data->isoc;
	struct usb_endpoint_descriptor *ep_desc;
	int i, err;

	if (!data->isoc)
		return -ENODEV;

	err = usb_set_interface(data->udev, 1, altsetting);
	if (err < 0) {
		BT_ERR("%s setting interface failed (%d)", hdev->name, -err);
		return err;
	}

	data->isoc_altsetting = altsetting;

	data->isoc_tx_ep = NULL;
	data->isoc_rx_ep = NULL;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->isoc_tx_ep && usb_endpoint_is_isoc_out(ep_desc)) {
			data->isoc_tx_ep = ep_desc;
			continue;
		}

		if (!data->isoc_rx_ep && usb_endpoint_is_isoc_in(ep_desc)) {
			data->isoc_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->isoc_tx_ep || !data->isoc_rx_ep) {
		BT_ERR("%s invalid SCO descriptors", hdev->name);
		return -ENODEV;
	}

	return 0;
}

static void btusb_work(struct work_struct *work)
{
	struct btusb_data *data = container_of(work, struct btusb_data, work);
	struct hci_dev *hdev = data->hdev;
	int new_alts;
	int err;

	if (hdev->conn_hash.sco_num > 0) {
		if (!test_bit(BTUSB_DID_ISO_RESUME, &data->flags)) {
			err = usb_autopm_get_interface(data->isoc ? data->isoc : data->intf);
			if (err < 0) {
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
				usb_kill_anchored_urbs(&data->isoc_anchor);
				return;
			}

			set_bit(BTUSB_DID_ISO_RESUME, &data->flags);
		}

		if (hdev->voice_setting & 0x0020) {
			static const int alts[3] = { 2, 4, 5 };
			new_alts = alts[hdev->conn_hash.sco_num - 1];
		} else {
			new_alts = hdev->conn_hash.sco_num;
		}

		if (data->isoc_altsetting != new_alts) {
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			usb_kill_anchored_urbs(&data->isoc_anchor);

			if (__set_isoc_interface(hdev, new_alts) < 0)
				return;
		}

		if (!test_and_set_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
			if (btusb_submit_isoc_urb(hdev, GFP_KERNEL) < 0)
				clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
			else
				btusb_submit_isoc_urb(hdev, GFP_KERNEL);
		}
	} else {
		clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		usb_kill_anchored_urbs(&data->isoc_anchor);

		__set_isoc_interface(hdev, 0);
		if (test_and_clear_bit(BTUSB_DID_ISO_RESUME, &data->flags))
			usb_autopm_put_interface(data->isoc ? data->isoc : data->intf);
	}
}

static void btusb_waker(struct work_struct *work)
{
	struct btusb_data *data = container_of(work, struct btusb_data, waker);
	int err;

	err = usb_autopm_get_interface(data->intf);
	if (err < 0)
		return;

	usb_autopm_put_interface(data->intf);
}

static int btusb_setup_bcm92035(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	u8 val = 0x00;

	BT_DBG("%s", hdev->name);

	skb = __hci_cmd_sync(hdev, 0xfc3b, 1, &val, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb))
		BT_ERR("BCM92035 command failed (%ld)", -PTR_ERR(skb));
	else
		kfree_skb(skb);

	return 0;
}

struct intel_version {
	u8 status;
	u8 hw_platform;
	u8 hw_variant;
	u8 hw_revision;
	u8 fw_variant;
	u8 fw_revision;
	u8 fw_build_num;
	u8 fw_build_ww;
	u8 fw_build_yy;
	u8 fw_patch_num;
} __packed;

static const struct firmware *btusb_setup_intel_get_fw(struct hci_dev *hdev,
						struct intel_version *ver)
{
	const struct firmware *fw;
	char fwname[64];
	int ret;

	snprintf(fwname, sizeof(fwname),
		 "intel/ibt-hw-%x.%x.%x-fw-%x.%x.%x.%x.%x.bseq",
		 ver->hw_platform, ver->hw_variant, ver->hw_revision,
		 ver->fw_variant,  ver->fw_revision, ver->fw_build_num,
		 ver->fw_build_ww, ver->fw_build_yy);

	ret = request_firmware(&fw, fwname, &hdev->dev);
	if (ret < 0) {
		if (ret == -EINVAL) {
			BT_ERR("%s Intel firmware file request failed (%d)",
			       hdev->name, ret);
			return NULL;
		}

		BT_ERR("%s failed to open Intel firmware file: %s(%d)",
		       hdev->name, fwname, ret);

		/* If the correct firmware patch file is not found, use the
		 * default firmware patch file instead
		 */
		snprintf(fwname, sizeof(fwname), "intel/ibt-hw-%x.%x.bseq",
			 ver->hw_platform, ver->hw_variant);
		if (request_firmware(&fw, fwname, &hdev->dev) < 0) {
			BT_ERR("%s failed to open default Intel fw file: %s",
			       hdev->name, fwname);
			return NULL;
		}
	}

	BT_INFO("%s: Intel Bluetooth firmware file: %s", hdev->name, fwname);

	return fw;
}

static int btusb_setup_intel_patching(struct hci_dev *hdev,
				      const struct firmware *fw,
				      const u8 **fw_ptr, int *disable_patch)
{
	struct sk_buff *skb;
	struct hci_command_hdr *cmd;
	const u8 *cmd_param;
	struct hci_event_hdr *evt = NULL;
	const u8 *evt_param = NULL;
	int remain = fw->size - (*fw_ptr - fw->data);

	/* The first byte indicates the types of the patch command or event.
	 * 0x01 means HCI command and 0x02 is HCI event. If the first bytes
	 * in the current firmware buffer doesn't start with 0x01 or
	 * the size of remain buffer is smaller than HCI command header,
	 * the firmware file is corrupted and it should stop the patching
	 * process.
	 */
	if (remain > HCI_COMMAND_HDR_SIZE && *fw_ptr[0] != 0x01) {
		BT_ERR("%s Intel fw corrupted: invalid cmd read", hdev->name);
		return -EINVAL;
	}
	(*fw_ptr)++;
	remain--;

	cmd = (struct hci_command_hdr *)(*fw_ptr);
	*fw_ptr += sizeof(*cmd);
	remain -= sizeof(*cmd);

	/* Ensure that the remain firmware data is long enough than the length
	 * of command parameter. If not, the firmware file is corrupted.
	 */
	if (remain < cmd->plen) {
		BT_ERR("%s Intel fw corrupted: invalid cmd len", hdev->name);
		return -EFAULT;
	}

	/* If there is a command that loads a patch in the firmware
	 * file, then enable the patch upon success, otherwise just
	 * disable the manufacturer mode, for example patch activation
	 * is not required when the default firmware patch file is used
	 * because there are no patch data to load.
	 */
	if (*disable_patch && le16_to_cpu(cmd->opcode) == 0xfc8e)
		*disable_patch = 0;

	cmd_param = *fw_ptr;
	*fw_ptr += cmd->plen;
	remain -= cmd->plen;

	/* This reads the expected events when the above command is sent to the
	 * device. Some vendor commands expects more than one events, for
	 * example command status event followed by vendor specific event.
	 * For this case, it only keeps the last expected event. so the command
	 * can be sent with __hci_cmd_sync_ev() which returns the sk_buff of
	 * last expected event.
	 */
	while (remain > HCI_EVENT_HDR_SIZE && *fw_ptr[0] == 0x02) {
		(*fw_ptr)++;
		remain--;

		evt = (struct hci_event_hdr *)(*fw_ptr);
		*fw_ptr += sizeof(*evt);
		remain -= sizeof(*evt);

		if (remain < evt->plen) {
			BT_ERR("%s Intel fw corrupted: invalid evt len",
			       hdev->name);
			return -EFAULT;
		}

		evt_param = *fw_ptr;
		*fw_ptr += evt->plen;
		remain -= evt->plen;
	}

	/* Every HCI commands in the firmware file has its correspond event.
	 * If event is not found or remain is smaller than zero, the firmware
	 * file is corrupted.
	 */
	if (!evt || !evt_param || remain < 0) {
		BT_ERR("%s Intel fw corrupted: invalid evt read", hdev->name);
		return -EFAULT;
	}

	skb = __hci_cmd_sync_ev(hdev, le16_to_cpu(cmd->opcode), cmd->plen,
				cmd_param, evt->evt, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s sending Intel patch command (0x%4.4x) failed (%ld)",
		       hdev->name, cmd->opcode, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	/* It ensures that the returned event matches the event data read from
	 * the firmware file. At fist, it checks the length and then
	 * the contents of the event.
	 */
	if (skb->len != evt->plen) {
		BT_ERR("%s mismatch event length (opcode 0x%4.4x)", hdev->name,
		       le16_to_cpu(cmd->opcode));
		kfree_skb(skb);
		return -EFAULT;
	}

	if (memcmp(skb->data, evt_param, evt->plen)) {
		BT_ERR("%s mismatch event parameter (opcode 0x%4.4x)",
		       hdev->name, le16_to_cpu(cmd->opcode));
		kfree_skb(skb);
		return -EFAULT;
	}
	kfree_skb(skb);

	return 0;
}

static int btusb_setup_intel(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	const struct firmware *fw;
	const u8 *fw_ptr;
	int disable_patch;
	struct intel_version *ver;

	const u8 mfg_enable[] = { 0x01, 0x00 };
	const u8 mfg_disable[] = { 0x00, 0x00 };
	const u8 mfg_reset_deactivate[] = { 0x00, 0x01 };
	const u8 mfg_reset_activate[] = { 0x00, 0x02 };

	BT_DBG("%s", hdev->name);

	/* The controller has a bug with the first HCI command sent to it
	 * returning number of completed commands as zero. This would stall the
	 * command processing in the Bluetooth core.
	 *
	 * As a workaround, send HCI Reset command first which will reset the
	 * number of completed commands and allow normal command processing
	 * from now on.
	 */
	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s sending initial HCI reset command failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	/* Read Intel specific controller version first to allow selection of
	 * which firmware file to load.
	 *
	 * The returned information are hardware variant and revision plus
	 * firmware variant, revision and build number.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc05, 0, NULL, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s reading Intel fw version command failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}

	if (skb->len != sizeof(*ver)) {
		BT_ERR("%s Intel version event length mismatch", hdev->name);
		kfree_skb(skb);
		return -EIO;
	}

	ver = (struct intel_version *)skb->data;
	if (ver->status) {
		BT_ERR("%s Intel fw version event failed (%02x)", hdev->name,
		       ver->status);
		kfree_skb(skb);
		return -bt_to_errno(ver->status);
	}

	BT_INFO("%s: read Intel version: %02x%02x%02x%02x%02x%02x%02x%02x%02x",
		hdev->name, ver->hw_platform, ver->hw_variant,
		ver->hw_revision, ver->fw_variant,  ver->fw_revision,
		ver->fw_build_num, ver->fw_build_ww, ver->fw_build_yy,
		ver->fw_patch_num);

	/* fw_patch_num indicates the version of patch the device currently
	 * have. If there is no patch data in the device, it is always 0x00.
	 * So, if it is other than 0x00, no need to patch the deivce again.
	 */
	if (ver->fw_patch_num) {
		BT_INFO("%s: Intel device is already patched. patch num: %02x",
			hdev->name, ver->fw_patch_num);
		kfree_skb(skb);
		return 0;
	}

	/* Opens the firmware patch file based on the firmware version read
	 * from the controller. If it fails to open the matching firmware
	 * patch file, it tries to open the default firmware patch file.
	 * If no patch file is found, allow the device to operate without
	 * a patch.
	 */
	fw = btusb_setup_intel_get_fw(hdev, ver);
	if (!fw) {
		kfree_skb(skb);
		return 0;
	}
	fw_ptr = fw->data;

	kfree_skb(skb);

	/* This Intel specific command enables the manufacturer mode of the
	 * controller.
	 *
	 * Only while this mode is enabled, the driver can download the
	 * firmware patch data and configuration parameters.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc11, 2, mfg_enable, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s entering Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		release_firmware(fw);
		return PTR_ERR(skb);
	}

	if (skb->data[0]) {
		u8 evt_status = skb->data[0];
		BT_ERR("%s enable Intel manufacturer mode event failed (%02x)",
		       hdev->name, evt_status);
		kfree_skb(skb);
		release_firmware(fw);
		return -bt_to_errno(evt_status);
	}
	kfree_skb(skb);

	disable_patch = 1;

	/* The firmware data file consists of list of Intel specific HCI
	 * commands and its expected events. The first byte indicates the
	 * type of the message, either HCI command or HCI event.
	 *
	 * It reads the command and its expected event from the firmware file,
	 * and send to the controller. Once __hci_cmd_sync_ev() returns,
	 * the returned event is compared with the event read from the firmware
	 * file and it will continue until all the messages are downloaded to
	 * the controller.
	 *
	 * Once the firmware patching is completed successfully,
	 * the manufacturer mode is disabled with reset and activating the
	 * downloaded patch.
	 *
	 * If the firmware patching fails, the manufacturer mode is
	 * disabled with reset and deactivating the patch.
	 *
	 * If the default patch file is used, no reset is done when disabling
	 * the manufacturer.
	 */
	while (fw->size > fw_ptr - fw->data) {
		int ret;

		ret = btusb_setup_intel_patching(hdev, fw, &fw_ptr,
						 &disable_patch);
		if (ret < 0)
			goto exit_mfg_deactivate;
	}

	release_firmware(fw);

	if (disable_patch)
		goto exit_mfg_disable;

	/* Patching completed successfully and disable the manufacturer mode
	 * with reset and activate the downloaded firmware patches.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc11, sizeof(mfg_reset_activate),
			     mfg_reset_activate, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s exiting Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	BT_INFO("%s: Intel Bluetooth firmware patch completed and activated",
		hdev->name);

	return 0;

exit_mfg_disable:
	/* Disable the manufacturer mode without reset */
	skb = __hci_cmd_sync(hdev, 0xfc11, sizeof(mfg_disable), mfg_disable,
			     HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s exiting Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	BT_INFO("%s: Intel Bluetooth firmware patch completed", hdev->name);
	return 0;

exit_mfg_deactivate:
	release_firmware(fw);

	/* Patching failed. Disable the manufacturer mode with reset and
	 * deactivate the downloaded firmware patches.
	 */
	skb = __hci_cmd_sync(hdev, 0xfc11, sizeof(mfg_reset_deactivate),
			     mfg_reset_deactivate, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		BT_ERR("%s exiting Intel manufacturer mode failed (%ld)",
		       hdev->name, PTR_ERR(skb));
		return PTR_ERR(skb);
	}
	kfree_skb(skb);

	BT_INFO("%s: Intel Bluetooth firmware patch completed and deactivated",
		hdev->name);

	return 0;
}

static int btusb_probe(struct usb_interface *intf,
				const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *ep_desc;
	struct btusb_data *data;
	struct hci_dev *hdev;
	int i, err;

	BT_DBG("intf %p id %p", intf, id);

	/* interface numbers are hardcoded in the spec */
	if (intf->cur_altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;

	if (!id->driver_info) {
		const struct usb_device_id *match;
		match = usb_match_id(intf, blacklist_table);
		if (match)
			id = match;
	}

	if (id->driver_info == BTUSB_IGNORE)
		return -ENODEV;

	if (ignore_dga && id->driver_info & BTUSB_DIGIANSWER)
		return -ENODEV;

	if (ignore_csr && id->driver_info & BTUSB_CSR)
		return -ENODEV;

	if (ignore_sniffer && id->driver_info & BTUSB_SNIFFER)
		return -ENODEV;

	if (id->driver_info & BTUSB_ATH3012) {
		struct usb_device *udev = interface_to_usbdev(intf);

		/* Old firmware would otherwise let ath3k driver load
		 * patch and sysconfig files */
		if (le16_to_cpu(udev->descriptor.bcdDevice) <= 0x0001)
			return -ENODEV;
	}

	data = devm_kzalloc(&intf->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for (i = 0; i < intf->cur_altsetting->desc.bNumEndpoints; i++) {
		ep_desc = &intf->cur_altsetting->endpoint[i].desc;

		if (!data->intr_ep && usb_endpoint_is_int_in(ep_desc)) {
			data->intr_ep = ep_desc;
			continue;
		}

		if (!data->bulk_tx_ep && usb_endpoint_is_bulk_out(ep_desc)) {
			data->bulk_tx_ep = ep_desc;
			continue;
		}

		if (!data->bulk_rx_ep && usb_endpoint_is_bulk_in(ep_desc)) {
			data->bulk_rx_ep = ep_desc;
			continue;
		}
	}

	if (!data->intr_ep || !data->bulk_tx_ep || !data->bulk_rx_ep)
		return -ENODEV;

	data->cmdreq_type = USB_TYPE_CLASS;

	data->udev = interface_to_usbdev(intf);
	data->intf = intf;

	spin_lock_init(&data->lock);

	INIT_WORK(&data->work, btusb_work);
	INIT_WORK(&data->waker, btusb_waker);
	spin_lock_init(&data->txlock);

	init_usb_anchor(&data->tx_anchor);
	init_usb_anchor(&data->intr_anchor);
	init_usb_anchor(&data->bulk_anchor);
	init_usb_anchor(&data->isoc_anchor);
	init_usb_anchor(&data->deferred);

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
	if (id->driver_info == BTUSB_MT7662TU) {
		data->io_buf = kmalloc(256, GFP_ATOMIC);
		if (!data->io_buf)
			return -ENOMEM;

		btusb_switch_iobase(data, WLAN);

		btusb_cap_init(data);

		if (data->need_load_rom_patch) {
			err = btusb_load_rom_patch(data);

			if (err < 0) {
				kfree(data->io_buf);
				kfree(data->rom_patch_bin_file_name);
				vfree(data->rom_patch);
				BT_ERR("%s : end Error 4\n", __func__);
				return err;
			}
		}
	}
#endif

	hdev = hci_alloc_dev();
	if (!hdev)
		return -ENOMEM;

	hdev->bus = HCI_USB;
	hci_set_drvdata(hdev, data);

	data->hdev = hdev;

	SET_HCIDEV_DEV(hdev, &intf->dev);

	hdev->open   = btusb_open;
	hdev->close  = btusb_close;
	hdev->flush  = btusb_flush;
	hdev->send   = btusb_send_frame;
	hdev->notify = btusb_notify;

	if (id->driver_info & BTUSB_BCM92035)
		hdev->setup = btusb_setup_bcm92035;

	if (id->driver_info & BTUSB_INTEL)
		hdev->setup = btusb_setup_intel;

	if (id->driver_info & BTUSB_INTEL_BOOT)
		set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);

	/* Interface numbers are hardcoded in the specification */
	data->isoc = usb_ifnum_to_if(data->udev, 1);

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
	if (id->driver_info == BTUSB_MT7662TU)
		kfree(data->rom_patch_bin_file_name);
#endif

	if (!reset)
		set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);

	if (force_scofix || id->driver_info & BTUSB_WRONG_SCO_MTU) {
		if (!disable_scofix)
			set_bit(HCI_QUIRK_FIXUP_BUFFER_SIZE, &hdev->quirks);
	}

	if (id->driver_info & BTUSB_BROKEN_ISOC)
		data->isoc = NULL;

	if (id->driver_info & BTUSB_DIGIANSWER) {
		data->cmdreq_type = USB_TYPE_VENDOR;
		set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);
	}

	if (id->driver_info & BTUSB_CSR) {
		struct usb_device *udev = data->udev;

		/* Old firmware would otherwise execute USB reset */
		if (le16_to_cpu(udev->descriptor.bcdDevice) < 0x117)
			set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);
	}

	if (id->driver_info & BTUSB_SNIFFER) {
		struct usb_device *udev = data->udev;

		/* New sniffer firmware has crippled HCI interface */
		if (le16_to_cpu(udev->descriptor.bcdDevice) > 0x997)
			set_bit(HCI_QUIRK_RAW_DEVICE, &hdev->quirks);

		data->isoc = NULL;
	}

	if (data->isoc) {
		err = usb_driver_claim_interface(&btusb_driver,
							data->isoc, data);
		if (err < 0) {
			hci_free_dev(hdev);
			return err;
		}
	}

	err = hci_register_dev(hdev);
	if (err < 0) {
		hci_free_dev(hdev);
		return err;
	}

	usb_set_intfdata(intf, data);

	return 0;
}

static void btusb_disconnect(struct usb_interface *intf)
{
	struct btusb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev;
#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
	const struct usb_device_id *id;
#endif

	BT_DBG("intf %p", intf);

	if (!data)
		return;

	hdev = data->hdev;
	usb_set_intfdata(data->intf, NULL);

	if (data->isoc)
		usb_set_intfdata(data->isoc, NULL);

	hci_unregister_dev(hdev);

	if (intf == data->isoc)
		usb_driver_release_interface(&btusb_driver, data->intf);
	else if (data->isoc)
		usb_driver_release_interface(&btusb_driver, data->isoc);

	hci_free_dev(hdev);

#ifdef CONFIG_BT_MT7662TU_LOAD_ROM_PATCH
	id = usb_match_id(intf, blacklist_table);
	if (id && id->driver_info == BTUSB_MT7662TU) {
		kfree(data->io_buf);

		if (data->need_load_rom_patch)
			vfree(data->rom_patch);
	}
#endif
}

#ifdef CONFIG_PM
static int btusb_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct btusb_data *data = usb_get_intfdata(intf);

	BT_DBG("intf %p", intf);

	if (data->suspend_count++)
		return 0;

	spin_lock_irq(&data->txlock);
	if (!(PMSG_IS_AUTO(message) && data->tx_in_flight)) {
		set_bit(BTUSB_SUSPENDING, &data->flags);
		spin_unlock_irq(&data->txlock);
	} else {
		spin_unlock_irq(&data->txlock);
		data->suspend_count--;
		return -EBUSY;
	}

	cancel_work_sync(&data->work);

	btusb_stop_traffic(data);
	usb_kill_anchored_urbs(&data->tx_anchor);

	return 0;
}

static void play_deferred(struct btusb_data *data)
{
	struct urb *urb;
	int err;

	while ((urb = usb_get_from_anchor(&data->deferred))) {
		err = usb_submit_urb(urb, GFP_ATOMIC);
		if (err < 0)
			break;

		data->tx_in_flight++;
	}
	usb_scuttle_anchored_urbs(&data->deferred);
}

static int btusb_resume(struct usb_interface *intf)
{
	struct btusb_data *data = usb_get_intfdata(intf);
	struct hci_dev *hdev = data->hdev;
	int err = 0;

	BT_DBG("intf %p", intf);

	if (--data->suspend_count)
		return 0;

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		goto done;

	if (test_bit(BTUSB_INTR_RUNNING, &data->flags)) {
		err = btusb_submit_intr_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_INTR_RUNNING, &data->flags);
			goto failed;
		}
	}

	if (test_bit(BTUSB_BULK_RUNNING, &data->flags)) {
		err = btusb_submit_bulk_urb(hdev, GFP_NOIO);
		if (err < 0) {
			clear_bit(BTUSB_BULK_RUNNING, &data->flags);
			goto failed;
		}

		btusb_submit_bulk_urb(hdev, GFP_NOIO);
	}

	if (test_bit(BTUSB_ISOC_RUNNING, &data->flags)) {
		if (btusb_submit_isoc_urb(hdev, GFP_NOIO) < 0)
			clear_bit(BTUSB_ISOC_RUNNING, &data->flags);
		else
			btusb_submit_isoc_urb(hdev, GFP_NOIO);
	}

	spin_lock_irq(&data->txlock);
	play_deferred(data);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);
	schedule_work(&data->work);

	return 0;

failed:
	usb_scuttle_anchored_urbs(&data->deferred);
done:
	spin_lock_irq(&data->txlock);
	clear_bit(BTUSB_SUSPENDING, &data->flags);
	spin_unlock_irq(&data->txlock);

	return err;
}
#endif

static struct usb_driver btusb_driver = {
	.name		= "btusb",
	.probe		= btusb_probe,
	.disconnect	= btusb_disconnect,
#ifdef CONFIG_PM
	.suspend	= btusb_suspend,
	.resume		= btusb_resume,
#endif
	.id_table	= btusb_table,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(btusb_driver);

module_param(ignore_dga, bool, 0644);
MODULE_PARM_DESC(ignore_dga, "Ignore devices with id 08fd:0001");

module_param(ignore_csr, bool, 0644);
MODULE_PARM_DESC(ignore_csr, "Ignore devices with id 0a12:0001");

module_param(ignore_sniffer, bool, 0644);
MODULE_PARM_DESC(ignore_sniffer, "Ignore devices with id 0a12:0002");

module_param(disable_scofix, bool, 0644);
MODULE_PARM_DESC(disable_scofix, "Disable fixup of wrong SCO buffer size");

module_param(force_scofix, bool, 0644);
MODULE_PARM_DESC(force_scofix, "Force fixup of wrong SCO buffers size");

module_param(reset, bool, 0644);
MODULE_PARM_DESC(reset, "Send HCI reset command on initialization");

MODULE_AUTHOR("Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("Generic Bluetooth USB driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
