
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
//#include <linux/leds.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include <linux/fb.h>
#include <linux/uaccess.h>

#include "ili9341.h"


#define DRIVER_NAME			"lcd_ili9341_drv"


static struct device *dev;

static const struct of_device_id my_drvr_match[];



typedef enum {
	LCD_Landscape,
	LCD_Portrait
} LCD_Orientation;


struct lcd_data {
	struct spi_device *spi_device;

	struct fb_info *info;

	struct class *sys_class;
	struct gpio_desc *RST_gpiod;
	struct gpio_desc *DC_gpiod;
	struct gpio_desc *CS_gpiod;

	struct work_struct display_update_ws;

	LCD_Orientation orientation;

	//u16 *lcd_vmem;

	u16 width;
	u16 height;
	u8 RST_state;
	u8 DC_state;
	u8 CS_state;

};


static struct lcd_data *lcd;

static u16 *lcd_vmem;

static uint32_t pseudo_palette[16];

static struct fb_fix_screeninfo ili9341fb_fix = {
	.id 			= "Ilitek ili9341",
	.type       	= FB_TYPE_PACKED_PIXELS,
	.visual     	= FB_VISUAL_TRUECOLOR,
	.xpanstep   	= 0,
	.ypanstep   	= 0,
	.ywrapstep  	= 0,
	.line_length    = (LCD_WIDTH * BPP) / 8,
	.accel      	= FB_ACCEL_NONE,
};


static struct fb_var_screeninfo ili9341fb_var = {
		.xres           = LCD_WIDTH,
		.yres           = LCD_HEIGHT,
		.xres_virtual   = LCD_WIDTH,
		.yres_virtual   = LCD_HEIGHT,
		.height         = -1,
		.width          = -1,
		.bits_per_pixel = BPP,
		
		.red            = { 11, 5, 0 },
		.green          = { 5, 6, 0 },
		.blue           = { 0, 5, 0 },
		.nonstd         = 0,
};





static void UtilWaitDelay(u32 delay)
{

}



static void CS_Force(u8 state)
{
	gpiod_set_value(lcd->CS_gpiod, state);

	UtilWaitDelay(0x0000000F);
	//dev_info(dev, "%s (%d)\n", __FUNCTION__, state);
}


static void RST_Force(u8 state)
{
	gpiod_set_value(lcd->RST_gpiod, state);

	UtilWaitDelay(0x0000000F);
	//dev_info(dev, "%s (%d)\n", __FUNCTION__, state);
}


static void DC_Force(u8 state)
{
	gpiod_set_value(lcd->DC_gpiod, state);

	UtilWaitDelay(0x0000000F);
	//dev_info(dev, "%s (%d)\n", __FUNCTION__, state);
}


#define TFT_DC_SET		DC_Force(1);
#define TFT_DC_RESET	DC_Force(0);

#define TFT_RST_SET		RST_Force(1);
#define TFT_RST_RESET	RST_Force(0);

#define TFT_CS_SET		CS_Force(1);
#define TFT_CS_RESET	CS_Force(0);



static void SPI_Send_byte(u8 byte)
{
	u8 bufff[2] = {0};
	int ret;

	struct spi_device *spi = lcd->spi_device;

	bufff[0] = byte;

	ret = spi_write(spi, &bufff, 1);//sizeof(bufff));
	if(ret < 0)
		dev_err(dev, "failed to write to SPI\n");
}

static void SPI_Send_two_bytes(u8 byte1, u8 byte2)
{
	u8 bufff[3] = {0};
	int ret;

	struct spi_device *spi = lcd->spi_device;

	bufff[0] = byte1;
	bufff[1] = byte2;

	ret = spi_write(spi, &bufff, 2);//sizeof(bufff));
	if(ret < 0)
		dev_err(dev, "failed to write to SPI\n");
}



static void SPI_Send_buff(u8 *buff, u16 len)
{
	int ret;

	struct spi_device *spi = lcd->spi_device;

	ret = spi_write(spi, buff, len);
	if(ret < 0)
		dev_err(dev, "failed to write to SPI\n");
}


static void LCD_SendCommand(u8 com) {
	TFT_DC_RESET;
	TFT_CS_RESET;
	SPI_Send_byte(com);
	TFT_CS_SET;
}


static void LCD_SendData(u8 data) {
	TFT_DC_SET;
	TFT_CS_RESET;
	SPI_Send_byte(data);
	TFT_CS_SET;
}

static void LCD_SendDataW(u8 data1, u8 data2) {
	TFT_DC_SET;
	TFT_CS_RESET;
	SPI_Send_two_bytes(data1, data2);
	TFT_CS_SET;
}


static void LCD_SetCursorPosition(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {

	LCD_SendCommand(ILI9341_COLUMN_ADDR);
	LCD_SendDataW(x1 >> 8, x1 & 0xFF);
	LCD_SendDataW(x2 >> 8, x2 & 0xFF);

	LCD_SendCommand(ILI9341_PAGE_ADDR);
	LCD_SendDataW(y1 >> 8, y1 & 0xFF);
	LCD_SendDataW(y2 >> 8, y2 & 0xFF);
}




static void LCD_Rotate(LCD_Orientation orientation) {

	LCD_SendCommand(ILI9341_MAC);

/*	if (orientation == LCD_Orientation_Portrait_1) {
		LCD_SendData(0x58);
	} else if (orientation == LCD_Orientation_Portrait_2) {
		LCD_SendData(0x88);
	} else if (orientation == LCD_Orientation_Landscape_1) {
		LCD_SendData(0x28);
	} else if (orientation == LCD_Orientation_Landscape_2) {
		LCD_SendData(0xE8);
	}
*/
	if (orientation == LCD_Portrait) {
		LCD_SendData(0x58);
	}
	else if (orientation == LCD_Landscape) {
		LCD_SendData(0x28);
	}

	if (orientation == LCD_Portrait) {
		lcd->width 			= LCD_WIDTH;
		lcd->height 		= LCD_HEIGHT;
		lcd->orientation 	= LCD_Portrait;
	} else {
		lcd->width 			= LCD_HEIGHT;
		lcd->height 		= LCD_WIDTH;
		lcd->orientation 	= LCD_Landscape;
	}
}


static void LCD_Init(void){

	TFT_CS_SET;

	// LCD reset
	//mdelay(20);
	TFT_RST_SET;

	TFT_CS_RESET;
	LCD_SendCommand(ILI9341_RESET);
	//mdelay(20);

	/// LCD init
	LCD_SendCommand(ILI9341_POWERA);
	LCD_SendCommand(ILI9341_POWERA);
	LCD_SendData(0x39);
	LCD_SendData(0x2C);
	LCD_SendData(0x00);
	LCD_SendData(0x34);
	LCD_SendData(0x02);
	LCD_SendCommand(ILI9341_POWERB);
	LCD_SendData(0x00);
	LCD_SendData(0xC1);
	LCD_SendData(0x30);
	LCD_SendCommand(ILI9341_DTCA);
	LCD_SendData(0x85);
	LCD_SendData(0x00);
	LCD_SendData(0x78);
	LCD_SendCommand(ILI9341_DTCB);
	LCD_SendData(0x00);
	LCD_SendData(0x00);
	LCD_SendCommand(ILI9341_POWER_SEQ);
	LCD_SendData(0x64);
	LCD_SendData(0x03);
	LCD_SendData(0x12);
	LCD_SendData(0x81);
	LCD_SendCommand(ILI9341_PRC);
	LCD_SendData(0x20);
	LCD_SendCommand(ILI9341_POWER1);
	LCD_SendData(0x23);
	LCD_SendCommand(ILI9341_POWER2);
	LCD_SendData(0x10);
	LCD_SendCommand(ILI9341_VCOM1);
	LCD_SendData(0x3E);
	LCD_SendData(0x28);
	LCD_SendCommand(ILI9341_VCOM2);
	LCD_SendData(0x86);


	//LCD_Rotate(LCD_Landscape);
	LCD_Rotate(LCD_Portrait);

	LCD_SendCommand(ILI9341_PIXEL_FORMAT);
	LCD_SendData(0x55);
	LCD_SendCommand(ILI9341_FRC);
	LCD_SendData(0x00);
	LCD_SendData(0x18);
	LCD_SendCommand(ILI9341_DFC);
	LCD_SendData(0x08);
	LCD_SendData(0x82);
	LCD_SendData(0x27);
	LCD_SendCommand(ILI9341_3GAMMA_EN);
	LCD_SendData(0x00);

	LCD_SetCursorPosition(0, 0, lcd->width - 1, lcd->height - 1);

	LCD_SendCommand(ILI9341_GAMMA);
	LCD_SendData(0x01);
	LCD_SendCommand(ILI9341_PGAMMA);
	LCD_SendData(0x0F);
	LCD_SendData(0x31);
	LCD_SendData(0x2B);
	LCD_SendData(0x0C);
	LCD_SendData(0x0E);
	LCD_SendData(0x08);
	LCD_SendData(0x4E);
	LCD_SendData(0xF1);
	LCD_SendData(0x37);
	LCD_SendData(0x07);
	LCD_SendData(0x10);
	LCD_SendData(0x03);
	LCD_SendData(0x0E);
	LCD_SendData(0x09);
	LCD_SendData(0x00);
	LCD_SendCommand(ILI9341_NGAMMA);
	LCD_SendData(0x00);
	LCD_SendData(0x0E);
	LCD_SendData(0x14);
	LCD_SendData(0x03);
	LCD_SendData(0x11);
	LCD_SendData(0x07);
	LCD_SendData(0x31);
	LCD_SendData(0xC1);
	LCD_SendData(0x48);
	LCD_SendData(0x08);
	LCD_SendData(0x0F);
	LCD_SendData(0x0C);
	LCD_SendData(0x31);
	LCD_SendData(0x36);
	LCD_SendData(0x0F);
	LCD_SendCommand(ILI9341_SLEEP_OUT);

	//Delay(100);
	LCD_SendCommand(ILI9341_DISPLAY_ON);
	LCD_SendCommand(ILI9341_GRAM);
}




#if 0
static void LCD_Fill(uint16_t color) {
	unsigned int n, i, j;
	i = color >> 8;
	j = color & 0xFF;
	LCD_SetCursorPosition(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

	LCD_SendCommand(ILI9341_GRAM);

	TFT_DC_SET;
	for (n = 0; n < LCD_PIXEL_COUNT; n++) {
		//SPI_Send_byte(i);
		//SPI_Send_byte(j);
		SPI_Send_two_bytes(i, j);
	}
}
#endif


static void ili9341_UpdateScreen(struct lcd_data *lcd){
	unsigned int n;
	u16 *pcolor = 0;
	u8 buff[LCD_WIDTH * 2] = {0};
	u8 *psrc = 0;

	LCD_SetCursorPosition(0, 0, lcd->width - 1, lcd->height - 1);

	LCD_SendCommand(ILI9341_GRAM);

	TFT_DC_SET;
	TFT_CS_RESET;

	pcolor = lcd_vmem;
	psrc = (u8 *)lcd_vmem;
	for (n = 0; n < LCD_HEIGHT; n++) {
		memcpy(buff, psrc, LCD_WIDTH *2);
		psrc += LCD_WIDTH*2;
		SPI_Send_buff(buff, LCD_WIDTH *2);
	}


	TFT_CS_SET;
}




static void update_display_work(struct work_struct *work)
{
	struct lcd_data *lcd =
		container_of(work, struct lcd_data, display_update_ws);
	//ssd1307fb_update_display(lcd);
	ili9341_UpdateScreen(lcd);
}


static ssize_t sys_lcd_clear(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;

	i += sprintf(buf, "sys_lcd_clear\n");

	memset(lcd_vmem, 0x0000, lcd->height * lcd->width*2);

	ili9341_UpdateScreen(lcd);

/*
	LCD_SendCommand(0x81);
	LCD_SendCommand(0xF0);
	LCD_SendData(0x0F);
	LCD_SendData(0x18);
*/

	return i;
}


static ssize_t sys_lcd_paint(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t i = 0;
	long indx = 0, dx = 0;
	u16 *pmem = 0;
	u16 *pmem2 = 0;
	u16 color = 0xF800;

	i += sprintf(buf, "sys_lcd_paint\n");
	// http://www.barth-dev.de/online/rgb565-color-picker/

	pmem = lcd_vmem;

	for(indx = 0; indx < lcd->height/4; indx++){
		color++;
		pmem += lcd->width;
		pmem2 = pmem;
		for(dx = 0; dx < lcd->width/4; dx++){
			*pmem2++ = color;
		}
	}

	ili9341_UpdateScreen(lcd);
/*
	LCD_SendCommand(0x55);
	LCD_SendCommand(0x5A);
	LCD_SendData(0xAA);
	LCD_SendData(0xA5);
*/
	return i;
}


/* <linux/device.h>
* #define CLASS_ATTR(_name, _mode, _show, _store) \
* struct class_attribute class_attr_##_name = __ATTR(_name, _mode,
* _show, _store)
*/
CLASS_ATTR(clear, 0664, &sys_lcd_clear, NULL);
CLASS_ATTR(paint, 0664, &sys_lcd_paint, NULL);


static void make_sysfs_entry(struct spi_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const char *name;
	int res;

	struct class *sys_class;

	if (np) {

		if (!of_property_read_string(np, "label", &name))
			dev_info(dev, "label = %s\n", name);


		sys_class = class_create(THIS_MODULE, name);

		if (IS_ERR(sys_class)){
			dev_err(dev, "bad class create\n");
		}
		else{
			res = class_create_file(sys_class, &class_attr_clear);
			res = class_create_file(sys_class, &class_attr_paint);

			lcd->sys_class = sys_class;


			dev_info(dev, "sys class created = %s\n", name);
		}
	}

}




static int get_platform_info(struct spi_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const char *name;

	int gpiod_cnt;

	if (np) {

		if (!of_property_read_string(np, "label", &name))
			dev_info(dev, "label = %s\n", name);

		if (np->name)
			dev_info(dev, "np->name = %s\n", np->name);

		if (np->parent)
			dev_info(dev, "np->parent->name = %s\n", np->parent->name);

		if (np->child)
			dev_info(dev, "np->child->name = %s\n", np->child->name);



		gpiod_cnt = gpiod_count(dev, "reset");
		dev_info(dev, "gpiod_cnt = %d\n", gpiod_cnt);

		lcd->CS_gpiod = devm_gpiod_get(dev, "cs", GPIOD_OUT_HIGH);
		if (IS_ERR(lcd->CS_gpiod)) {
			dev_err(dev, "fail to get reset-gpios()\n");
			return EINVAL;
		}
		if(!gpiod_direction_output(lcd->CS_gpiod, 1))
			dev_info(dev, "cs-gpios set as OUT\n");



		lcd->RST_gpiod = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(lcd->RST_gpiod)) {
			dev_err(dev, "fail to get reset-gpios()\n");
			return EINVAL;
		}
		if(!gpiod_direction_output(lcd->RST_gpiod, 1))
			dev_info(dev, "reset-gpios set as OUT\n");


		lcd->DC_gpiod = devm_gpiod_get(dev, "dc", GPIOD_OUT_HIGH);
		if (IS_ERR(lcd->DC_gpiod)) {
			dev_err(dev, "fail to get dc-gpios()\n");
			return EINVAL;
		}
		if(!gpiod_direction_output(lcd->DC_gpiod, 1))
			dev_info(dev, "dc-gpios set as OUT\n");
		
		return 1;
	}
	else{
		dev_err(dev, "failed to get device_node\n");
		return -EINVAL;
	}

	return 0;
}



static int ili9341fb_setcolreg (u_int regno,
        u_int red, u_int green, u_int blue,
        u_int transp, struct fb_info *info)
{
	u32 *palette = info->pseudo_palette;

	if (regno >= 16)
			return -EINVAL;

	/* only FB_VISUAL_TRUECOLOR supported */

	red >>= 16 - info->var.red.length;
	green >>= 16 - info->var.green.length;
	blue >>= 16 - info->var.blue.length;
	transp >>= 16 - info->var.transp.length;
	
	palette[regno] = (red << info->var.red.offset) 		|
					(green << info->var.green.offset) 	|
					(blue << info->var.blue.offset) 	|
					(transp << info->var.transp.offset);
	return 0;
}


static ssize_t ili9341fb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct lcd_data *lcd = info->par;
	unsigned long total_size;
	unsigned long p = *ppos;
	u8 __iomem *dst;

	total_size = info->fix.smem_len;

	//dev_info(dev, "%s\n", __FUNCTION__);

	if (p > total_size)
		return -EINVAL;

	if (count + p > total_size)
		count = total_size - p;

	if (!count)
		return -EINVAL;

	dst = (void __force *) (info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		return -EFAULT;

	//ili9341_UpdateScreen(lcd);
	schedule_work(&lcd->display_update_ws);

	*ppos += count;

	return count;
}


static int ili9341fb_blank(int blank_mode, struct fb_info *info)
{
	return 0;
}

static void ili9341fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct lcd_data *lcd = info->par;
	sys_fillrect(info, rect);
	schedule_work(&lcd->display_update_ws);

	//dev_info(dev, "%s\n", __FUNCTION__);
}

static void ili9341fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct lcd_data *lcd = info->par;
	sys_copyarea(info, area);
	schedule_work(&lcd->display_update_ws);

	//dev_info(dev, "%s\n", __FUNCTION__);
}

static void ili9341fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	sys_imageblit(info, image);
	schedule_work(&lcd->display_update_ws);

	//dev_info(dev, "%s\n", __FUNCTION__);
}

static struct fb_ops ili9341fb_ops = {
	.owner          = THIS_MODULE,
	.fb_setcolreg   = ili9341fb_setcolreg,
	.fb_read        = fb_sys_read,
	.fb_write       = ili9341fb_write,
	.fb_blank       = ili9341fb_blank,
	.fb_fillrect    = ili9341fb_fillrect,
	.fb_copyarea    = ili9341fb_copyarea,
	.fb_imageblit   = ili9341fb_imageblit,
};


static int ili9341_probe(struct spi_device *spi)
{
	const struct of_device_id *match;
	struct fb_info *info;
	u32 vmem_size;
	int ret;

	dev = &spi->dev;

	match = of_match_device(of_match_ptr(my_drvr_match), dev);
	if (!match) {
		dev_err(dev, "failed of_match_device()\n");
		return -EINVAL;
	}


	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi_setup(spi);


	info = framebuffer_alloc(sizeof(struct lcd_data), &spi->dev);

	if (!info){
		dev_err(dev, "framebuffer_alloc has failed\n");
		return -ENOMEM;
	}


	lcd 				= info->par;
	lcd->spi_device 	= spi;

	lcd->width  		= LCD_WIDTH;
	lcd->height 		= LCD_HEIGHT;


	vmem_size = (lcd->width * lcd->height * BPP) / 8;


	lcd_vmem = devm_kzalloc(dev, vmem_size, GFP_KERNEL);

	if (!lcd_vmem) {
		dev_err(dev, "Couldn't allocate graphical memory.\n");
		return -ENOMEM;
	}


	info->fbops     		= &ili9341fb_ops;
	info->fix       		= ili9341fb_fix;
	info->fix.line_length 	= (lcd->width * BPP) / 8;

	info->var 				= ili9341fb_var;

	info->var.xres 			= lcd->width;
	info->var.xres_virtual 	= lcd->width;
	info->var.yres 			= lcd->height;
	info->var.yres_virtual 	= lcd->height;

	info->screen_base = (u8 __force __iomem *)lcd_vmem;

	info->fix.smem_start = __pa(lcd_vmem);
	info->fix.smem_len = vmem_size;
	info->flags = FBINFO_FLAG_DEFAULT;

	info->pseudo_palette = pseudo_palette;

	lcd->info 			= info;


	dev_info(dev, "Hello, world!\n");
	dev_info(dev, "spiclk %u KHz.\n",	(spi->max_speed_hz + 500) / 1000);


	if(!get_platform_info(spi)){
		dev_err(dev, "failed to get platform info\n");
		return -EINVAL;
	}


	LCD_Init();
	INIT_WORK(&lcd->display_update_ws, update_display_work);

	spi_set_drvdata(spi, lcd);

	make_sysfs_entry(spi);



	ret = register_framebuffer(info);
	if (ret) {
		dev_err(dev, "Couldn't register the framebuffer\n");
		return ret;
	}


	dev_info(dev, "module initialized\n");
	return 0;
}



static int ili9341_remove(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct class *sys_class;
	struct fb_info *info = lcd->info;

	sys_class = lcd->sys_class;

	cancel_work_sync(&lcd->display_update_ws);

	unregister_framebuffer(info);

	class_remove_file(sys_class, &class_attr_clear);
	class_remove_file(sys_class, &class_attr_paint);
	class_destroy(sys_class);

	dev_info(dev, "Goodbye, world!\n");
	return 0;
}





static const struct of_device_id my_drvr_match[] = {
	{ .compatible = "DAndy,lcd_ili9341_drv", },
	{ },
};
MODULE_DEVICE_TABLE(of, my_drvr_match);



static const struct spi_device_id ili9341_spi_ids[] = {
	{ "lcd_ili9341_drv", 0 },
	{}
};
MODULE_DEVICE_TABLE(spi, ili9341_spi_ids);
 

static struct spi_driver my_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = my_drvr_match,
	},
	.probe 		= ili9341_probe,
	.remove 	= ili9341_remove,
	.id_table 	= ili9341_spi_ids,
};
module_spi_driver(my_driver);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Devyatov Andrey <devyatov.andey@gmail.com>");