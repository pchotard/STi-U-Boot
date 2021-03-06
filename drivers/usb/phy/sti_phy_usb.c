/*
 * Copyright (c) 2017
 * Patrice Chotard <patrice.chotard@st.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <bitfield.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <regmap.h>
#include <reset-uclass.h>
#include <syscon.h>
#include <wait_bit.h>

#include <linux/bitops.h>
#include <linux/compat.h>

DECLARE_GLOBAL_DATA_PTR;

/* Default PHY_SEL and REFCLKSEL configuration */
#define STIH407_USB_PICOPHY_CTRL_PORT_CONF	0x6

/* ports parameters overriding */
#define STIH407_USB_PICOPHY_PARAM_DEF		0x39a4dc

#define PHYPARAM_REG	1
#define PHYCTRL_REG	2
#define PHYPARAM_NB	3

struct sti_phy_usb {
	struct regmap *regmap;
	struct reset_ctl global_ctl;
	struct reset_ctl port_ctl;
	int param;
	int ctrl;
};

static int sti_phy_usb_deassert(struct sti_phy_usb *phy)
{
	int ret;

	ret = reset_deassert(&phy->global_ctl);
	if (ret < 0) {
		error("PHY global deassert failed: %d", ret);
		return ret;
	}

	ret = reset_deassert(&phy->port_ctl);
	if (ret < 0)
		error("PHY port deassert failed: %d", ret);

	return ret;
}

static void sti_phy_usb_init(struct sti_phy_usb *phy)
{
	void __iomem *reg;

	/* set ctrl picophy value */
	reg = (void __iomem *)phy->regmap->base + phy->ctrl;
	/* CTRL_PORT mask is 0x1f */
	clrsetbits_le32(reg, 0x1f, STIH407_USB_PICOPHY_CTRL_PORT_CONF);

	/* set ports parameters overriding */
	reg = (void __iomem *)phy->regmap->base + phy->param;
	/* PARAM_DEF mask is 0xffffffff */
	clrsetbits_le32(reg, 0xffffffff, STIH407_USB_PICOPHY_PARAM_DEF);
}

int sti_phy_usb_probe(struct udevice *dev)
{
	struct sti_phy_usb *priv = dev_get_priv(dev);
	struct udevice *syscon;
	struct fdtdec_phandle_args syscfg_phandle;
	u32 cells[PHYPARAM_NB];
	int ret, count;

	/* get corresponding syscon phandle */
	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, dev->of_offset,
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

	priv->regmap = syscon_get_regmap(syscon);
	if (!priv->regmap) {
		error("unable to find regmap\n");
		return -ENODEV;
	}

	/* get phy param offset */
	count = fdtdec_get_int_array_count(gd->fdt_blob, dev->of_offset,
					   "st,syscfg", cells,
					   ARRAY_SIZE(cells));

	if (count < 0) {
		error("Bad PHY st,syscfg property %d\n", count);
		return -EINVAL;
	}

	if (count > PHYPARAM_NB) {
		error("Unsupported PHY param count %d\n", count);
		return -EINVAL;
	}

	priv->param = cells[PHYPARAM_REG];
	priv->ctrl = cells[PHYCTRL_REG];

	/* get global reset control */
	ret = reset_get_by_name(dev, "global", &priv->global_ctl);
	if (ret) {
		error("can't get global reset for %s (%d)", dev->name, ret);
		return ret;
	}

	/* get port reset control */
	ret = reset_get_by_name(dev, "port", &priv->port_ctl);
	if (ret) {
		error("can't get port reset for %s (%d)", dev->name, ret);
		return ret;
	}

	sti_phy_usb_init(priv);

	return sti_phy_usb_deassert(priv);
}

static const struct udevice_id sti_phy_usb_ids[] = {
	{ .compatible = "st,stih407-usb2-phy" },
	{ }
};

U_BOOT_DRIVER(sti_phy_usb) = {
	.name = "sti_phy_usb",
	.id = UCLASS_MISC,
	.of_match = sti_phy_usb_ids,
	.probe = sti_phy_usb_probe,
	.priv_auto_alloc_size = sizeof(struct sti_phy_usb),
};
