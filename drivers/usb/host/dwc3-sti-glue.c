/*
 * STiH407 family DWC3 specific Glue layer
 *
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <dwc3-sti-glue.h>
#include <errno.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <dm/lists.h>
#include <regmap.h>
#include <reset-uclass.h>
#include <syscon.h>
#include <usb.h>

#include <linux/usb/dwc3.h>

DECLARE_GLOBAL_DATA_PTR;

struct sti_dwc3_glue_platdata {
	phys_addr_t syscfg_base;
	phys_addr_t glue_base;
	phys_addr_t syscfg_offset;
	struct reset_ctl powerdown_ctl;
	struct reset_ctl softreset_ctl;
	enum usb_dr_mode mode;
};

static int sti_dwc3_glue_drd_init(struct sti_dwc3_glue_platdata *plat)
{
	unsigned long val;

	val = readl(plat->syscfg_base + plat->syscfg_offset);

	val &= USB3_CONTROL_MASK;

	switch (plat->mode) {
	case USB_DR_MODE_PERIPHERAL:
		val &= ~(USB3_DELAY_VBUSVALID
			| USB3_SEL_FORCE_OPMODE | USB3_FORCE_OPMODE(0x3)
			| USB3_SEL_FORCE_DPPULLDOWN2 | USB3_FORCE_DPPULLDOWN2
			| USB3_SEL_FORCE_DMPULLDOWN2 | USB3_FORCE_DMPULLDOWN2);

		val |= USB3_DEVICE_NOT_HOST | USB3_FORCE_VBUSVALID;
		break;

	case USB_DR_MODE_HOST:
		val &= ~(USB3_DEVICE_NOT_HOST | USB3_FORCE_VBUSVALID
			| USB3_SEL_FORCE_OPMODE	| USB3_FORCE_OPMODE(0x3)
			| USB3_SEL_FORCE_DPPULLDOWN2 | USB3_FORCE_DPPULLDOWN2
			| USB3_SEL_FORCE_DMPULLDOWN2 | USB3_FORCE_DMPULLDOWN2);

		val |= USB3_DELAY_VBUSVALID;
		break;

	default:
		error("Unsupported mode of operation %d\n", plat->mode);
		return -EINVAL;
	}
	return writel(val, plat->syscfg_base + plat->syscfg_offset);
}

static void sti_dwc3_glue_init(struct sti_dwc3_glue_platdata *plat)
{
	unsigned long reg;

	reg = readl(plat->glue_base + CLKRST_CTRL);

	reg |= AUX_CLK_EN | EXT_CFG_RESET_N | XHCI_REVISION;
	reg &= ~SW_PIPEW_RESET_N;

	writel(reg, plat->glue_base + CLKRST_CTRL);

	/* configure mux for vbus, powerpresent and bvalid signals */
	reg = readl(plat->glue_base + USB2_VBUS_MNGMNT_SEL1);

	reg |= SEL_OVERRIDE_VBUSVALID(USB2_VBUS_UTMIOTG) |
	       SEL_OVERRIDE_POWERPRESENT(USB2_VBUS_UTMIOTG) |
	       SEL_OVERRIDE_BVALID(USB2_VBUS_UTMIOTG);

	writel(reg, plat->glue_base + USB2_VBUS_MNGMNT_SEL1);

	setbits_le32(plat->glue_base + CLKRST_CTRL, SW_PIPEW_RESET_N);
}

int sti_dwc3_init(enum usb_dr_mode mode)
{
	struct sti_dwc3_glue_platdata plat;
	struct fdtdec_phandle_args syscfg_phandle;
	struct udevice *syscon;
	struct regmap *regmap;
	int node, ret;
	const void *blob = gd->fdt_blob;
	u32 reg[4];

	/* find the dwc3 node */
	node = fdt_node_offset_by_compatible(blob, -1, "st,stih407-dwc3");

	ret = fdtdec_get_int_array(blob, node, "reg", reg, ARRAY_SIZE(reg));
	if (ret) {
		error("unable to find st,stih407-dwc3 reg property(%d)\n", ret);
		return ret;
	}

	plat.glue_base = reg[0];
	plat.syscfg_offset = reg[2];
	plat.mode = mode;

	/* get corresponding syscon phandle */
	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, node,
					     "st,syscfg", NULL, 0, 0,
					     &syscfg_phandle);
	if (ret < 0) {
		error("Can't get syscfg phandle: %d\n", ret);
		return ret;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_SYSCON, syscfg_phandle.node,
					     &syscon);
	if (ret) {
		error("unable to find syscon device (%d)\n", ret);
		return ret;
	}

	/* get syscfg-reg base address */
	regmap = syscon_get_regmap(syscon);
	if (!regmap) {
		error("unable to find regmap\n");
		return -ENODEV;
	}
	plat.syscfg_base = regmap->base;

	sti_dwc3_glue_drd_init(&plat);
	sti_dwc3_glue_init(&plat);

	return 0;
}

static int sti_dwc3_glue_ofdata_to_platdata(struct udevice *dev)
{
	struct sti_dwc3_glue_platdata *plat = dev_get_platdata(dev);
	struct fdtdec_phandle_args syscfg_phandle;
	struct udevice *syscon;
	struct regmap *regmap;
	int ret;
	u32 reg[4];

	ret = fdtdec_get_int_array(gd->fdt_blob, dev_of_offset(dev),
				   "reg", reg, ARRAY_SIZE(reg));
	if (ret) {
		error("unable to find st,stih407-dwc3 reg property(%d)\n", ret);
		return ret;
	}

	plat->glue_base = reg[0];
	plat->syscfg_offset = reg[2];

	/* get corresponding syscon phandle */
	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, dev_of_offset(dev),
					     "st,syscfg", NULL, 0, 0,
					     &syscfg_phandle);
	if (ret < 0) {
		error("Can't get syscfg phandle: %d\n", ret);
		return ret;
	}

	ret = uclass_get_device_by_of_offset(UCLASS_SYSCON, syscfg_phandle.node,
					     &syscon);
	if (ret) {
		error("unable to find syscon device (%d)\n", ret);
		return ret;
	}

	/* get syscfg-reg base address */
	regmap = syscon_get_regmap(syscon);
	if (!regmap) {
		error("unable to find regmap\n");
		return -ENODEV;
	}
	plat->syscfg_base = regmap->base;

	/* get powerdown reset */
	ret = reset_get_by_name(dev, "powerdown", &plat->powerdown_ctl);
	if (ret) {
		error("can't get powerdown reset for %s (%d)", dev->name, ret);
		return ret;
	}

	/* get softreset reset */
	ret = reset_get_by_name(dev, "softreset", &plat->softreset_ctl);
	if (ret)
		error("can't get soft reset for %s (%d)", dev->name, ret);

	return ret;
};

static int sti_dwc3_glue_bind(struct udevice *dev)
{
	int dwc3_node;

	/* check if one subnode is present */
	dwc3_node = fdt_first_subnode(gd->fdt_blob, dev_of_offset(dev));
	if (dwc3_node <= 0) {
		error("Can't find subnode for %s\n", dev->name);
		return -ENODEV;
	}

	/* check if the subnode compatible string is the dwc3 one*/
	if (fdt_node_check_compatible(gd->fdt_blob, dwc3_node,
				      "snps,dwc3") != 0) {
		error("Can't find dwc3 subnode for %s\n", dev->name);
		return -ENODEV;
	}

	return dm_scan_fdt_dev(dev);
}

static int sti_dwc3_glue_probe(struct udevice *dev)
{
	struct sti_dwc3_glue_platdata *plat = dev_get_platdata(dev);
	int ret;

	/* deassert both powerdown and softreset */
	ret = reset_deassert(&plat->powerdown_ctl);
	if (ret < 0) {
		error("DWC3 powerdown reset deassert failed: %d", ret);
		return ret;
	}

	ret = reset_deassert(&plat->softreset_ctl);
	if (ret < 0)
		error("DWC3 soft reset deassert failed: %d", ret);

	return ret;
}

static int sti_dwc3_glue_remove(struct udevice *dev)
{
	struct sti_dwc3_glue_platdata *plat = dev_get_platdata(dev);
	int ret;

	/* assert both powerdown and softreset */
	ret = reset_assert(&plat->powerdown_ctl);
	if (ret < 0) {
		error("DWC3 powerdown reset deassert failed: %d", ret);
		return ret;
	}

	ret = reset_assert(&plat->softreset_ctl);
	if (ret < 0)
		error("DWC3 soft reset deassert failed: %d", ret);

	return ret;
}

static const struct udevice_id sti_dwc3_glue_ids[] = {
	{ .compatible = "st,stih407-dwc3" },
	{ }
};

U_BOOT_DRIVER(dwc3_sti_glue) = {
	.name = "dwc3_sti_glue",
	.id = UCLASS_MISC,
	.of_match = sti_dwc3_glue_ids,
	.ofdata_to_platdata = sti_dwc3_glue_ofdata_to_platdata,
	.probe = sti_dwc3_glue_probe,
	.remove = sti_dwc3_glue_remove,
	.bind = sti_dwc3_glue_bind,
	.platdata_auto_alloc_size = sizeof(struct sti_dwc3_glue_platdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
