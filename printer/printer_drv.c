#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

// printer 相关寄存器物理地址
#define GPBCON  0x56000010
#define GPBDAT  0x56000014

// 主设备号
int major;
// 设备相关变量
static struct class *printer_drv_class;
static struct class_device *printer_drv_dev;
// printer 相关寄存器变量
volatile unsigned long *gpbcon = NULL;
volatile unsigned long *gpbdat = NULL;

static int printer_drv_open(struct inode *inode, struct file *file)
{
    // 配置 printer 相关引脚为输出模式
    *gpbcon &= ~(0x3<<(7*2));
    *gpbcon |= (0x1<<(7*2));

    // printk("Printer driver opened!\n");

    return 0;
}

static ssize_t printer_drv_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
    unsigned char val;

    if (count != 1)
    {
        printk("Beep write error: count must be 1!\n");
        return -EINVAL;
    }

    copy_from_user(&val, buffer, 1);
    if (val == 0)
    {
        *gpbdat &= ~(1<<7);
    }
    else
    {
        *gpbdat |= 1<<7;
    }
    
}

static struct file_operations printer_drv_fops = {
    .owner  =   THIS_MODULE,
    .open   =   printer_drv_open, 
    .write  =   printer_drv_write,
};

static int printer_drv_init(void)
{
    major = register_chrdev(0, "printer_drv", &printer_drv_fops);

    printer_drv_class = class_create(THIS_MODULE, "printer_drv");
    printer_drv_dev = class_device_create(printer_drv_class, NULL, 
                            MKDEV(major, 0), NULL, "printer");

    gpbcon = (volatile unsigned long *)ioremap(GPBCON, 1);
    gpbdat = (volatile unsigned long *)ioremap(GPBDAT, 1);

    printk("Printer module loaded!\n");

    return 0;
}

static void printer_drv_exit(void)
{
    unregister_chrdev(major, "printer_drv");

    class_device_unregister(printer_drv_dev);
    class_destroy(printer_drv_class);

    iounmap(gpbcon);
    iounmap(gpbdat);

    // printk("Beep driver closed!\n");
}

module_init(printer_drv_init);
module_exit(printer_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fox_Benjiaming");
