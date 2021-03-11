#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

static int bmp280_spi_open(struct inode *pinode, struct file *pfile){
    printk(KERN_NOTICE "BMP280_spi opened\n");
    return 0;
}

static int bmp280_spi_close(struct inode *inode, struct file *pfile){
    printk(KERN_NOTICE "BMP280_spi closed\n");
    return 0;
}

static struct file_operations bmp280_spi_fops = {
    .owner = THIS_MODULE,
    .open = bmp280_spi_open,
    .release = bmp280_spi_close,
};

static int bmp280_spi_init(void){
    printk(KERN_NOTICE "BMP280 - Hello Kernel\n");
    register_chrdev(90, "bmp280_spi drv", &bmp280_spi_fops);
    return 0;
}

static void bmp280_spi_exit(void){
    printk(KERN_NOTICE "BMP280 - Goodbye Kernel\n");
    unregister_chrdev(90, "bmp280_spi drv");
}

MODULE_LICENSE("GPL");

module_init(bmp280_spi_init);
module_exit(bmp280_spi_exit);