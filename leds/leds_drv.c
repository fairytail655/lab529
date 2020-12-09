#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

// leds 相关寄存器物理地址
#define GPFCON  0x56000050
#define GPFDAT  0x56000054

// 主设备号
int major;
// 设备相关变量
static struct class *leds_drv_class;
static struct class_device *leds_drv_dev;
// leds 相关寄存器变量
volatile unsigned long *gpfcon = NULL;
volatile unsigned long *gpfdat = NULL;

static int leds_drv_open(struct inode *inode, struct file *file)
{
    // 配置 leds 相关引脚为输出模式
    *gpfcon &= ~((0x3<<(4*2)) | (0x3<<(5*2)) | (0x3<<(6*2)));
    *gpfcon |= ((0x1<<(4*2)) | (0x1<<(5*2)) | (0x1<<(6*2)));

    // printk("Leds driver opened!\n");

    return 0;
}

static ssize_t leds_drv_write(struct file *filp, const char __user *buffer, size_t count, loff_t *ppos)
{
    // 对单个 led 进行操作
    // val[0]: 对哪个 led 进行操作, 0,1,2
    // val[1]: 执行的操作, 1-开启, 0-关闭
    unsigned char vals[2];
    // 对整体 leds 进行操作
    // val 二进制对应位: 1-开启, 0-关闭
    unsigned char val;

    if (count > 2)
    {
        printk("Leds write error: count couldn't more than %d\n", 2);
        return -EINVAL;
    }
    else if (count == 1)
    {
        copy_from_user(&val, buffer, 1);   // user space to kernel space
        // led0
        if (val & 0x1)
        {
            *gpfdat &= ~(1<<4);
        }
        else
        {
            *gpfdat |= (1<<4);
        }
        // led1
        if (val & 0x2)
        {
            *gpfdat &= ~(1<<5);
        }
        else
        {
            *gpfdat |= (1<<5);
        }
        // led2
        if (val & 0x4)
        {
            *gpfdat &= ~(1<<6);
        }
        else
        {
            *gpfdat |= (1<<6);
        }
    }
    else if (count == 2)
    {
        copy_from_user(vals, buffer, 2);   // user space to kernel space
        switch (vals[0])
        {
            // led0
            case 0:
                if (vals[1] == 1)
                {
                    *gpfdat &= ~(1<<4);
                }
                else if (vals[1] == 0)
                {
                    *gpfdat |= (1<<4);
                }
                break;
            // led1
            case 1:
                if (vals[1] == 1)
                {
                    *gpfdat &= ~(1<<5);
                }
                else if (vals[1] == 0)
                {
                    *gpfdat |= (1<<5);
                }
                break;
            // led2
            case 2:
                if (vals[1] == 1)
                {
                    *gpfdat &= ~(1<<6);
                }
                else if (vals[1] == 0)
                {
                    *gpfdat |= (1<<6);
                }
                break;
            default:
                break;
        }
    }
    
    // printk("leds_drv_write\n");
    
    return 0;
}

static struct file_operations leds_drv_fops = {
    .owner  =   THIS_MODULE,
    .open   =   leds_drv_open, 
    .write  =   leds_drv_write,
};

static int leds_drv_init(void)
{
    major = register_chrdev(0, "leds_drv", &leds_drv_fops);

    leds_drv_class = class_create(THIS_MODULE, "leds_drv");
    leds_drv_dev = class_device_create(leds_drv_class, NULL, 
                            MKDEV(major, 0), NULL, "leds");

    gpfcon = (volatile unsigned long *)ioremap(GPFCON, 1);
    gpfdat = (volatile unsigned long *)ioremap(GPFDAT, 1);

    printk("Leds module loaded!\n");

    return 0;
}

static void leds_drv_exit(void)
{
    unregister_chrdev(major, "leds_drv");

    class_device_unregister(leds_drv_dev);
    class_destroy(leds_drv_class);

    iounmap(gpfcon);
    iounmap(gpfdat);

    printk("Leds module closed!\n");
}

module_init(leds_drv_init);
module_exit(leds_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fox_Benjiaming");
