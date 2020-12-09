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
#define TCNTB0  0x5100000C
#define TCNTB1  0x51000018
#define TIMER0_AUTOLOAD     (1 << 3)
#define TIMER0_UPDATE       (1 << 1)
#define TIMER0_START        (1 << 0)
#define TIMER1_AUTOLOAD     (1 << 11)
#define TIMER1_UPDATE       (1 << 9)
#define TIMER1_START        (1 << 8)

// 定时器写数据结构体
typedef struct 
{
    // 0-timer0, 1-timer1
    unsigned int timer_id;
    // TCNTBn
    unsigned int timer_cnt;
} timer0_1_wr_data;

// 主设备号
int major;
// 设备相关变量
static struct class *timer0_1_drv_class;
static struct class_device *timer0_1_drv_dev;
// timer1 相关寄存器变量
volatile unsigned long * tcfg0 = NULL;
volatile unsigned long * tcfg1 = NULL;
volatile unsigned long * tcon = NULL;
volatile unsigned long * tcntb0 = NULL;
volatile unsigned long * tcntb1 = NULL;
// 信号量
static DECLARE_WAIT_QUEUE_HEAD(timer0_read_waitq);
static volatile int timer0_ev_press = 0;
static DECLARE_WAIT_QUEUE_HEAD(timer1_read_waitq);
static volatile int timer1_ev_press = 0;

static irqreturn_t timer0_read_irq(int irq, void * dev_instance)
{
    // *tcon &= ~TIMER0_START;
    // printk("timer0 irq");
    timer0_ev_press = 1;
    wake_up_interruptible(&timer0_read_waitq);

    return IRQ_RETVAL(IRQ_HANDLED);
}

static irqreturn_t timer1_read_irq(int irq, void * dev_instance)
{
    // printk("timer1 irq");
    // *tcon &= ~TIMER1_START;
    timer1_ev_press = 1;
    wake_up_interruptible(&timer1_read_waitq);

    return IRQ_RETVAL(IRQ_HANDLED);
}

static int timer0_1_drv_open(struct inode *inode, struct file *file)
{
    // 设置 prescaler = 249, divider = 8
    // F_timer0_1 = PCLK / (prescaler + 1) / (divider)
    //            = 50 MHz / 250 / 8
    //            = 25 KHz = 40 us
    *tcfg0 &= ~(0xff);
    *tcfg0 |= 249;
    *tcfg1 &= ~(0xff);
    *tcfg1 |= 0x22;
    // auto reload on, timer stop
    *tcon &= ~(0xfff);
    *tcon |= TIMER0_AUTOLOAD | TIMER1_AUTOLOAD;
    *tcntb0 = 0;
    *tcntb1 = 0;
    // 关联中断源和中断服务程序
    request_irq(IRQ_TIMER0, timer0_read_irq, 0, "timer0", NULL);
    request_irq(IRQ_TIMER1, timer1_read_irq, 0, "timer1", NULL);

    return 0;
}

static ssize_t timer0_1_drv_write(struct file *filp, const char __user *buffer,
    size_t count, loff_t *ppos)
{
    timer0_1_wr_data data;

    if (count != sizeof(data))
    {
        printk("[Timer0_1] write error: count should be %d instead of %d\n", sizeof(data), count);
        return -EINVAL;
    }

    copy_from_user(&data, buffer, sizeof(data));
    printk("%d,%d\n", data.timer_id, data.timer_cnt);

    if (data.timer_cnt > 65535)
    {
        printk("[Timer0_1] write error: val should less than 65536 instead of %d\n", data.timer_cnt);
        return -EINVAL;
    }

    switch (data.timer_id)
    {
    case 0:
        *tcntb0 = data.timer_cnt;
        *tcon |= TIMER0_UPDATE;
        *tcon |= TIMER0_START;
        *tcon &= ~(TIMER0_UPDATE);
        break;
    case 1:
        *tcntb1 = data.timer_cnt;
        *tcon |= TIMER1_UPDATE;
        *tcon |= TIMER1_START;
        *tcon &= ~(TIMER1_UPDATE);
        break;
    default:
        printk("[Timer0_1] write error: timer_id should be 0 or 1 instead of %d\n", data.timer_id);
        return -EINVAL;
    }

    return 0;
}

static ssize_t timer0_1_drv_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    switch (count)
    {
    case 0:
        wait_event_interruptible(timer0_read_waitq, timer0_ev_press);
        timer0_ev_press = 0;
        break;
    case 1:
        wait_event_interruptible(timer1_read_waitq, timer1_ev_press);
        timer1_ev_press = 0;
        break;
    default:
        printk("[Timer0_1] read error: count should be 0 or 1 instead of %d\n", count);
        return -EINVAL;
    }

    return 0;
}

static int timer0_1_drv_release(struct inode *inode, struct file *file)
{
    free_irq(IRQ_TIMER0, 1);
    free_irq(IRQ_TIMER1, 1);
    *tcfg0 &= ~0xff;
    *tcfg1 &= ~0xff;
    *tcon &= ~0xfff;
    *tcntb0 = 0;
    *tcntb1 = 0;

    return 0;
}

static struct file_operations timer1_drv_fops = {
    .owner      =   THIS_MODULE,
    .open       =   timer0_1_drv_open, 
    .write      =   timer0_1_drv_write,
    .read       =   timer0_1_drv_read,
    .release    =   timer0_1_drv_release,
};

static int timer0_1_drv_init(void)
{
    major = register_chrdev(0, "timer0_1_drv", &timer1_drv_fops);

    timer0_1_drv_class = class_create(THIS_MODULE, "timer0_1_drv");
    timer0_1_drv_dev = class_device_create(timer0_1_drv_class, NULL, 
                            MKDEV(major, 0), NULL, "timer0_1");

    tcfg0 = (volatile unsigned long *)ioremap(TCFG0, 1);
    tcfg1 = (volatile unsigned long *)ioremap(TCFG1, 1);
    tcon = (volatile unsigned long *)ioremap(TCON, 1);
    tcntb0 = (volatile unsigned long *)ioremap(TCNTB0, 1);
    tcntb1 = (volatile unsigned long *)ioremap(TCNTB1, 1);

    printk("[Timer0_1] module loaded!\n");

    return 0;
}

static void timer0_1_drv_exit(void)
{
    unregister_chrdev(major, "timer0_1_drv");

    class_device_unregister(timer0_1_drv_dev);
    class_destroy(timer0_1_drv_class);

    iounmap(tcfg0);
    iounmap(tcfg1);
    iounmap(tcon);
    iounmap(tcntb0);
    iounmap(tcntb1);

    printk("[Timer0_1] module exited!\n");
}

module_init(timer0_1_drv_init);
module_exit(timer0_1_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fox_Benjiaming");

