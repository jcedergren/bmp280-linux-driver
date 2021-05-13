#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#define DRIVER_NAME "bmp280-spi-drv"
#define DRIVER_CLASS "bmp280-spi-class"
#define SPI_BUS_NUM 0

#define T_STANDBY_1S 0b101
#define OSRS_T_16 0b101
#define OSRS_P_16 0b101
#define MODE_NORMAL 0b11
#define ADR_CONFIG 0xF5
#define ADR_CTRL_MEAS 0xF4

static dev_t device_nbr;
static struct class *device_class;
static struct cdev c_device;
static struct spi_device *bmp280_dev;
static s32 dig_T1, dig_T2, dig_T3;
static s32 t_fine;

static int bmp280_spi_open(struct inode *pinode, struct file *pfile)
{
	printk(KERN_NOTICE "BMP280_spi opened\n");
	return 0;
}

static int bmp280_spi_close(struct inode *inode, struct file *pfile)
{
	printk(KERN_NOTICE "BMP280_spi closed\n");
	return 0;
}

static s32 calibration_compensation_temp(s32 adc_T)
{
	s32 var1, var2, T;
	var1 = ((((adc_T >> 3) - (dig_T1 << 1))) * (dig_T2)) >> 11;
	var2 = (((((adc_T >> 4) - (dig_T1)) * ((adc_T >> 4) - (dig_T1))) >>
		 12) *
		(dig_T3)) >>
	       14;
	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	return T;
}

static s32 read_raw_adc_temp(void)
{
	s32 temp_msb, temp_lsb, temp_xlsb;

	temp_msb = spi_w8r8(bmp280_dev, 0xFA);
	temp_lsb = spi_w8r8(bmp280_dev, 0xFB);
	temp_xlsb = spi_w8r8(bmp280_dev, 0xFC);

	return ((temp_msb << 16) | (temp_lsb << 8) | temp_xlsb) >> 4;
}

static int bmp280_spi_read(struct file *pfile, char __user *user_buffer,
			   size_t len, loff_t *offset)
{
	int bytes_to_copy, bytes_not_copied;
	int temperature;
	char output_string[20];

	memset(output_string, 0, sizeof(output_string));
	temperature = 12345;

	if (sizeof(output_string) <= *offset) {
		return 0;
	}

	bytes_to_copy = min(sizeof(output_string - *offset), len);

	snprintf(output_string, sizeof(output_string), "%d\n", temperature);

	bytes_not_copied = copy_to_user(user_buffer, output_string + *offset,
					bytes_to_copy);

	*offset += bytes_to_copy;

	printk(KERN_NOTICE "BMP280_spi read\n");
	return bytes_to_copy;
}

static struct file_operations bmp280_spi_fops = {
	.owner = THIS_MODULE,
	.open = bmp280_spi_open,
	.release = bmp280_spi_close,
	.read = bmp280_spi_read,
};

static int __init setup_spi(void)
{
	struct spi_master *master;

	struct spi_board_info spi_device_info = {
		.modalias = "bmp280",
		.max_speed_hz = 1000000,
		.bus_num = SPI_BUS_NUM,
		.chip_select = 0,
		.mode = 3,
	};

	master = spi_busnum_to_master(SPI_BUS_NUM);

	if (!master) {
		printk("%s - There is no spi bus with nbr %d\n", DRIVER_NAME,
		       SPI_BUS_NUM);
		return -1;
	}

	bmp280_dev = spi_new_device(master, &spi_device_info);
	if (!bmp280_dev) {
		printk("%s - Cannot create spi device\n", DRIVER_NAME);
		return -1;
	}
	bmp280_dev->bits_per_word = 8;

	if (spi_setup(bmp280_dev) != 0) {
		printk("%s - Cannot change spi bus setup\n", DRIVER_NAME);
		spi_unregister_device(bmp280_dev);
		return -1;
	}

	return 0;
}

static int __init setup_ch_dev(void)
{
	if (alloc_chrdev_region(&device_nbr, 0, 1, DRIVER_NAME) < 0) {
		printk("%s - Device nbr could not be allocated.\n",
		       DRIVER_NAME);
		return -1;
	}
	printk("%s - Device nbr. registered, Major: %d, Minor: %d.\n",
	       DRIVER_NAME, device_nbr >> 20, device_nbr && 0xfffff);

	if ((device_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
		printk("%s - Device class cannot be created\n", DRIVER_NAME);
		goto ClassError;
	}

	if (device_create(device_class, NULL, device_nbr, NULL, DRIVER_NAME) ==
	    NULL) {
		printk("%s - Cannot create device file.\n", DRIVER_NAME);
		goto FileError;
	}
	cdev_init(&c_device, &bmp280_spi_fops);

	if (cdev_add(&c_device, device_nbr, 1) == -1) {
		printk("%s - Registering of device to kernel failed.\n",
		       DRIVER_NAME);
		goto AddError;
	}
	return 0;

AddError:
	device_destroy(device_class, device_nbr);
FileError:
	class_destroy(device_class);
ClassError:
	unregister_chrdev_region(device_nbr, 1);
	return -1;
}

static int __init setup_sensor(void)
{
	u8 conf_msg[2];
	u8 ctrl_meas_msg[2];

	conf_msg[0] = ADR_CONFIG & ~(1 << 7);
	conf_msg[1] = (T_STANDBY_1S << 5);
	ctrl_meas_msg[0] = ADR_CTRL_MEAS & ~(1 << 7);
	ctrl_meas_msg[1] =
		(OSRS_T_16 << 5) | (OSRS_P_16 << 2) | (MODE_NORMAL << 0);

	spi_write(bmp280_dev, conf_msg, sizeof(conf_msg));
	spi_write(bmp280_dev, ctrl_meas_msg, sizeof(ctrl_meas_msg));

	dig_T1 = spi_w8r16(bmp280_dev, 0x88);
	dig_T2 = spi_w8r16(bmp280_dev, 0x8a);
	dig_T3 = spi_w8r16(bmp280_dev, 0x8c);

	return 1;
}

static int __init bmp280_spi_init(void)
{
	printk(KERN_INFO "%s - Initialization started.\n", DRIVER_NAME);
	if (setup_spi() == -1)
		return -1;

	if (setup_ch_dev() == -1)
		return -1;

	if (setup_sensor() == -1)
		return -1;
	printk(KERN_INFO "%s - Initialization successful.\n", DRIVER_NAME);

	return 0;
}

static void bmp280_spi_exit(void)
{
	if (bmp280_dev)
		spi_unregister_device(bmp280_dev);

	cdev_del(&c_device);
	device_destroy(device_class, device_nbr);
	class_destroy(device_class);
	unregister_chrdev_region(device_nbr, 1);
	printk(KERN_INFO "%s - Unregistered.\n", DRIVER_NAME);
}

MODULE_LICENSE("GPL");

module_init(bmp280_spi_init);
module_exit(bmp280_spi_exit);
