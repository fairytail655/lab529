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

// timer0 相关寄存器物理地址
#define TCFG0   0x51000000
#define TCFG1   0x51000004
#define TCON    0x51000008
#define TCNTB0  0x5100000C
#define TIMER0_AUTOLOAD     (1 << 3)
#define TIMER0_UPDATE       (1 << 1)
#define TIMER0_START        (1 << 0)

// 主设备号
int major;
// 设备相关变量
static struct class *timer0_drv_class;
static struct class_device *timer0_drv_dev;
// timer0 相关寄存器变量
volatile unsigned long * tcfg0 = NULL;
volatile unsigned long * tcfg1 = NULL;
volatile unsigned long * tcon = NULL;
volatile unsigned long * tcntb0 = NULL;
// 信号量
static DECLARE_WAIT_QUEUE_HEAD(timer0_read_waitq);
static volatile int ev_press = 0;

static irqreturn_t timer0_read_irp(int irq, void * dev_instance)
{
    *tcon &= ~TIMER0_START;
    ev_press = 1;
    wake_up_interruptible(&timer0_read_waitq);

    // printk("timer0 irq!\n");

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int timer0_drv_open(struct inode *inode, struct file *file)
{
    // 设置 prescaler = 249, divider = 8
    // F_timer0 = PCLK / (prescaler + 1) / (divider)
    //          = 50 MHz / 250 / 8
    //          = 25 KHz = 40 us
    *tcfg0 &= ~(0xff);
    *tcfg0 |= 249;
    *tcfg1 &= ~(0xf);
    *tcfg1 |= 2;
    // auto reload on, timer stop
    *tcon &= ~(0xff);
    *tcon |= TIMER0_AUTOLOAD;
    *tcntb0 = 0;
    // 关联中断源和中断服务程序
    request_irq(IRQ_TIMER0, timer0_read_irp, 0, "timer0", NULL);

    return 0;
}

static ssize_t timer0_drv_write(struct file *filp, const char __user *buffer,
    size_t count, loff_t *ppos)
{
    unsigned int val;

    if (count != 2)
    {
        printk("[Timer0] write error: count should be 2 instead of %d\n", count);
        return -EINVAL;
    }

    copy_from_user(&val, buffer, 2);

    if (val > 65535)
    {
        printk("[Timer0] write error: val should less than 65536 instead of %d\n", val);
        return -EINVAL;
    }

    // printk("%d\n", val);
    *tcntb0 = val;
    *tcon |= TIMER0_UPDATE;
    *tcon |= TIMER0_START;
    *tcon &= ~(TIMER0_UPDATE);

    return 0;
}

static ssize_t timer0_drv_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    wait_event_interruptible(timer0_read_waitq, ev_press);
    ev_press = 0;

    return 0;
}

static int timer0_drv_release(struct inode *inode, struct file *file)
{
    free_irq(IRQ_TIMER0, 1);
    *tcfg0 &= ~0xff;
    *tcfg1 &= ~0xf;
    *tcon &= ~0xff;
    *tcntb0 = 0;

    return 0;
}

static struct file_operations timer0_drv_fops = {
    .owner      =   THIS_MODULE,
    .open       =   timer0_drv_open, 
    .write      =   timer0_drv_write,
    .read       =   timer0_drv_read,
    .release    =   timer0_drv_release,
};

static int timer0_drv_init(void)
{
    major = register_chrdev(0, "timer0_drv", &timer0_drv_fops);

    timer0_drv_class = class_create(THIS_MODULE, "timer0_drv");
    timer0_drv_dev = class_device_create(timer0_drv_class, NULL, 
                            MKDEV(major, 0), NULL, "timer0");

    tcfg0 = (volatile unsigned long *)ioremap(TCFG0, 1);
    tcfg1 = (volatile unsigned long *)ioremap(TCFG1, 1);
    tcon = (volatile unsigned long *)ioremap(TCON, 1);
    tcntb0 = (volatile unsigned long *)ioremap(TCNTB0, 1);

    printk("[Timer0] module loaded!\n");

    return 0;
}

static void timer0_drv_exit(void)
{
    unregister_chrdev(major, "timer0_drv");

    class_device_unregister(timer0_drv_dev);
    class_destroy(timer0_drv_class);

    iounmap(tcfg0);
    iounmap(tcfg1);
    iounmap(tcon);
    iounmap(tcntb0);

    printk("[Timer0] module exited!\n");
}

module_init(timer0_drv_init);
module_exit(timer0_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fox_Benjiaming");