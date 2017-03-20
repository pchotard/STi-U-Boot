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
#include <fdtdec.h>
#include <libfdt.h>
#include <reset-uclass.h>
#include <usb.h>

#include <linux/usb/dwc3.h>

#include "xhci.h"

DECLARE_GLOBAL_DATA_PTR;

struct sti_xhci_platdata {
	struct reset_ctl powerdown_ctl;
	struct reset_ctl softreset_ctl;
	phys_addr_t dwc3_regs;
};

struct sti_xhci_priv {
	struct xhci_ctrl ctrl;
};


static int sti_xhci_ofdata_to_platdata(struct udevice *dev)
{
	struct sti_xhci_platdata *plat = dev_get_platdata(dev);
	struct udevice *dev_phy;
	int ret;
	int phy_node;
	int dwc3_node;
	u32 reg[2];

	/* get powerdown reset */
	ret = reset_get_by_name(dev, "powerdown", &plat->powerdown_ctl);
	if (ret) {
		error("can't get powerdown reset for %s (%d)", dev->name, ret);
		return ret;
	}

	/* get softreset reset */
	ret = reset_get_by_name(dev, "softreset", &plat->softreset_ctl);
	if (ret) {
		error("can't get soft reset for %s (%d)", dev->name, ret);
		return ret;
	}

	/* deassert both powerdown and softreset */
	ret = reset_deassert(&plat->powerdown_ctl);
	if (ret < 0) {
		error("DWC3 powerdown reset deassert failed: %d", ret);
		return ret;
	}

	ret = reset_deassert(&plat->softreset_ctl);
	if (ret < 0) {
		error("DWC3 soft reset deassert failed: %d", ret);
		return ret;
	}

	/* check if dwc3 subnode is present */
	dwc3_node = fdt_first_subnode(gd->fdt_blob, dev_of_offset(dev));
	if (dwc3_node <= 0) {
		error("Can't find subnode for st_dwc3 glue driver\n");
		return -ENODEV;
	}

	if (fdt_node_check_compatible(gd->fdt_blob, dwc3_node,
				      "snps,dwc3") != 0) {
		error("Can't find dwv3 subnode for st_dwc3 glue driver\n");
		return -ENODEV;
	}

	/*
	 * now parse the dwc3 node
	 * first get the phy node: only usb2-phy, no need to get usb3-phy as
	 * usb3 is not wired
	 */
	phy_node = fdtdec_lookup_phandle(gd->fdt_blob, dwc3_node, "phys");
	if (phy_node <= 0) {
		error("Can't find usb phy device\n");
		return -ENODEV;
	}

	/* get the dwc3 register space base address */
	if (fdtdec_get_int_array(gd->fdt_blob, dwc3_node, "reg", reg,
				 ARRAY_SIZE(reg))) {
		debug("dwc3 node has bad/missing 'reg' property\n");
		return -FDT_ERR_NOTFOUND;
	}
	plat->dwc3_regs = reg[0];

	/* probe associated phy */
	return uclass_get_device_by_of_offset(UCLASS_MISC, phy_node, &dev_phy);
};

static int sti_xhci_core_init(struct dwc3 *dwc3_reg)
{
	int ret;

	ret = dwc3_core_init(dwc3_reg);
	if (ret) {
		debug("failed to initialize core\n");
		return ret;
	}

	/* We are hard-coding DWC3 core to Host Mode */
	dwc3_set_mode(dwc3_reg, DWC3_GCTL_PRTCAP_HOST);

	return 0;
}

static int sti_dwc3_probe(struct udevice *dev)
{
	struct sti_xhci_platdata *plat = dev_get_platdata(dev);
	struct xhci_hcor *hcor;
	struct xhci_hccr *hccr;
	struct dwc3 *dwc3_reg;

	hccr = (struct xhci_hccr *)plat->dwc3_regs;
	hcor = (struct xhci_hcor *)((phys_addr_t)hccr +
			HC_LENGTH(xhci_readl(&(hccr)->cr_capbase)));

	dwc3_reg = (struct dwc3 *)((char *)(hccr) + DWC3_REG_OFFSET);

	sti_xhci_core_init(dwc3_reg);

	return xhci_register(dev, hccr, hcor);
}

static const struct udevice_id sti_dwc3_ids[] = {
	{ .compatible = "st,stih407-dwc3" },
	{ }
};

U_BOOT_DRIVER(xhci_sti) = {
	.name = "xhci_sti",
	.id = UCLASS_USB,
	.of_match = sti_dwc3_ids,
	.ofdata_to_platdata = sti_xhci_ofdata_to_platdata,
	.probe = sti_dwc3_probe,
	.remove = xhci_deregister,
	.ops = &xhci_usb_ops,
	.priv_auto_alloc_size = sizeof(struct sti_xhci_priv),
	.platdata_auto_alloc_size = sizeof(struct sti_xhci_platdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
