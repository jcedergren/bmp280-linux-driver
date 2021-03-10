#include <linux/init.h>
#include <linux/module.h>


static int bmp280_spi_init(void){
    printk(KERN_NOTICE "BMP280 - Hello Kernel\n");
    return 0;
}

static void bmp280_spi_exit(void){
    printk(KERN_NOTICE "BMP280 - Goodbye Kernel\n");
}

MODULE_LICENSE("GPL");

module_init(bmp280_spi_init);
module_exit(bmp280_spi_exit);