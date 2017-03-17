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

DECLARE_GLOBAL_DATA_PTR;

struct sti_ehci_priv {
	struct ehci_ctrl ctrl;
	struct reset_ctl power_ctl;
	struct reset_ctl softreset_ctl;
};

static int sti_ehci_probe(struct udevice *dev)
{
	struct sti_ehci_priv *priv = dev_get_priv(dev);
	struct ehci_hccr *hccr = priv->ctrl.hccr;
	struct ehci_hcor *hcor;
	struct udevice *dev_phy;
	int ret, phy_node;

	hccr = (struct ehci_hccr *)dev_get_addr(dev);

	if (hccr == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = reset_get_by_name(dev, "power", &priv->power_ctl);
	if (ret) {
		error("can't get power for %s (%d)", dev->name, ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "softreset", &priv->softreset_ctl);
	if (ret) {
		error("can't get soft reset for %s (%d)", dev->name, ret);
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

	/* get phy node */
	phy_node = fdtdec_lookup_phandle(gd->fdt_blob, dev->of_offset, "phys");
	if (phy_node <= 0) {
		error("USB PHY DT node not found.\n");
		return -ENODEV;
	}

	/* probe associated phy */
	ret = uclass_get_device_by_of_offset(UCLASS_MISC, phy_node, &dev_phy);

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
	if (ret < 0) {
		error("EHCI soft reset assert failed: %d", ret);
		return ret;
	}

	return 0;
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
