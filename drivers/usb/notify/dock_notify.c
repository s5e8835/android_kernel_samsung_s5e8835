// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015-2023 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

 /* usb notify layer v4.0 */

#define pr_fmt(fmt) "usb_notify: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/usb.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include <linux/usb_notify.h>
#include <linux/usb/hcd.h>

#define SMARTDOCK_INDEX	1
#define MMDOCK_INDEX	2
#define ROOTHUB_MAX_INDEX 10

struct dev_table {
	struct usb_device_id dev;
	int index;
};

struct roothub_vid_pid {
	u16 vid;
	u16 pid;
};

static struct dev_table enable_notify_hub_table[] = {
	{ .dev = { USB_DEVICE(0x0424, 0x2514), },
	   .index = SMARTDOCK_INDEX,
	}, /* SMART DOCK HUB 1 */
	{ .dev = { USB_DEVICE(0x1a40, 0x0101), },
	   .index = SMARTDOCK_INDEX,
	}, /* SMART DOCK HUB 2 */
	{ .dev = { USB_DEVICE(0x0424, 0x9512), },
	   .index = MMDOCK_INDEX,
	}, /* SMSC USB LAN HUB 9512 */
	{}
};

static struct dev_table essential_device_table[] = {
	{ .dev = { USB_DEVICE(0x08bb, 0x2704), },
	   .index = SMARTDOCK_INDEX,
	}, /* TI USB Audio DAC 1 */
	{ .dev = { USB_DEVICE(0x08bb, 0x27c4), },
	   .index = SMARTDOCK_INDEX,
	}, /* TI USB Audio DAC 2 */
	{ .dev = { USB_DEVICE(0x0424, 0xec00), },
	   .index = MMDOCK_INDEX,
	}, /* SMSC LAN Driver */
	{}
};

static struct dev_table unsupport_device_table[] = {
	{ .dev = { USB_DEVICE(0x1a0a, 0x0201), },
	}, /* The device for usb certification */
	{}
};

#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
static struct roothub_vid_pid root_hub_reserved[ROOTHUB_MAX_INDEX];

static void save_roothub_vid_pid(struct usb_device *dev)
{
	u16 vid = le16_to_cpu(dev->descriptor.idVendor);
	u16 pid = le16_to_cpu(dev->descriptor.idProduct);
	int i;

	for (i = 0; i < ROOTHUB_MAX_INDEX; i++) {
		if (root_hub_reserved[i].vid == vid && root_hub_reserved[i].pid == pid)
			break;
		if (root_hub_reserved[i].vid == 0 && root_hub_reserved[i].pid == 0) {
			root_hub_reserved[i].vid = vid;
			root_hub_reserved[i].pid = pid;
			break;
		}
	}
}

static bool match_roothub_vid_pid(struct usb_device *dev)
{
	u16 vid = le16_to_cpu(dev->descriptor.idVendor);
	u16 pid = le16_to_cpu(dev->descriptor.idProduct);
	bool ret = false;
	int i;

	for (i = 0; i < ROOTHUB_MAX_INDEX; i++) {
		if (root_hub_reserved[i].vid == 0 && root_hub_reserved[i].pid == 0)
			break;

		if (root_hub_reserved[i].vid == vid && root_hub_reserved[i].pid == pid) {
			ret = true;
			break;
		}
	}

	return ret;
}
#endif

static int check_essential_device(struct usb_device *dev, int index)
{
	struct dev_table *id;
	int ret = 0;

	/* check VID, PID */
	for (id = essential_device_table; id->dev.match_flags; id++) {
		if ((id->dev.match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		(id->dev.match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		id->dev.idVendor == le16_to_cpu(dev->descriptor.idVendor) &&
		id->dev.idProduct == le16_to_cpu(dev->descriptor.idProduct) &&
		id->index == index) {
			ret = 1;
			break;
		}
	}
	return ret;
}

static int check_gamepad_device(struct usb_device *dev)
{
	int ret = 0;

	unl_info("%s : product=%s\n", __func__, dev->product);

	if (!dev->product)
		return ret;

	if (!strncmp(dev->product, "Gamepad for SAMSUNG", 19))
		ret = 1;

	return ret;
}

static int check_lanhub_device(struct usb_device *dev)
{
	int ret = 0;

	unl_info("%s : product=%s\n", __func__, dev->product);

	if (!dev->product)
		return ret;

	if (!strncmp(dev->product, "LAN9512", 8))
		ret = 1;

	return ret;
}

static int is_notify_hub(struct usb_device *dev)
{
	struct dev_table *id;
	struct usb_device *hdev;
	int ret = 0;

	hdev = dev->parent;
	if (!hdev)
		goto skip;
	/* check VID, PID */
	for (id = enable_notify_hub_table; id->dev.match_flags; id++) {
		if ((id->dev.match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		(id->dev.match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		id->dev.idVendor == le16_to_cpu(hdev->descriptor.idVendor) &&
		id->dev.idProduct == le16_to_cpu(hdev->descriptor.idProduct)) {
			ret = (hdev->parent &&
			(hdev->parent == dev->bus->root_hub)) ? id->index : 0;
			break;
		}
	}
skip:
	return ret;
}

static int call_battery_notify(struct usb_device *dev, bool on)
{
	struct usb_device *hdev;
	struct usb_device *udev;
	struct otg_notify *o_notify = get_otg_notify();
	int index = 0;
	int count = 0;
	int port;

	index = is_notify_hub(dev);
	if (!index)
		goto skip;
	if (!check_essential_device(dev, index))
		goto skip;

	hdev = dev->parent;
	if (!hdev)
		goto skip;

	usb_hub_for_each_child(hdev, port, udev) {
		if (check_essential_device(udev, index)) {
			if (!on && (udev == dev))
				continue;
			else
				count++;
		}
	}

	unl_info("%s : VID : 0x%x, PID : 0x%x, on=%d, count=%d\n", __func__,
		dev->descriptor.idVendor, dev->descriptor.idProduct,
			on, count);
	if (on) {
		if (count == 1) {
			if (index == SMARTDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_SMTD_EXT_CURRENT, 1);
			else if (index == MMDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_MMD_EXT_CURRENT, 1);
		}
	} else {
		if (!count) {
			if (index == SMARTDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_SMTD_EXT_CURRENT, 0);
			else if (index == MMDOCK_INDEX)
				send_otg_notify(o_notify,
					NOTIFY_EVENT_MMD_EXT_CURRENT, 0);
		}
	}
skip:
	return 0;
}

static void seek_usb_interface(struct usb_device *dev)
{
	struct usb_interface *intf;
	int i;

	if (!dev) {
		unl_err("%s no dev\n", __func__);
		goto done;
	}

	if (!dev->actconfig) {
		unl_info("%s no set config\n", __func__);
		goto done;
	}

	for (i = 0; i < dev->actconfig->desc.bNumInterfaces; i++) {
		intf = dev->actconfig->interface[i];
		/* You can use this function for various purposes */
		store_usblog_notify(NOTIFY_PORT_CLASS,
			(void *)&dev->descriptor.bDeviceClass,
			(void *)&intf->cur_altsetting->desc.bInterfaceClass);
	}
done:
	return;
}

static void disconnect_usb_driver(struct usb_device *dev)
{
	struct usb_interface *intf = NULL;
	struct usb_driver *driver = NULL;
	int i;

	if (!dev) {
		unl_err("%s no dev\n", __func__);
		goto done;
	}

	if (!dev->actconfig) {
		unl_err("%s no set config\n", __func__);
		goto done;
	}

	for (i = 0; i < dev->actconfig->desc.bNumInterfaces; i++) {
		intf = dev->actconfig->interface[i];
		if (intf->dev.driver) {
			driver = to_usb_driver(intf->dev.driver);
			usb_driver_release_interface(driver, intf);
		} 
	}
done:
	return;
}

#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
static void connect_usb_driver(struct usb_device *dev)
{
	struct usb_interface *intf = NULL;
	int i, ret = 0;

	if (!dev) {
		unl_err("%s no dev\n", __func__);
		goto done;
	}

	if (!dev->actconfig) {
		unl_err("%s no set config\n", __func__);
		goto done;
	}

	for (i = 0; i < dev->actconfig->desc.bNumInterfaces; i++) {
		intf = dev->actconfig->interface[i];
		intf->authorized = 1;
	}

	for (i = 0; i < dev->actconfig->desc.bNumInterfaces; i++) {
		intf = dev->actconfig->interface[i];
		if (!intf->dev.driver) {
			ret = device_attach(&intf->dev);
			if (ret < 0)
				unl_err("%s attach intf->dev. error ret(%d)\n", __func__, ret);
			else
				unl_info("%s attach intf->dev\n", __func__);
		}
	}
done:
	return;
}

static void intf_authorized_clear(struct usb_device *dev)
{
	struct usb_hcd *hcd = bus_to_hcd(dev->bus);

	unl_info("%s\n", __func__);
	if (hcd)
		clear_bit(HCD_FLAG_INTF_AUTHORIZED, &hcd->flags);
}
#endif

static int call_device_notify(struct usb_device *dev, int connect)
{
	struct otg_notify *o_notify = get_otg_notify();
#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
	int ret = 0;
#endif

	if (dev->bus->root_hub != dev) {
		if (connect) {
			unl_info("%s device\n", __func__);
			send_otg_notify(o_notify,
				NOTIFY_EVENT_DEVICE_CONNECT, 1);

			if (check_gamepad_device(dev))
				send_otg_notify(o_notify,
					NOTIFY_EVENT_GAMEPAD_CONNECT, 1);
			else if (check_lanhub_device(dev))
				send_otg_notify(o_notify,
					NOTIFY_EVENT_LANHUB_CONNECT, 1);
			else
				;
			store_usblog_notify(NOTIFY_PORT_CONNECT,
				(void *)&dev->descriptor.idVendor,
				(void *)&dev->descriptor.idProduct);

			seek_usb_interface(dev);

			if (!usb_check_whitelist_for_mdm(dev)) {
				unl_info("This device will be disabled.\n");
				disconnect_usb_driver(dev);
				usb_set_device_state(dev, USB_STATE_NOTATTACHED);
				goto done;
			}

			switch (usb_check_whitelist_enable_state()) {
			case NOTIFY_MDM_NONE:
				/* whitelist_for_serial OFF, whitelist_for_id OFF */
				break;
			case NOTIFY_MDM_SERIAL:
				/* whitelist_for_serial ON, whitelist_for_id OFF */
				if (!usb_check_whitelist_for_serial(dev)) {
					unl_info("This device will be disabled.\n");
					disconnect_usb_driver(dev);
					usb_set_device_state(dev, USB_STATE_NOTATTACHED);
				}
				break;
			case NOTIFY_MDM_ID:
				/* whitelist_for_serial OFF, whitelist_for_id ON */
				if (!usb_check_whitelist_for_id(dev)) {
					unl_info("This device will be disabled.\n");
					disconnect_usb_driver(dev);
					usb_set_device_state(dev, USB_STATE_NOTATTACHED);
				}
				break;
			case NOTIFY_MDM_ID_AND_SERIAL:
				/* whitelist_for_serial ON, whitelist_for_id ON */
				if (!usb_check_whitelist_for_id(dev) &&
						!usb_check_whitelist_for_serial(dev)) {
					unl_info("This device will be disabled.\n");
					disconnect_usb_driver(dev);
					usb_set_device_state(dev, USB_STATE_NOTATTACHED);
				}
				break;
			default:
				break;
			}
#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
			ret = usb_check_allowlist_for_lockscreen_enabled_id(dev);
			if (ret == USB_NOTIFY_NOLIST) {
				unl_info("This device will be disabled.\n");
				disconnect_usb_driver(dev);
				usb_set_device_state(dev, USB_STATE_NOTATTACHED);
				dev->authorized = 0;
			} else if (ret == USB_NOTIFY_ALLOWLOST) {
				if (!match_roothub_vid_pid(dev)) {
					connect_usb_driver(dev);
				} else {
					unl_info("error. this device has same vid,pid with root hub.\n");
					disconnect_usb_driver(dev);
					usb_set_device_state(dev, USB_STATE_NOTATTACHED);
				}
			} else if (ret == USB_NOTIFY_NORESTRICT) {
				connect_usb_driver(dev);
			}
#endif
		} else {
			send_otg_notify(o_notify,
				NOTIFY_EVENT_DEVICE_CONNECT, 0);
			store_usblog_notify(NOTIFY_PORT_DISCONNECT,
				(void *)&dev->descriptor.idVendor,
				(void *)&dev->descriptor.idProduct);
#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
			if (!dev->authorized)
				disconnect_unauthorized_device(dev);
#endif
		}
	} else {
		if (connect) {
			unl_info("%s root hub\n", __func__);
#ifndef CONFIG_DISABLE_LOCKSCREEN_USB_RESTRICTION
			if (check_usb_restrict_lock_state(o_notify))
				intf_authorized_clear(dev);
			save_roothub_vid_pid(dev);
#endif
		}
	}
done:
	return 0;
}

static void check_roothub_device(struct usb_device *dev, bool on)
{
	struct otg_notify *o_notify = get_otg_notify();
	struct usb_device *hdev;
	struct usb_device *udev;
	int port = 0;
	int speed = USB_SPEED_UNKNOWN;
	int pr_speed = USB_SPEED_UNKNOWN;
	static int hs_hub;
	static int ss_hub;
	int con_hub = 0;

	if (!o_notify) {
		unl_err("%s otg_notify is null\n", __func__);
		return;
	}

	pr_speed = get_con_dev_max_speed(o_notify);

	hdev = dev->parent;
	if (!hdev)
		return;

	hdev = dev->bus->root_hub;
	if (!hdev)
		return;

	usb_hub_for_each_child(hdev, port, udev) {
		if (!on && (udev == dev))
			continue;
#if defined(CONFIG_USB_HW_PARAM)
		if (is_known_usbaudio(udev))
			inc_hw_param(o_notify, USB_HOST_CLASS_AUDIO_SAMSUNG_COUNT);
#endif
		if (is_usbhub(udev))
			con_hub = 1;

		if (udev->speed > speed)
			speed = udev->speed;
	}

	if (hdev->speed >= USB_SPEED_SUPER) {
		if (speed > USB_SPEED_UNKNOWN)
			ss_hub = 1;
		else
			ss_hub = 0;
	} else if (hdev->speed > USB_SPEED_UNKNOWN
			&& hdev->speed != USB_SPEED_WIRELESS) {
		if (speed > USB_SPEED_UNKNOWN)
			hs_hub = 1;
		else
			hs_hub = 0;
	} else
		;

	if (ss_hub || hs_hub) {
		if (speed > pr_speed)
			set_con_dev_max_speed(o_notify, speed);
	} else
		set_con_dev_max_speed(o_notify, USB_SPEED_UNKNOWN);

	unl_info("%s : dev->speed %s %s\n", __func__,
		usb_speed_string(dev->speed), on ? "on" : "off");

	unl_info("%s : o_notify->speed %s\n", __func__,
		usb_speed_string(get_con_dev_max_speed(o_notify)));

	set_con_dev_hub(o_notify, speed, con_hub);
}

#if defined(CONFIG_USB_HW_PARAM)
static int set_hw_param(struct usb_device *dev)
{
	struct otg_notify *o_notify = get_otg_notify();
	int ret = 0;
	int bInterfaceClass = 0, speed = 0;

	if (o_notify == NULL || dev->config->interface[0] == NULL) {
		ret =  -EFAULT;
		goto err;
	}

	if (dev->bus->root_hub != dev) {
		bInterfaceClass
			= dev->config->interface[0]
				->cur_altsetting->desc.bInterfaceClass;
		speed = dev->speed;

		unl_info("%s USB device connected - Class : 0x%x, speed : 0x%x\n",
			__func__, bInterfaceClass, speed);

		if (bInterfaceClass == USB_CLASS_AUDIO)
			inc_hw_param(o_notify, USB_HOST_CLASS_AUDIO_COUNT);
		else if (bInterfaceClass == USB_CLASS_COMM)
			inc_hw_param(o_notify, USB_HOST_CLASS_COMM_COUNT);
		else if (bInterfaceClass == USB_CLASS_HID)
			inc_hw_param(o_notify, USB_HOST_CLASS_HID_COUNT);
		else if (bInterfaceClass == USB_CLASS_PHYSICAL)
			inc_hw_param(o_notify, USB_HOST_CLASS_PHYSICAL_COUNT);
		else if (bInterfaceClass == USB_CLASS_STILL_IMAGE)
			inc_hw_param(o_notify, USB_HOST_CLASS_IMAGE_COUNT);
		else if (bInterfaceClass == USB_CLASS_PRINTER)
			inc_hw_param(o_notify, USB_HOST_CLASS_PRINTER_COUNT);
		else if (bInterfaceClass == USB_CLASS_MASS_STORAGE) {
			inc_hw_param(o_notify, USB_HOST_CLASS_STORAGE_COUNT);
			if (speed == USB_SPEED_SUPER)
				inc_hw_param(o_notify,
					USB_HOST_STORAGE_SUPER_COUNT);
			else if (speed == USB_SPEED_HIGH)
				inc_hw_param(o_notify,
					USB_HOST_STORAGE_HIGH_COUNT);
			else if (speed == USB_SPEED_FULL)
				inc_hw_param(o_notify,
					USB_HOST_STORAGE_FULL_COUNT);
		} else if (bInterfaceClass == USB_CLASS_HUB)
			inc_hw_param(o_notify, USB_HOST_CLASS_HUB_COUNT);
		else if (bInterfaceClass == USB_CLASS_CDC_DATA)
			inc_hw_param(o_notify, USB_HOST_CLASS_CDC_COUNT);
		else if (bInterfaceClass == USB_CLASS_CSCID)
			inc_hw_param(o_notify, USB_HOST_CLASS_CSCID_COUNT);
		else if (bInterfaceClass == USB_CLASS_CONTENT_SEC)
			inc_hw_param(o_notify, USB_HOST_CLASS_CONTENT_COUNT);
		else if (bInterfaceClass == USB_CLASS_VIDEO)
			inc_hw_param(o_notify, USB_HOST_CLASS_VIDEO_COUNT);
		else if (bInterfaceClass == USB_CLASS_WIRELESS_CONTROLLER)
			inc_hw_param(o_notify, USB_HOST_CLASS_WIRELESS_COUNT);
		else if (bInterfaceClass == USB_CLASS_MISC)
			inc_hw_param(o_notify, USB_HOST_CLASS_MISC_COUNT);
		else if (bInterfaceClass == USB_CLASS_APP_SPEC)
			inc_hw_param(o_notify, USB_HOST_CLASS_APP_COUNT);
		else if (bInterfaceClass == USB_CLASS_VENDOR_SPEC)
			inc_hw_param(o_notify, USB_HOST_CLASS_VENDOR_COUNT);

		if (speed == USB_SPEED_SUPER)
			inc_hw_param(o_notify, USB_HOST_SUPER_SPEED_COUNT);
		else if (speed == USB_SPEED_HIGH)
			inc_hw_param(o_notify, USB_HOST_HIGH_SPEED_COUNT);
		else if (speed == USB_SPEED_FULL)
			inc_hw_param(o_notify, USB_HOST_FULL_SPEED_COUNT);
		else if (speed == USB_SPEED_LOW)
			inc_hw_param(o_notify, USB_HOST_LOW_SPEED_COUNT);
	}
err:
	return ret;
}
#endif

static void check_unsupport_device(struct usb_device *dev)
{
	struct dev_table *id;

	/* check VID, PID */
	for (id = unsupport_device_table; id->dev.match_flags; id++) {
		if ((id->dev.match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		(id->dev.match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		id->dev.idVendor == le16_to_cpu(dev->descriptor.idVendor) &&
		id->dev.idProduct == le16_to_cpu(dev->descriptor.idProduct)) {
#if defined(CONFIG_USB_HOST_CERTI)
			send_usb_certi_uevent(USB_CERTI_UNSUPPORT_ACCESSORY);
#endif
			break;
		}
	}
}

static int dev_notify(struct notifier_block *self,
			       unsigned long action, void *dev)
{
	switch (action) {
	case USB_DEVICE_ADD:
		call_device_notify(dev, 1);
		call_battery_notify(dev, 1);
		check_roothub_device(dev, 1);
#if defined(CONFIG_USB_HW_PARAM)
		set_hw_param(dev);
#endif
		check_unsupport_device(dev);
		check_usbaudio(dev);
		check_usbgroup(dev);
		break;
	case USB_DEVICE_REMOVE:
		call_device_notify(dev, 0);
		call_battery_notify(dev, 0);
		check_roothub_device(dev, 0);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block dev_nb = {
	.notifier_call = dev_notify,
};

void register_usbdev_notify(void)
{
	usb_register_notify(&dev_nb);
}
EXPORT_SYMBOL(register_usbdev_notify);

void unregister_usbdev_notify(void)
{
	usb_unregister_notify(&dev_nb);
}
EXPORT_SYMBOL(unregister_usbdev_notify);

