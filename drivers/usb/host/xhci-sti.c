/*
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <usb.h>
#include <usb_phy.h>

#include "xhci.h"
#include <linux/usb/dwc3.h>

DECLARE_GLOBAL_DATA_PTR;

__weak int __board_usb_init(int index, enum usb_init_type init)
{
	return 0;
}
/*int board_usb_init(int index, enum usb_init_type init)*/
/*        __attribute__((weak, alias("__board_usb_init")));*/

struct sti_xhci_platdata {
	struct usb_phy_desc usb_phy;
	phys_addr_t dwc3_regs;
};

struct sti_xhci_priv {
	struct xhci_ctrl ctrl;
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

static int sti_xhci_ofdata_to_platdata(struct udevice *dev)
{
	struct sti_xhci_platdata *plat = dev_get_platdata(dev);
	u32 reg[2];
	int ret;

	/* get the dwc3 register space base address */
	if (fdtdec_get_int_array(gd->fdt_blob, dev_of_offset(dev), "reg", reg,
				 ARRAY_SIZE(reg))) {
		debug("dwc3 node has bad/missing 'reg' property\n");
		return -FDT_ERR_NOTFOUND;
	}
	plat->dwc3_regs = reg[0];

	ret = usb_phy_get_by_name(dev, "usb2-phy", &plat->usb_phy);
	if (ret)
		error("USB PHY DT node not found for %s\n", dev->name);

	return 0;
}

static int sti_xhci_probe(struct udevice *dev)
{
	struct sti_xhci_platdata *plat = dev_get_platdata(dev);
	struct xhci_hcor *hcor;
	struct xhci_hccr *hccr;
	struct dwc3 *dwc3_reg;
	int ret;

	hccr = (struct xhci_hccr *)plat->dwc3_regs;
	hcor = (struct xhci_hcor *)((phys_addr_t)hccr +
			HC_LENGTH(xhci_readl(&(hccr)->cr_capbase)));

	ret = usb_phy_init(&plat->usb_phy);
	if (ret) {
		error("Can't init USB PHY for %s\n", dev->name);
		return ret;
	}

	dwc3_reg = (struct dwc3 *)((char *)(hccr) + DWC3_REG_OFFSET);

	sti_xhci_core_init(dwc3_reg);

	return xhci_register(dev, hccr, hcor);
}

static const struct udevice_id sti_xhci_ids[] = {
	{ .compatible = "snps,dwc3" },
	{ }
};

U_BOOT_DRIVER(xhci_sti) = {
	.name = "xhci_sti",
	.id = UCLASS_USB,
	.of_match = sti_xhci_ids,
	.ofdata_to_platdata = sti_xhci_ofdata_to_platdata,
	.probe = sti_xhci_probe,
	.remove = xhci_deregister,
	.ops = &xhci_usb_ops,
	.priv_auto_alloc_size = sizeof(struct sti_xhci_priv),
	.platdata_auto_alloc_size = sizeof(struct sti_xhci_platdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
