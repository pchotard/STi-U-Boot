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
#include "ehci.h"
#include <reset-uclass.h>
#include <usb.h>
#include <usb_phy.h>

DECLARE_GLOBAL_DATA_PTR;

struct sti_ehci_priv {
	struct ehci_ctrl ctrl;
	struct reset_ctl power_ctl;
	struct reset_ctl softreset_ctl;
	struct usb_phy_desc usb_phy;
};

static int sti_ehci_probe(struct udevice *dev)
{
	struct sti_ehci_priv *priv = dev_get_priv(dev);
	struct ehci_hccr *hccr = priv->ctrl.hccr;
	struct ehci_hcor *hcor;
	int ret;

	hccr = (struct ehci_hccr *)dev_get_addr(dev);

	if (hccr == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = reset_get_by_name(dev, "power", &priv->power_ctl);
	if (ret) {
		error("can't get power for %s: %d", dev->name, ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "softreset", &priv->softreset_ctl);
	if (ret) {
		error("can't get soft reset for %s: %d", dev->name, ret);
		return ret;
	}

	ret = usb_phy_get_by_name(dev, "usb", &priv->usb_phy);
	if (ret) {
		error("USB PHY DT node not found for %s: %d", dev->name, ret);
		return ret;
	}

	ret = reset_deassert(&priv->softreset_ctl);
	if (ret < 0) {
		error("EHCI soft reset deassert failed: %d", ret);
		return ret;
	}

	ret = reset_deassert(&priv->power_ctl);
	if (ret < 0) {
		error("EHCI power deassert failed: %d", ret);
		return ret;
	}

	ret = usb_phy_init(&priv->usb_phy);
	if (ret) {
		error("Can't init USB PHY for %s: %d\n", dev->name, ret);
		return ret;
	}

	hcor = (struct ehci_hcor *)((phys_addr_t)hccr +
			HC_LENGTH(ehci_readl(&(hccr)->cr_capbase)));

	return ehci_register(dev, hccr, hcor, NULL, 0, USB_INIT_HOST);
}

static int sti_ehci_remove(struct udevice *dev)
{
	struct sti_ehci_priv *priv = dev_get_priv(dev);
	int ret;

	ret = ehci_deregister(dev);
	if (ret)
		return ret;

	ret = reset_assert(&priv->power_ctl);
	if (ret < 0) {
		error("EHCI power assert failed: %d", ret);
		return ret;
	}

	ret = reset_assert(&priv->softreset_ctl);
	if (ret < 0)
		error("EHCI soft reset assert failed: %d", ret);

	return ret;
}

static const struct udevice_id sti_usb_ids[] = {
	{ .compatible = "st,st-ehci-300x" },
	{ }
};

U_BOOT_DRIVER(ehci_sti) = {
	.name = "ehci_sti",
	.id = UCLASS_USB,
	.of_match = sti_usb_ids,
	.probe = sti_ehci_probe,
	.remove = sti_ehci_remove,
	.ops = &ehci_usb_ops,
	.priv_auto_alloc_size = sizeof(struct sti_ehci_priv),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};
