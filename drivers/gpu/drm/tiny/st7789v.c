// SPDX-License-Identifier: GPL-2.0+
/*
 * DRM driver for Sitronix ST7789V panels
 *
 * Copyright 2025 Nithin Manne <nithinmanne@gmail.com>
 *
 * Based on ili9341.c, st7735r.c and ../panel/panel-sitronix-st7789v.c:
 * Copyright 2018, 2017 David Lechner, 2019 Glider bvba and 2017 Free Electrons
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/spi/spi.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_mipi_dbi.h>
#include <drm/drm_modeset_helper.h>
#include <video/mipi_display.h>


#define ST7789V_PORCTRL_CMD		0xb2
#define ST7789V_GCTRL_CMD		0xb7
#define ST7789V_VCOMS_CMD		0xbb
#define ST7789V_LCMCTRL_CMD		0xc0
#define ST7789V_VDVVRHEN_CMD	0xc2
#define ST7789V_VRHS_CMD		0xc3
#define ST7789V_VDVS_CMD		0xc4
#define ST7789V_FRCTRL2_CMD		0xc6
#define ST7789V_PWCTRL1_CMD		0xd0
#define ST7789V_PVGAMCTRL_CMD	0xe0
#define ST7789V_NVGAMCTRL_CMD	0xe1

#define ST7789V_MADCTL_MV	BIT(5)
#define ST7789V_MADCTL_MX	BIT(6)
#define ST7789V_MADCTL_MY	BIT(7)


static void st7789v_enable(struct drm_simple_display_pipe *pipe,
			     struct drm_crtc_state *crtc_state,
			     struct drm_plane_state *plane_state)
{
	struct mipi_dbi_dev *dbidev = drm_to_mipi_dbi_dev(pipe->crtc.dev);
	struct mipi_dbi *dbi = &dbidev->dbi;
	u8 addr_mode;
	int ret, idx;

	if (!drm_dev_enter(pipe->crtc.dev, &idx))
		return;

	DRM_DEBUG_KMS("\n");

	ret = mipi_dbi_poweron_reset(dbidev);
	if (ret)
		goto out_exit;

	msleep(150);

	switch (dbidev->rotation) {
	default:
		addr_mode = ST7789V_MADCTL_MX | ST7789V_MADCTL_MY;
		break;
	case 90:
		addr_mode = ST7789V_MADCTL_MY | ST7789V_MADCTL_MV;
		break;
	case 180:
		addr_mode = 0;
		break;
	case 270:
		addr_mode = ST7789V_MADCTL_MX | ST7789V_MADCTL_MV;
		break;
	}

	mipi_dbi_command(dbi, MIPI_DCS_SET_ADDRESS_MODE, addr_mode);
	
	mipi_dbi_command(dbi, MIPI_DCS_SET_PIXEL_FORMAT, MIPI_DCS_PIXEL_FMT_16BIT);

	mipi_dbi_command(dbi, ST7789V_PORCTRL_CMD, 0xb, 0xb, 0, 0x33, 0x35);

	mipi_dbi_command(dbi, ST7789V_GCTRL_CMD, 0x11);

	mipi_dbi_command(dbi, ST7789V_VCOMS_CMD, 0x35);

	mipi_dbi_command(dbi, ST7789V_LCMCTRL_CMD, 0x2c);

	mipi_dbi_command(dbi, ST7789V_VDVVRHEN_CMD, 0x01);

	mipi_dbi_command(dbi, ST7789V_VRHS_CMD, 0xd);

	mipi_dbi_command(dbi, ST7789V_VDVS_CMD, 0x20);

	mipi_dbi_command(dbi, ST7789V_FRCTRL2_CMD, 0x13);

	mipi_dbi_command(dbi, ST7789V_PWCTRL1_CMD, 0xa4, 0xa1);

	mipi_dbi_command(dbi, 0xd6, 0xa1);

	mipi_dbi_command(dbi, ST7789V_PVGAMCTRL_CMD,
				0xf0, 0x6, 0xb, 0xa, 0x9, 0x26,
				0x29, 0x33, 0x41, 0x18, 0x16, 0x15,
				0x29, 0x2d);

	mipi_dbi_command(dbi, ST7789V_NVGAMCTRL_CMD,
				0xf0, 0x4, 0x8, 0x8, 0x7, 0x3,
				0x28, 0x32, 0x40, 0x3b, 0x19, 0x18,
				0x2a, 0x2e);

	mipi_dbi_command(dbi, MIPI_DCS_ENTER_INVERT_MODE);

	mipi_dbi_command(dbi, MIPI_DCS_EXIT_SLEEP_MODE);
	/* We need to wait 120ms after a sleep out command */
	msleep(120);

	mipi_dbi_command(dbi, MIPI_DCS_SET_DISPLAY_ON);
	msleep(20);

	mipi_dbi_enable_flush(dbidev, crtc_state, plane_state);
out_exit:
	drm_dev_exit(idx);
}

static const struct drm_simple_display_pipe_funcs st7789v_pipe_funcs = {
	.mode_valid = mipi_dbi_pipe_mode_valid,
	.enable = st7789v_enable,
	.disable = mipi_dbi_pipe_disable,
	.update = mipi_dbi_pipe_update,
};

static const struct drm_display_mode st7789v_mode = {
	DRM_SIMPLE_MODE(240, 320, 37, 49),
};

DEFINE_DRM_GEM_DMA_FOPS(st7789v_fops);

static const struct drm_driver st7789v_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops			= &st7789v_fops,
	DRM_GEM_DMA_DRIVER_OPS_VMAP,
	.debugfs_init		= mipi_dbi_debugfs_init,
	.name			= "st7789v",
	.desc			= "Sitronix ST7789V",
	.date			= "20250823",
	.major			= 1,
	.minor			= 0,
};

static const struct of_device_id st7789v_of_match[] = {
	{ .compatible = "waveshare,capacitive-lcd-28" },
	{ .compatible = "sitronix,st7789v" },
	{ }
};
MODULE_DEVICE_TABLE(of, st7789v_of_match);

static const struct spi_device_id st7789v_id[] = {
	{ "capacitive-lcd-28", 0 },
	{ "st7789v", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, st7789v_id);

static int st7789v_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct mipi_dbi_dev *dbidev;
	struct drm_device *drm;
	struct mipi_dbi *dbi;
	struct gpio_desc *dc;
	u32 rotation = 0;
	int ret;

	dbidev = devm_drm_dev_alloc(dev, &st7789v_driver,
				    struct mipi_dbi_dev, drm);
	if (IS_ERR(dbidev))
		return PTR_ERR(dbidev);

	dbi = &dbidev->dbi;
	drm = &dbidev->drm;

	dbi->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(dbi->reset))
		return dev_err_probe(dev, PTR_ERR(dbi->reset), "Failed to get GPIO 'reset'\n");

	dc = devm_gpiod_get_optional(dev, "dc", GPIOD_OUT_LOW);
	if (IS_ERR(dc))
		return dev_err_probe(dev, PTR_ERR(dc), "Failed to get GPIO 'dc'\n");

	dbidev->backlight = devm_of_find_backlight(dev);
	if (IS_ERR(dbidev->backlight))
		return PTR_ERR(dbidev->backlight);

	device_property_read_u32(dev, "rotation", &rotation);

	ret = mipi_dbi_spi_init(spi, dbi, dc);
	if (ret)
		return ret;

	ret = mipi_dbi_dev_init(dbidev, &st7789v_pipe_funcs, &st7789v_mode, rotation);
	if (ret)
		return ret;

	drm_mode_config_reset(drm);

	ret = drm_dev_register(drm, 0);
	if (ret)
		return ret;

	spi_set_drvdata(spi, drm);

	drm_fbdev_generic_setup(drm, 0);

	return 0;
}

static void st7789v_remove(struct spi_device *spi)
{
	struct drm_device *drm = spi_get_drvdata(spi);

	drm_dev_unplug(drm);
	drm_atomic_helper_shutdown(drm);
}

static void st7789v_shutdown(struct spi_device *spi)
{
	drm_atomic_helper_shutdown(spi_get_drvdata(spi));
}

static struct spi_driver st7789v_spi_driver = {
	.driver = {
		.name = "st7789v",
		.of_match_table = st7789v_of_match,
	},
	.id_table = st7789v_id,
	.probe = st7789v_probe,
	.remove = st7789v_remove,
	.shutdown = st7789v_shutdown,
};
module_spi_driver(st7789v_spi_driver);

MODULE_DESCRIPTION("Sitronix ST7789V DRM driver");
MODULE_AUTHOR("Nithin Manne <nithinmanne@gmail.com>");
MODULE_LICENSE("GPL v2");
