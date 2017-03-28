/*
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _USB_PHY_H
#define _USB_PHY_H

#include <linux/errno.h>

struct udevice;

/**
 * struct usb_phy_desc - A handle to a USB PHY.
 *
 * Clients provide storage for reset control handles. The content of the
 * structure is managed solely by the reset API and reset drivers. A reset
 * control struct is initialized by "get"ing the reset control struct. The
 * reset control struct is passed to all other reset APIs to identify which
 * reset signal to operate upon.
 *
 * @dev: The device which implements the reset signal.
 *
 */
struct usb_phy_desc {
	struct udevice *dev;
};

#ifdef CONFIG_DM_RESET
/**
 * reset_get_by_index - Get/request a USB PHY by integer index.
 *
 * This looks up and requests a USB PHY. The index is relative to the
 * client device; each device is assumed to have n USB PHY associated
 * with it somehow, and this function finds and requests one of them.
 *
 * @dev:	  The client device.
 * @index:	  The index of the USB PHY to request, within the client's
 *		  list of USB PHYs.
 * @usb_phy_desc  A pointer to a USB PHY struct to initialize.
 * @return 0 if OK, or a negative error code.
 */
int usb_phy_get_by_index(struct udevice *dev, int index,
			 struct usb_phy_desc *usb_phy_desc);

/**
 * usb_phy_get_by_name - Get/request a USB PHY by name.
 *
 * This looks up and requests a USB PHY. The name is relative to the
 * client device; each device is assumed to have n USB PHY associated
 * with it somehow, and this function finds and requests one of them.
 *
 * @dev:	  The client device.
 * @name:	  The name of the USB PHY to request, within the client's
 *		  list of USB PHYs.
 * @usb_phy_desc: A pointer to a USB PHY struct to initialize.
 * @return 0 if OK, or a negative error code.
 */
int usb_phy_get_by_name(struct udevice *dev, const char *name,
			struct usb_phy_desc *usb_phy_desc);

/**
 * init - Initialize a USB PHY.
 *
 * Typically initialize PHY specific registers
 * and deassert USB PHY associated reset signals.
 *
 * @usb_phy_desc: A USB PHY struct that was previously successfully
 *		  requested by reset_get_by_*().
 * @return 0 if OK, or a negative error code.
 */
int usb_phy_init(struct usb_phy_desc *usb_phy_desc);

/**
 * exit - operations to be performed while exiting
 *
 * Typically deassert USB PHY associated reset signals.
 *
 * @usb_phy_desc: A USB PHY struct that was previously successfully
 *		  requested by reset_get_by_*().
 * @return 0 if OK, or a negative error code.
 */
int usb_phy_exit(struct usb_phy_desc *usb_phy_desc);

#else
static inline int usb_phy_get_by_index(struct udevice *dev, int index,
				     struct usb_phy_desc *usb_phy_desc)
{
	return -ENOTSUPP;
}

static inline int usb_phy_get_by_name(struct udevice *dev, const char *name,
				    struct usb_phy_desc *usb_phy_desc)
{
	return -ENOTSUPP;
}

static inline int init(struct usb_phy_desc *usb_phy_desc)
{
	return 0;
}

static inline int exit(struct usb_phy_desc *usb_phy_desc)
{
	return 0;
}

#endif

#endif
