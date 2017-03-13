/*
 * dwc3-sti.c - STiH407 family DWC3 specific Glue layer
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <dwc3-sti-uboot.h>
#include <fdtdec.h>
#include <regmap.h>
#include <syscon.h>
#include <usb.h>

DECLARE_GLOBAL_DATA_PTR;

__weak int __board_usb_init(int index, enum usb_init_type init)
{
	return 0;
}
/*int board_usb_init(int index, enum usb_init_type init)*/
/*        __attribute__((weak, alias("__board_usb_init")));*/

static int sti_dwc3_drd_init(struct sti_dwc3_platdata *plat)
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

static void sti_dwc3_init(struct sti_dwc3_platdata *plat)
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

int sti_dwc3_glue_init(enum usb_dr_mode mode)
{
	struct sti_dwc3_platdata plat;
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

	sti_dwc3_drd_init(&plat);
	sti_dwc3_init(&plat);

	return 0;
}
