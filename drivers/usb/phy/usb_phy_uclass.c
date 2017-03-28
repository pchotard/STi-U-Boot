/*
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <usb_phy-uclass.h>

DECLARE_GLOBAL_DATA_PTR;

static inline struct usb_phy_ops *usb_phy_dev_ops(struct udevice *dev)
{
	return (struct usb_phy_ops *)dev->driver->ops;
}

int usb_phy_get_by_index(struct udevice *dev, int index,
			 struct usb_phy_desc *usb_phy_desc)
{
	struct fdtdec_phandle_args args;
	int ret;
	struct udevice *dev_usb_phy;

	debug("%s(dev=%p, index=%d, usb_phy_desc=%p)\n", __func__, dev, index,
	      usb_phy_desc);

	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, dev_of_offset(dev),
					     "phys", "#phy-cells", 0,
					     index, &args);
	if (ret) {
		debug("%s: fdtdec_parse_phandle_with_args failed: %d\n",
		      __func__, ret);
		return ret;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_USB_PHY, args.node,
					     &dev_usb_phy);
	if (ret) {
		debug("%s: uclass_get_device_by_of_offset failed: %d\n",
		      __func__, ret);
		return ret;
	}

	usb_phy_desc->dev = dev_usb_phy;

	return 0;
}

int usb_phy_get_by_name(struct udevice *dev, const char *name,
			struct usb_phy_desc *usb_phy_desc)
{
	int index;

	debug("%s(dev=%p, name=%s, usb_phy_desc=%p)\n", __func__, dev, name,
	      usb_phy_desc);

	index = fdt_stringlist_search(gd->fdt_blob, dev_of_offset(dev),
				      "phy-names", name);
	if (index < 0) {
		debug("fdt_stringlist_search() failed: %d\n", index);
		return index;
	}

	return usb_phy_get_by_index(dev, index, usb_phy_desc);
}

int usb_phy_init(struct usb_phy_desc *usb_phy_desc)
{
	struct usb_phy_ops *ops = usb_phy_dev_ops(usb_phy_desc->dev);

	debug("%s(usb_phy_desc=%p)\n", __func__, usb_phy_desc);

	return ops->init(usb_phy_desc);
}

int usb_phy_exit(struct usb_phy_desc *usb_phy_desc)
{
	struct usb_phy_ops *ops = usb_phy_dev_ops(usb_phy_desc->dev);

	debug("%s(usb_phy_desc=%p)\n", __func__, usb_phy_desc);

	return ops->exit(usb_phy_desc);
}

UCLASS_DRIVER(usb_phy) = {
	.id	= UCLASS_USB_PHY,
	.name	= "usb_phy",
};
