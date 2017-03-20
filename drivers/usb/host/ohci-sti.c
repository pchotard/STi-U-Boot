/*
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <errno.h>
#include "ohci.h"
#include <reset-uclass.h>
#include <usb.h>

DECLARE_GLOBAL_DATA_PTR;

#if !defined(CONFIG_USB_OHCI_NEW)
# error "Generic OHCI driver requires CONFIG_USB_OHCI_NEW"
#endif

struct sti_ohci_priv {
	ohci_t ohci;
	struct reset_ctl power_ctl;
	struct reset_ctl softreset_ctl;
};

static int ohci_usb_probe(struct udevice *dev)
{
	struct sti_ohci_priv *priv = dev_get_priv(dev);
	struct ohci_regs *regs;
	struct udevice *dev_phy;
	int ret, phy_node;

	regs = (struct ohci_regs *)dev_get_addr(dev);
	if (regs == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = reset_get_by_name(dev, "power", &priv->power_ctl);
	if (ret) {
		error("can't get power reset for %s (%d)", dev->name, ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "softreset", &priv->softreset_ctl);
	if (ret) {
		error("can't get soft reset for %s (%d)", dev->name, ret);
		return ret;
	}

	ret = reset_deassert(&priv->power_ctl);
	if (ret < 0) {
		error("OHCI power reset deassert failed: %d", ret);
		return ret;
	}

	ret = reset_deassert(&priv->softreset_ctl);
	if (ret < 0) {
		error("OHCI soft reset deassert failed: %d", ret);
		return ret;
	}

	/* get phy node */
	phy_node = fdtdec_lookup_phandle(gd->fdt_blob, dev->of_offset, "phys");
	if (phy_node <= 0) {
		error("Not found usb phy device\n");
		return -ENODEV;
	}

	/* probe associated phy node */
	ret = uclass_get_device_by_of_offset(UCLASS_MISC, phy_node, &dev_phy);

	return ohci_register(dev, regs);
}

static const struct udevice_id sti_usb_ids[] = {
	{ .compatible = "st,st-ohci-300x" },
	{ }
};

U_BOOT_DRIVER(ohci_sti) = {
	.name = "ohci_sti",
	.id = UCLASS_USB,
	.of_match = sti_usb_ids,
	.probe = ohci_usb_probe,
	.remove = ohci_deregister,
	.ops = &ohci_usb_ops,
	.priv_auto_alloc_size = sizeof(struct sti_ohci_priv),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
