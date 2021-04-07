#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#define DRIVER_NAME "bmp280-spi-drv"
#define DRIVER_CLASS "bmp280-spi-class"
#define SPI_BUS_NUM 0

static dev_t device_nbr;
static struct class *device_class;
static struct cdev c_device;
static struct spi_device *bmp280_dev;

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

static int __init bmp280_spi_init(void)
{
	printk(KERN_INFO "%s - Initialization started.\n", DRIVER_NAME);
	if (setup_spi() == -1)
		return -1;

	if (setup_ch_dev() == -1)
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
