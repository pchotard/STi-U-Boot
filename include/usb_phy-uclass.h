/*
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _USB_PHY_UCLASS_H
#define _USB_PHY_UCLASS_H

/* See usb_phy.h for background documentation. */

#include <usb_phy.h>

struct udevice;

/**
 * struct usb_phy_ops - The functions that a usb_phy controller driver must
 * implement.
 */
struct usb_phy_ops {
	/**
	 * init - operations to be performed for USB PHY initialisation
	 *
	 * Typically, registers init and reset deassertion
	 *
	 * @usb_phy_desc: The usb_phy struct to init;
	 * @return 0 if OK, or a negative error code.
	 */
	int (*init)(struct usb_phy_desc *usb_phy_desc);
	/**
	 * exit - operations to be performed while exiting
	 *
	 * Typically reset assertion
	 *
	 * @usb_phy_desc: The usb_phy to free.
	 * @return 0 if OK, or a negative error code.
	 */
	int (*exit)(struct usb_phy_desc *usb_phy_desc);
};

#endif
