#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/time.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

// timer1 相关寄存器物理地址
#define TCFG0   0x51000000
#define TCFG1   0x51000004
#define TCON    0x51000008
#define TCNTB1  0x51000018
#define TIMER1_AUTOLOAD     (1 << 11)
#define TIMER1_UPDATE       (1 << 9)
#define TIMER1_START        (1 << 8)

// 主设备号
int major;
// 设备相关变量
static struct class *timer1_drv_class;
static struct class_device *timer1_drv_dev;
// timer1 相关寄存器变量
volatile unsigned long * tcfg0 = NULL;
volatile unsigned long * tcfg1 = NULL;
volatile unsigned long * tcon = NULL;
volatile unsigned long * tcntb1 = NULL;
// 信号量
static DECLARE_WAIT_QUEUE_HEAD(timer1_read_waitq);
static volatile int ev_press = 0;

static irqreturn_t timer1_read_irp(int irq, void * dev_instance)
{
    *tcon &= ~TIMER1_START;
    ev_press = 1;
    wake_up_interruptible(&timer1_read_waitq);

    return IRQ_RETVAL(IRQ_HANDLED);
}

static int timer1_drv_open(struct inode *inode, struct file *file)
{
    // 设置 prescaler = 249, divider = 8
    // F_timer1 = PCLK / (prescaler + 1) / (divider)
    //          = 50 MHz / 250 / 8
    //          = 25 KHz = 40 us
    *tcfg0 &= ~(0xff);
    *tcfg0 |= 249;
    *tcfg1 &= ~(0xf0);
    *tcfg1 |= 0x20;
    // auto reload on, timer stop
    *tcon &= ~(0xf00);
    *tcon |= TIMER1_AUTOLOAD;
    *tcntb1 = 0;
    // 关联中断源和中断服务程序
    request_irq(IRQ_TIMER1, timer1_read_irp, 0, "timer1", NULL);

    return 0;
}

static ssize_t timer1_drv_write(struct file *filp, const char __user *buffer,
    size_t count, loff_t *ppos)
{
    unsigned int val;

    if (count != 2)
    {
        printk("[Timer1] write error: count should be 2 instead of %d\n", count);
        return -EINVAL;
    }

    copy_from_user(&val, buffer, 2);

    if (val > 65535)
    {
        printk("[Timer1] write error: val should less than 65536 instead of %d\n", val);
        return -EINVAL;
    }

    // printk("%d\n", val);
    *tcntb1 = val;
    *tcon |= TIMER1_UPDATE;
    *tcon |= TIMER1_START;
    *tcon &= ~(TIMER1_UPDATE);

    return 0;
}

static ssize_t timer1_drv_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    wait_event_interruptible(timer1_read_waitq, ev_press);
    ev_press = 0;

    return 0;
}

static int timer1_drv_release(struct inode *inode, struct file *file)
{
    free_irq(IRQ_TIMER1, 1);
    *tcfg0 &= ~0xff;
    *tcfg1 &= ~0xf0;
    *tcon &= ~0xf00;
    *tcntb1 = 0;

    return 0;
}

static struct file_operations timer1_drv_fops = {
    .owner      =   THIS_MODULE,
    .open       =   timer1_drv_open, 
    .write      =   timer1_drv_write,
    .read       =   timer1_drv_read,
    .release    =   timer1_drv_release,
};

static int timer1_drv_init(void)
{
    major = register_chrdev(0, "timer1_drv", &timer1_drv_fops);

    timer1_drv_class = class_create(THIS_MODULE, "timer1_drv");
    timer1_drv_dev = class_device_create(timer1_drv_class, NULL, 
                            MKDEV(major, 0), NULL, "timer1");

    tcfg0 = (volatile unsigned long *)ioremap(TCFG0, 1);
    tcfg1 = (volatile unsigned long *)ioremap(TCFG1, 1);
    tcon = (volatile unsigned long *)ioremap(TCON, 1);
    tcntb1 = (volatile unsigned long *)ioremap(TCNTB1, 1);

    printk("[Timer1] module loaded!\n");

    return 0;
}

static void timer1_drv_exit(void)
{
    unregister_chrdev(major, "timer1_drv");

    class_device_unregister(timer1_drv_dev);
    class_destroy(timer1_drv_class);

    iounmap(tcfg0);
    iounmap(tcfg1);
    iounmap(tcon);
    iounmap(tcntb1);

    printk("[Timer1] module exited!\n");
}

module_init(timer1_drv_init);
module_exit(timer1_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fox_Benjiaming");

