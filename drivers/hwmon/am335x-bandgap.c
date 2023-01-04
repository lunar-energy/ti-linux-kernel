/*
 * Copyright (c) 2013 Jan Luebbe <j.luebbe@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#define DRV_NAME	"am335x_bandgap"

#define BANDGAP_CTRL			0x0
#define BANDGAP_CTRL_DTEMP_MASK		0x0000FF00
#define BANDGAP_CTRL_DTEMP_OFF		8
#define BANDGAP_CTRL_BGROFF		BIT(6)
#define BANDGAP_CTRL_SOC		BIT(4)
#define BANDGAP_CTRL_CLRZ		BIT(3) /* 0 = clear */
#define BANDGAP_CTRL_CONTCONV		BIT(2)
#define BANDGAP_CTRL_ECOZ		BIT(1)
#define BANDGAP_CTRL_TSHUT		BIT(0)

#define BANDGAP_TRIM			0x4
#define BANDGAP_TRIM_DTRBGAPC_MASK	0xFF000000
#define BANDGAP_TRIM_DTRBGAPC_OFF	24
#define BANDGAP_TRIM_DTRBGAPV_MASK	0x00FF0000
#define BANDGAP_TRIM_DTRBGAPV_OFF	16
#define BANDGAP_TRIM_DTRTEMPS_MASK	0x0000FF00
#define BANDGAP_TRIM_DTRTEMPS_OFF	8
#define BANDGAP_TRIM_DTRTEMPSC_MASK	0x000000FF
#define BANDGAP_TRIM_DTRTEMPSC_OFF	0

struct am335x_bandgap {
	u32 __iomem *regs;
	struct device *hwmon_dev;
};

static int am335x_bandgap_read(struct device *dev, enum hwmon_sensor_types type,
				u32 attr, int channel, long *temp)
{
	struct am335x_bandgap *data = dev_get_drvdata(dev);
	int value;

	switch (attr) {
	case hwmon_temp_input:
		value = readl(data->regs + BANDGAP_CTRL);
		value = (value & BANDGAP_CTRL_DTEMP_MASK) >> BANDGAP_CTRL_DTEMP_OFF;
		value = value * 1000;
		break;
	default:
		return -EOPNOTSUPP;
	}

	*temp = value;
	return 0;
}

static umode_t am335x_bandgap_is_visible(const void *_data, enum hwmon_sensor_types type,
				u32 attr, int channel)
{
	if (type != hwmon_temp)
		return 0;

	switch (attr) {
	case hwmon_temp_input:
		return 0444;
	default:
		return 0;
	}
}

static const u32 am335x_bandgap_temp_config[] = {
	HWMON_T_INPUT,
	0
};

static const struct hwmon_channel_info am335x_bandgap_hwmon_temp = {
	.type = hwmon_temp,
	.config = am335x_bandgap_temp_config
};

static const struct hwmon_channel_info *am335x_bandgap_hwmon_info[] = {
	&am335x_bandgap_hwmon_temp,
	NULL
};

static const struct hwmon_ops am335x_bandgap_hwmon_ops = {
	.is_visible = am335x_bandgap_is_visible,
	.read = am335x_bandgap_read,
};

static const struct hwmon_chip_info am335x_bandgap_chip_info = {
	.ops = &am335x_bandgap_hwmon_ops,
	.info = am335x_bandgap_hwmon_info,
};

static int am335x_bandgap_probe(struct platform_device *pdev)
{
	struct am335x_bandgap *data;
	int err;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->regs))
		return PTR_ERR(data->regs);

	platform_set_drvdata(pdev, data);

	data->hwmon_dev = hwmon_device_register_with_info(&pdev->dev,
						DRV_NAME,
						data,
						&am335x_bandgap_chip_info,
						NULL);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		dev_err(&pdev->dev, "Class registration failed (%d)\n", err);
		return err;
	}

	/* enable HW sensor */
	writel(BANDGAP_CTRL_SOC | BANDGAP_CTRL_CLRZ | BANDGAP_CTRL_CONTCONV,
		data->regs + BANDGAP_CTRL);

	return 0;
}

static int am335x_bandgap_remove(struct platform_device *pdev)
{
	struct am335x_bandgap *data = platform_get_drvdata(pdev);

	/* disable HW sensor */
	writel(0x0, data->regs + BANDGAP_CTRL);

	hwmon_device_unregister(data->hwmon_dev);

	return 0;
}

static const struct of_device_id am335x_bandgap_match[] = {
	{ .compatible = "ti,am335x-bandgap" },
	{},
};

static struct platform_driver am335x_bandgap_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(am335x_bandgap_match),
	},
	.probe	= am335x_bandgap_probe,
	.remove	= am335x_bandgap_remove,
};

module_platform_driver(am335x_bandgap_driver);

MODULE_AUTHOR("Jan Luebbe <j.luebbe@pengutronix.de>");
MODULE_DESCRIPTION("AM335x temperature sensor driver");
MODULE_LICENSE("GPL");
