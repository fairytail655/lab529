#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

// 定义 EINT19 相关寄存器
#define GPGCON      0x56000060
#define GPGUP       0x56000068

// 主设备号
int major;
// 创建设备相关变量
static struct class *tv_control_class;
static struct class_device *tv_control_device;
// button 读取相关变量
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
static volatile int ev_press = 0;
static volatile int is_ready = 0;

// 创建 button 相关寄存器变量，用于存储虚拟地址
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgup = NULL;

// EINT19 中断服务程序
static irqreturn_t button_irp(int irq, void *dev_instance)
{
    // printk("[Tv control] drv : irq is_ready:%d\n", is_ready);
    if (is_ready != 0)
    {
        is_ready = 0;
        ev_press = 1;
        wake_up_interruptible(&button_waitq);
    }

	return IRQ_RETVAL(IRQ_HANDLED);
}

static void device_init(void)
{
    // GPG11 作为 EINT19
    *gpgcon &= ~(0x3<<22);
    *gpgcon |= 0x2<<22;
    // GPG11 内部上拉
    *gpgup |= 0x1<<11;
}

static void device_uinit(void)
{
    // GPG11 作为普通IO口
    *gpgcon &= ~(0x3<<22);
    *gpgcon |= 0x1<<22;
}

static int tv_control_open(struct inode *inode, struct file *file)
{
    // 关联中断源和中断服务程序
    if (request_irq(IRQ_EINT19, button_irp, IRQT_FALLING, "EINT19", NULL))
    {
        printk("[Tv control] IRQ failed!");
    }
    return 0;
}

static ssize_t tv_control_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    is_ready = count;
    wait_event_interruptible(button_waitq, ev_press);
    ev_press = 0;

    return 1;
}

static int tv_control_release(struct inode *inode, struct file *file)
{

    // 释放中断
    free_irq(IRQ_EINT19, (void *)NULL);
    // printk("[Tv control] release\n");

    return 0;
}

static struct file_operations tv_control_fops = {
    .owner      =   THIS_MODULE,
    .open       =   tv_control_open,
    .read       =   tv_control_read,
    .release    =   tv_control_release,
};

static int tv_control_init(void)
{
    // 自动分配主设备号
    major = register_chrdev(0, "tv_control_drv", &tv_control_fops);
    // 自动创建设备
    tv_control_class = class_create(THIS_MODULE, "tv_control_drv");
    tv_control_device = class_device_create(tv_control_class, NULL, 
        MKDEV(major, 0), NULL, "tv_control");
    // 获取设备相关寄存器的虚拟地址
    gpgcon = (volatile unsigned long*)ioremap(GPGCON, 1);
    gpgup = (volatile unsigned long*)ioremap(GPGUP, 1);

    device_init();

    printk("[Tv control] module loaded!\n");

    return 0;
}

static void tv_control_exit(void)
{
    // 注销主设备号
    unregister_chrdev(major, "tv_control_drv");
    // 注销设备
    class_device_unregister(tv_control_device);
    class_destroy(tv_control_class);

    device_uinit();

    // 注销与物理地址映射的虚拟地址
    iounmap(gpgcon);
    iounmap(gpgup);

    printk("[Tv control] module exit!\n");
}

module_init(tv_control_init);
module_exit(tv_control_exit);

MODULE_AUTHOR("Fox_Benjiaming");
MODULE_LICENSE("GPL");
