#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "bmp280-spi-drv"
#define DRIVER_CLASS "bmp280-spi-class"

static dev_t device_nbr;
static struct class *device_class;
static struct cdev c_device;

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

static struct file_operations bmp280_spi_fops = {
	.owner = THIS_MODULE,
	.open = bmp280_spi_open,
	.release = bmp280_spi_close,
};

static int bmp280_spi_init(void)
{
	printk(KERN_INFO "%s - Initialization started.\n", DRIVER_NAME);

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

	/* Registering device to kernel */
	if (cdev_add(&c_device, device_nbr, 1) == -1) {
		printk("%s - Registering of device to kernel failed.\n",
		       DRIVER_NAME);
		goto AddError;
	}
	printk(KERN_INFO "%s - Initialization complete.\n", DRIVER_NAME);

	return 0;

AddError:
	device_destroy(device_class, device_nbr);
FileError:
	class_destroy(device_class);
ClassError:
	unregister_chrdev_region(device_nbr, 1);
	return -1;
}

static void bmp280_spi_exit(void)
{
	cdev_del(&c_device);
	device_destroy(device_class, device_nbr);
	class_destroy(device_class);
	unregister_chrdev_region(device_nbr, 1);
	printk(KERN_INFO "%s - Unregistered.\n", DRIVER_NAME);
}

MODULE_LICENSE("GPL");

module_init(bmp280_spi_init);
module_exit(bmp280_spi_exit);