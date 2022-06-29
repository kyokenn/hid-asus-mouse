// SPDX-License-Identifier: GPL-2.0+
/*
 * HID driver for ASUS mice
 *
 * Copyright (c) 2022 Kyoken <kyoken@kyoken.ninja>
 */

#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/module.h>

/* #include "hid-ids.h" */
#include "hid-asus-mouse.h"


void asus_mouse_parse_events(u32 bitmask[], u8 *data, int size)
{
	int i, bit, code, offset;
	for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++)
		bitmask[i] = 0;

	switch(size) {
  case ASUS_MOUSE_ARRAY_EVENT_SIZE:
    /* build bitmask from array of active key codes */
#ifdef ASUS_MOUSE_DEBUG
		printk(KERN_INFO "ASUS MOUSE: DATA %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
#endif

		for (offset = 3; offset < size; offset++) {
			code = data[offset];
			if (!code)
				continue;
			i = ASUS_MOUSE_DATA_KEY_STATE_NUM - (code / ASUS_MOUSE_DATA_KEY_STATE_BITS) - 1;
			bitmask[i] |= 1 << (code % ASUS_MOUSE_DATA_KEY_STATE_BITS);
		}
    break;

  case ASUS_MOUSE_BITMASK_EVENT_SIZE:
    /* build bitmask from event data */
#ifdef ASUS_MOUSE_DEBUG
		printk(KERN_INFO "ASUS MOUSE: DATA %02X %02X %02X %02X %02X %02X %02X %02X",
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		printk(KERN_INFO "ASUS MOUSE: DATA %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15], data[16]);
#endif

		offset = size - 1;
		for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++) {
			bit = 0;
			if (i == 0)  /* first byte of 16-byte number is missing, so we skip it */
				bit = 8;
			for (; bit < ASUS_MOUSE_DATA_KEY_STATE_BITS; bit += 8) {
				bitmask[i] |= data[offset] << (ASUS_MOUSE_DATA_KEY_STATE_BITS - 8 - bit);
				offset--;
			}
		}
    break;

  default:
    break;
	}
}

void asus_mouse_send_events(u32 bitmask[], struct asus_mouse_data *drv_data)
{
	int i, bit, asus_code, key_code;
	u32 modified;

	/* get key codes and send key events */
	for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++) {
		modified = drv_data->key_state[ASUS_MOUSE_DATA_KEY_STATE_NUM - i - 1] ^
			bitmask[ASUS_MOUSE_DATA_KEY_STATE_NUM - i - 1];
		for (bit = 0; bit < ASUS_MOUSE_DATA_KEY_STATE_BITS; bit += 1) {
			if (!(modified & (1 << bit)))
				continue;

			asus_code = i * ASUS_MOUSE_DATA_KEY_STATE_BITS + bit;
			if (asus_code >= ASUS_MOUSE_MAPPING_SIZE) {
				continue;
			}

			key_code = asus_mouse_key_mapping[asus_code];

			if (bitmask[ASUS_MOUSE_DATA_KEY_STATE_NUM - i - 1] & (1 << bit)) {
#ifdef ASUS_MOUSE_DEBUG
				printk(KERN_INFO "ASUS MOUSE: PRES 0x%02X (%d) - 0x%02X (%d) '%c'",
				       asus_code, asus_code, key_code, key_code, key_code);
#endif
				input_event(drv_data->input, EV_KEY, key_code, 1);
			} else {
#ifdef ASUS_MOUSE_DEBUG
				printk(KERN_INFO "ASUS MOUSE: RELE 0x%02X (%d) - 0x%02X (%d) '%c'",
				       asus_code, asus_code, key_code, key_code, key_code);
#endif
				input_event(drv_data->input, EV_KEY, key_code, 0);
			}
			input_sync(drv_data->input);
		}
	}

	/* save current keys state for tracking released keys */
	for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++)
		drv_data->key_state[i] = bitmask[i];
}

int asus_mouse_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
#ifdef ASUS_MOUSE_DEBUG
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
#endif
	struct hid_input *next, *hidinput = NULL;
	struct asus_mouse_data *drv_data;
	int error, i;

	drv_data = devm_kzalloc(&hdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (drv_data == NULL) {
		hid_err(hdev, "can't alloc device descriptor\n");
		return -ENOMEM;
	}

	for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++)
		drv_data->key_state[i] = 0;
	drv_data->input = NULL;
	hid_set_drvdata(hdev, drv_data);

	error = hid_parse(hdev);
	if (error) {
		hid_err(hdev, "parse failed\n");
		return error;
	}

	error = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (error) {
		hid_err(hdev, "hw start failed\n");
		return error;
	}

#ifdef ASUS_MOUSE_DEBUG
	/* printk(KERN_INFO "ASUS MOUSE: MOUS %d", USB_INTERFACE_PROTOCOL_MOUSE); */
	/* printk(KERN_INFO "ASUS MOUSE: KEYB %d", USB_INTERFACE_PROTOCOL_KEYBOARD); */
	printk(KERN_INFO "ASUS MOUSE: PROB %d %X", iface->cur_altsetting->desc.bInterfaceProtocol, hdev->collection->usage);
#endif

	list_for_each_entry_safe(hidinput, next, &hdev->inputs, list) {
		if (hidinput->registered && hidinput->input != NULL) {
			drv_data->input = hidinput->input;
			break;
		}
	}

	return 0;
}

void asus_mouse_remove(struct hid_device *hdev)
{
	struct asus_mouse_data *drv_data = hid_get_drvdata(hdev);
	if (drv_data != NULL) {
		/* TODO: clean up? */
	}
	hid_hw_stop(hdev);
}

int asus_mouse_raw_event(struct hid_device *hdev, struct hid_report *report,
		u8 *data, int size)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	struct asus_mouse_data *drv_data = hid_get_drvdata(hdev);
	u32 bitmask[ASUS_MOUSE_DATA_KEY_STATE_NUM];

	if (drv_data == NULL)
		return 0;

	if (iface->cur_altsetting->desc.bInterfaceProtocol != 0)
		return 0;

	asus_mouse_parse_events(bitmask, data, size);

#ifdef ASUS_MOUSE_DEBUG
	printk(KERN_INFO "ASUS MOUSE: STAT %08X %08X %08X %08X",
		drv_data->key_state[0], drv_data->key_state[1], drv_data->key_state[2], drv_data->key_state[3]);
	printk(KERN_INFO "ASUS MOUSE: MASK %08X %08X %08X %08X",
		bitmask[0], bitmask[1], bitmask[2], bitmask[3]);
#endif

	asus_mouse_send_events(bitmask, drv_data);
	return 0;
}

/*
  _RF devices are wireless devices connected with RF receiver
  _USB devices are wireless devices connected with USB cable
*/
static const struct hid_device_id asus_mouse_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_SPATHA_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_SPATHA_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_EVOLVE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_BUZZARD) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2_ORIGIN) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2_ORIGIN_PINK) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_PUGIO) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_PUGIO2_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_PUGIO2_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_CARRY) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT2_ELECTRO_PUNK) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT2_WIRELESS_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT2_WIRELESS_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_CHAKRAM_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_CHAKRAM_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_KERIS_WIRELESS_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_KERIS_WIRELESS_USB) },
	{ }
};
MODULE_DEVICE_TABLE(hid, asus_mouse_devices);

static struct hid_driver asus_mouse_driver = {
	.name = "asus-mouse",
	.id_table = asus_mouse_devices,
	.probe = asus_mouse_probe,
	.remove = asus_mouse_remove,
	.raw_event = asus_mouse_raw_event,
};
module_hid_driver(asus_mouse_driver);

MODULE_AUTHOR("Kyoken <kyoken@kyoken.ninja>");
MODULE_DESCRIPTION("ASUS mouse");
MODULE_LICENSE("GPL");
