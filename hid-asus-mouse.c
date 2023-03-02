// SPDX-License-Identifier: GPL-2.0+
/*
 * HID driver for ASUS mice
 *
 * Copyright (c) 2022 Kyoken <kyoken@kyoken.ninja>
 */

#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/timer.h>

/* #include "hid-ids.h" */
#include "hid-asus-mouse.h"

static struct input_dev *asus_mouse_input;

static void input_repeat_key(struct timer_list *t)
{
  int ms;

  switch(asus_mouse_input->repeat_key) {
  case KEY_KP4:
    input_report_rel(asus_mouse_input, REL_HWHEEL_HI_RES, ASUS_MOUSE_KP_WHEEL_RES);
    break;
  case KEY_KP6:
    input_report_rel(asus_mouse_input, REL_HWHEEL_HI_RES, -ASUS_MOUSE_KP_WHEEL_RES);
    break;
  case KEY_KP2:
    input_report_rel(asus_mouse_input, REL_WHEEL_HI_RES, -ASUS_MOUSE_KP_WHEEL_RES);
    break;
  case KEY_KP8:
    input_report_rel(asus_mouse_input, REL_WHEEL_HI_RES, ASUS_MOUSE_KP_WHEEL_RES);
    break;
  default:
    break;
  }
  input_sync(asus_mouse_input);

  ms = asus_mouse_input->rep[REP_PERIOD];
  if (asus_mouse_input->repeat_key && ms)
    mod_timer(&asus_mouse_input->timer, jiffies + msecs_to_jiffies(ms));
}

static void asus_mouse_handle_mouse(struct asus_mouse_data *drv_data, u8 *data, int size)
{
  s8 wheel;
  s16 x, y;

  if (size != ASUS_MOUSE_MOUSE_EVENT_SIZE)
    return;

  x = data[1] | (u16)data[2] << 8;
  y = data[3] | (u16)data[4] << 8;
  wheel = data[5];

#ifdef ASUS_MOUSE_DEBUG
  printk(KERN_INFO "hid-asus-mouse: MOUSE %02X %02X %02X %02X %02X %02X",
         data[0], data[1], data[2], data[3], data[4], data[5]);
  printk(KERN_INFO "hid-asus-mouse: BTN=%02X X=%d Y=%d WHL=%d",
         data[0], x, y, wheel);
#endif

  input_report_key(drv_data->input, BTN_LEFT, (data[0] & (1 << 0)) != 0);
  input_report_key(drv_data->input, BTN_RIGHT, (data[0] & (1 << 1)) != 0);
  input_report_key(drv_data->input, BTN_MIDDLE, (data[0] & (1 << 2)) != 0);
  input_report_key(drv_data->input, BTN_FORWARD, (data[0] & (1 << 3)) != 0);
  input_report_key(drv_data->input, BTN_BACK, (data[0] & (1 << 4)) != 0);
  input_report_rel(drv_data->input, REL_X, x);
  input_report_rel(drv_data->input, REL_Y, y);
  input_report_rel(drv_data->input, REL_WHEEL_HI_RES, wheel * ASUS_MOUSE_MOUSE_WHEEL_RES);
  input_sync(drv_data->input);
}

static void asus_mouse_handle_keyboard(struct asus_mouse_data *drv_data, u8 *data, int size)
{
	int i, bit, code, asus_code, key_code, offset;
  u32 bitmask[ASUS_MOUSE_DATA_KEY_STATE_NUM];
  u32 modified;
	for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++)
		bitmask[i] = 0;

  /* build bitmask */

	switch(size) {
  case ASUS_MOUSE_KEYS_ARRAY_EVENT_SIZE:
  case ASUS_MOUSE_KEYS_ARRAY_EVENT2_SIZE:
    /* build bitmask from array of active key codes */
#ifdef ASUS_MOUSE_DEBUG
    if (size == 9)
      printk(KERN_INFO "hid-asus-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
    if (size == 8)
      printk(KERN_INFO "hid-asus-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
#endif

    offset = 3;
    if (size == ASUS_MOUSE_KEYS_ARRAY_EVENT2_SIZE)
      offset--;

		for (; offset < size; offset++) {
			code = data[offset];
			if (!code)
				continue;
			i = ASUS_MOUSE_DATA_KEY_STATE_NUM - (code / ASUS_MOUSE_DATA_KEY_STATE_BITS) - 1;
			bitmask[i] |= 1 << (code % ASUS_MOUSE_DATA_KEY_STATE_BITS);
		}
    break;

  case ASUS_MOUSE_KEYS_BITMASK_EVENT_SIZE:
    /* build bitmask from event data */
#ifdef ASUS_MOUSE_DEBUG
		printk(KERN_INFO "hid-asus-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X",
           data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		printk(KERN_INFO "hid-asus-mouse: KEYS %02X %02X %02X %02X %02X %02X %02X %02X %02X",
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

#ifdef ASUS_MOUSE_DEBUG
  printk(KERN_INFO "hid-asus-mouse: STAT %08X %08X %08X %08X",
         drv_data->key_state[0], drv_data->key_state[1], drv_data->key_state[2], drv_data->key_state[3]);
  printk(KERN_INFO "hid-asus-mouse: MASK %08X %08X %08X %08X",
         bitmask[0], bitmask[1], bitmask[2], bitmask[3]);
#endif

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
				printk(KERN_INFO "hid-asus-mouse: PRES ASUS=0x%02X (%d) LINUX=0x%02X (%d) '%c'",
				       asus_code, asus_code, key_code, key_code, key_code);
#endif
        switch(key_code) {
        case KEY_KP4:
        case KEY_KP6:
        case KEY_KP2:
        case KEY_KP8:
          asus_mouse_input->repeat_key = key_code;
          input_repeat_key(NULL);
          break;
        default:
          input_report_key(drv_data->input, key_code, 1);
          input_sync(drv_data->input);
          break;
        }
			} else {
#ifdef ASUS_MOUSE_DEBUG
				printk(KERN_INFO "hid-asus-mouse: RELE ASUS=0x%02X (%d) LINUX=0x%02X (%d) '%c'",
				       asus_code, asus_code, key_code, key_code, key_code);
#endif
        switch(key_code) {
        case KEY_KP4:
        case KEY_KP6:
        case KEY_KP2:
        case KEY_KP8:
          asus_mouse_input->repeat_key = 0;
          break;
        default:
          input_report_key(drv_data->input, key_code, 0);
          input_sync(drv_data->input);
          break;
        }
			}
		}
	}

	/* save current keys state for tracking released keys */

	for (i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++)
		drv_data->key_state[i] = bitmask[i];
}

static int asus_mouse_raw_event(struct hid_device *hdev,
                                struct hid_report *report, u8 *data, int size)
{
  struct usb_interface *iface;

	struct asus_mouse_data *drv_data = hid_get_drvdata(hdev);
	if (!drv_data)
		return 0;

	iface = to_usb_interface(hdev->dev.parent);

#ifdef ASUS_MOUSE_DEBUG
  printk(KERN_INFO "hid-asus-mouse: RAW FROM=%d SIZE=%d",
         iface->cur_altsetting->desc.bInterfaceProtocol, size);
#endif

  switch(iface->cur_altsetting->desc.bInterfaceProtocol) {
  case 0:  /* keyboard */
  case 1:  /* keyboard on some mice */
    asus_mouse_handle_keyboard(drv_data, data, size);
    break;
  case 2:  /* mouse */
    asus_mouse_handle_mouse(drv_data, data, size);
    break;
  default:
    break;
  }

	return 0;
}

static int asus_mouse_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct asus_mouse_data *drv_data;
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "%s:parse of hid interface failed\n", __func__);
		return ret;
	}

	drv_data = devm_kzalloc(&hdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data) {
    hid_err(hdev, "%s: failed with error %d\n", __func__, ret);
		return -ENOMEM;
	}
  drv_data->input = asus_mouse_input;
	for (int i = 0; i < ASUS_MOUSE_DATA_KEY_STATE_NUM; i++)
		drv_data->key_state[i] = 0;
	hid_set_drvdata(hdev, drv_data);

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret) {
    hid_err(hdev, "%s: failed with error %d\n", __func__, ret);
    return ret;
  }

	ret = hid_hw_open(hdev);
	if (ret) {
    hid_err(hdev, "%s: failed with error %d\n", __func__, ret);
    return ret;
  }

	return 0;
}

static void asus_mouse_remove(struct hid_device *hdev)
{
	struct asus_mouse_data *drv_data = hid_get_drvdata(hdev);
	if (!drv_data) {
    hid_hw_stop(hdev);
    return;
  }

	hid_hw_stop(hdev);
  drv_data->input = NULL;
}

/*
  _RF devices are wireless devices connected with RF receiver
  _USB devices are wireless devices connected with USB cable
*/
static const struct hid_device_id asus_mouse_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_BUZZARD) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_CHAKRAM_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_CHAKRAM_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_CHAKRAM_X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2_CORE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2_ORIGIN) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS2_ORIGIN_PINK) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS3) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS3_WIRELESS) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS3_WIRELESS_AIMPOINT_RF) },
  { HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_GLADIUS3_WIRELESS_AIMPOINT_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_KERIS_WIRELESS_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_KERIS_WIRELESS_USB) },
  { HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_KERIS_WIRELESS_AIMPOINT_RF) },
  { HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_KERIS_WIRELESS_AIMPOINT_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_PUGIO) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_PUGIO2_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_PUGIO2_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_SPATHA_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_SPATHA_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_SPATHA_X_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_SPATHA_X_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_CARRY) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT2_ELECTRO_PUNK) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT2_WIRELESS_RF) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_ROG_STRIX_IMPACT2_WIRELESS_USB) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_ASUSTEK, USB_DEVICE_ID_ASUSTEK_TUF_GAMING_M3) },
	{ }
};
MODULE_DEVICE_TABLE(hid, asus_mouse_devices);

static struct hid_driver asus_mouse_driver = {
	.name = "hid-asus-mouse",
	.id_table = asus_mouse_devices,
	.probe = asus_mouse_probe,
	.remove = asus_mouse_remove,
	.raw_event = asus_mouse_raw_event,
};

static int __init asus_mouse_init(void)
{
  int ret;
	pr_info("hid-asus-mouse: loading ASUS mouse driver\n");

  asus_mouse_input = input_allocate_device();
  if (!asus_mouse_input) {
    return -ENOMEM;
  }

  asus_mouse_input->name = "ASUS mouse input";
  asus_mouse_input->phys = "hid-asus-mouse";

  /*
    KEYBOARD
    EV=100013
    KEY=1000300000007 ff800078000007ff febeffdff3cfffff fffffffffffffffe
    MSC=10
  */

  /*
    MOUSE
    EV=17
    KEY=7f0000 0 0 0 0
    REL=903
    MSC=10
  */

  /*
    TOUCHPAD
    PROP=5
    EV=1b
    KEY=e520 10000 0 0 0 0
    ABS=2e0800000000003
    MSC=20
  */

  asus_mouse_input->id.bustype = BUS_VIRTUAL;
  asus_mouse_input->id.product = 0x0000;
  asus_mouse_input->id.vendor = 0x0000;
  asus_mouse_input->id.version = 0x0000;

  /* asus_mouse_input->evbit[0] = 0x100013 | 0x17; */
  /* asus_mouse_input->keybit[3] = 0x1000300000007 | 0x7f0000 | 0xe520; */
  /* asus_mouse_input->keybit[2] = 0xff800078000007ff | 0x10000; */
  /* asus_mouse_input->keybit[1] = 0xfebeffdff3cfffff; */
  /* asus_mouse_input->keybit[0] = 0xfffffffffffffffe; */
  /* asus_mouse_input->relbit[0] = 0x903; */
  /* asus_mouse_input->absbit[0] = 0x2e0800000000003; */
  /* asus_mouse_input->mscbit[0] = 0x10 | 0x20; */

  asus_mouse_input->keybit[3] = 0x1000300000007;
  asus_mouse_input->keybit[2] = 0xff800078000007ff;
  asus_mouse_input->keybit[1] = 0xfebeffdff3cfffff;
  asus_mouse_input->keybit[0] = 0xfffffffffffffffe;

  set_bit(EV_REL, asus_mouse_input->evbit);
  set_bit(EV_KEY, asus_mouse_input->evbit);

  set_bit(REL_X, asus_mouse_input->relbit);
  set_bit(REL_Y, asus_mouse_input->relbit);
  set_bit(REL_WHEEL, asus_mouse_input->relbit);
  set_bit(REL_WHEEL_HI_RES, asus_mouse_input->relbit);
  set_bit(REL_HWHEEL, asus_mouse_input->relbit);
  set_bit(REL_HWHEEL_HI_RES, asus_mouse_input->relbit);

  /* set_bit(BTN_0, asus_mouse_input->keybit); */
  /* set_bit(BTN_1, asus_mouse_input->keybit); */
  /* set_bit(BTN_2, asus_mouse_input->keybit); */
  /* set_bit(BTN_3, asus_mouse_input->keybit); */
  /* set_bit(BTN_4, asus_mouse_input->keybit); */
  /* set_bit(BTN_5, asus_mouse_input->keybit); */
  /* set_bit(BTN_6, asus_mouse_input->keybit); */
  /* set_bit(BTN_7, asus_mouse_input->keybit); */
  /* set_bit(BTN_8, asus_mouse_input->keybit); */
  /* set_bit(BTN_9, asus_mouse_input->keybit); */
  set_bit(BTN_LEFT, asus_mouse_input->keybit);
  set_bit(BTN_RIGHT, asus_mouse_input->keybit);
  set_bit(BTN_MIDDLE, asus_mouse_input->keybit);
  set_bit(BTN_BACK, asus_mouse_input->keybit);
  set_bit(BTN_FORWARD, asus_mouse_input->keybit);
  set_bit(BTN_TOOL_DOUBLETAP, asus_mouse_input->keybit);
  set_bit(KEY_BACK, asus_mouse_input->keybit);
  set_bit(KEY_FORWARD, asus_mouse_input->keybit);

	asus_mouse_input->rep[REP_PERIOD] = 16;
  timer_setup(&asus_mouse_input->timer, input_repeat_key, 0);

	ret = input_register_device(asus_mouse_input);
	if (ret) {
    input_free_device(asus_mouse_input);
    return ret;
  }

	return hid_register_driver(&asus_mouse_driver);
}

static void __exit asus_mouse_exit(void)
{
	pr_info("hid-asus-mouse: unloading ASUS mouse driver\n");

  asus_mouse_input->repeat_key = 0;
  del_timer(&asus_mouse_input->timer);

  hid_unregister_driver(&asus_mouse_driver);
  input_unregister_device(asus_mouse_input);
}

module_init(asus_mouse_init);
module_exit(asus_mouse_exit);

MODULE_AUTHOR("Kyoken <kyoken@kyoken.ninja>");
MODULE_DESCRIPTION("ASUS Mouse");
MODULE_LICENSE("GPL");
