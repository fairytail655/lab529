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

// 定义串口2配置相关常量
// PCLK: 50MHz, baud: 115200
#define PCLK            (50*1000*1000)
#define UART_CLK        PCLK
#define UART_BAUD_RATE  9600
#define UART_BRD        ((UART_CLK/(UART_BAUD_RATE*16))-1)

// 定义串口2相关寄存器地址常量
#define GPHCON      0x56000070
#define GPHUP       0x56000078
#define ULCON2      0x50008000
#define UCON2       0x50008004
#define UFCON2      0x50008008
#define UTRSTAT2    0x50008010
#define UFSTAT2     0x50008018
#define UTXH2       0x50008020
#define URXH2       0x50008024
#define UBRDIV2     0x50008028
#define TX_READY   (1<<2)
#define RX_READY   (1<<0)
#define RX_FULL    (1<<6)
#define RX_COUNT   (0x3f<<0)
#define RX_CLEAR   (1<<1)
// 定义 EINT19 相关寄存器
#define GPGCON      0x56000060
#define GPGUP       0x56000068

// 其他常量
#define READ_BUFF_SIZE  32
#define WRITE_BUFF_SIZE 128
#define READ_AVAIL_SIZE 15

// 主设备号
int major;
// 创建设备相关变量
static struct class *tv_control_class;
static struct class_device *tv_control_device;
// button 读取相关变量
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
static volatile int ev_press = 0;
// 串口数据发送相关变量
static unsigned char uart_send_buffer[WRITE_BUFF_SIZE];

// 创建串口2相关寄存器变量，用于存储虚拟地址
volatile unsigned long *gphcon = NULL;
volatile unsigned long *gphup = NULL;
volatile unsigned long *ulcon2 = NULL;
volatile unsigned long *ucon2 = NULL;
volatile unsigned long *ufcon2 = NULL;
volatile unsigned long *utrstat2 = NULL;
volatile unsigned long *ufstat2 = NULL;
volatile unsigned char *utxh2 = NULL;
volatile unsigned char *urxh2 = NULL;
volatile unsigned long *ubrdiv2 = NULL;
// 创建 button 相关寄存器变量，用于存储虚拟地址
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgup = NULL;

// EINT19 中断服务程序
static irqreturn_t button_irp(int irq, void * dev_instance)
{
    // printk("[Tv control] drv : irq\n");
    ev_press = 1;
    wake_up_interruptible(&button_waitq);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static void device_init(void)
{
    // 串口1 相关初始化
    // GPH6,7 作为 TXD,RXD
    *gphcon &= ~((0x3<<12)|(0x3<<14));
    *gphcon |= ((0x2<<12)|(0x2<<14));
    // GPH6,7 内部上拉
    *gphup  &= ~(0x3<<6);
    *gphup  |= (0x3<<6);
    // 数据格式:8个数据位,没有流控,1个停止位,无奇偶校验
    *ulcon2 = 0x03;
    // 使能发送, 失能接收, 时钟源为PCLK
    *ucon2  = 0x04;
    // 禁用 fifo 和 流控
    *ufcon2 = 0;
    // 波特率:115200 
    *ubrdiv2 = UART_BRD;

    // button 相关初始化
    // GPG11 作为 EINT19
    *gpgcon &= ~(0x3<<22);
    *gpgcon |= 0x2<<22;
    // GPG11 内部上拉
    *gpgup |= 0x1<<11;

    // 关联中断源和中断服务程序
    if (!request_irq(IRQ_EINT19, button_irp, IRQT_FALLING, "EINT19", NULL))
        return;
}

static void device_uinit(void)
{
    // GPH6,7 作为普通IO口，防止串口继续接收数据
    *gphcon &= ~((0x3<<12)|(0x3<<14));
    *gphcon |= ((0x1<<12)|(0x1<<14));
    // GPG11 作为普通IO口
    *gpgcon &= ~(0x3<<22);
    *gpgcon |= 0x1<<22;
    // 释放中断
    free_irq(IRQ_EINT19, (void *)NULL);
}

static int tv_control_open(struct inode *inode, struct file *file)
{
    // printk("[Tv control] opened!\n");
    return 0;
}

static ssize_t tv_control_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    wait_event_interruptible(button_waitq, ev_press);
    ev_press = 0;

    return 1;
}

static ssize_t tv_control_write(struct file *filp, const char __user *buffer, 
    size_t count, loff_t *ppos)
{
    int i;

    if (count < WRITE_BUFF_SIZE)
    {
        if (!copy_from_user(uart_send_buffer, buffer, count))   // user space to kernel space
        {
            for (i = 0; i < count; i++)
            {
                while (!(*utrstat2 & TX_READY));
                *utxh2 = uart_send_buffer[i];
            }
        }
        // printk("Card uart writed!\n");
    }
    else
    {
        printk("[Tv control] write error: count couldn't more than %d!\n", WRITE_BUFF_SIZE);
    }

    return 0;
}

static int tv_control_release(struct inode *inode, struct file *file)
{

    // printk("[Tv control] closed!\n");

    return 0;
}

static struct file_operations tv_control_fops = {
    .owner      =   THIS_MODULE,
    .open       =   tv_control_open,
    .write      =   tv_control_write,
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
    gphcon = (volatile unsigned long *)ioremap(GPHCON, 1);
    gphup = (volatile unsigned long *)ioremap(GPHUP, 1);
    ulcon2 = (volatile unsigned long *)ioremap(ULCON2, 1);
    ucon2 = (volatile unsigned long *)ioremap(UCON2, 1);
    ufcon2 = (volatile unsigned long *)ioremap(UFCON2, 1);
    utrstat2 = (volatile unsigned long *)ioremap(UTRSTAT2, 1);
    ufstat2 = (volatile unsigned long *)ioremap(UFSTAT2, 1);
    utxh2 = (volatile unsigned char *)ioremap(UTXH2, 1);
    urxh2 = (volatile unsigned char *)ioremap(URXH2, 1);
    ubrdiv2 = (volatile unsigned long *)ioremap(UBRDIV2, 1);
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
    iounmap(gphcon);
    iounmap(gphup);
    iounmap(ulcon2);
    iounmap(ucon2);
    iounmap(ufcon2);
    iounmap(utrstat2);
    iounmap(ufstat2);
    iounmap(utxh2);
    iounmap(urxh2);
    iounmap(ubrdiv2);
    iounmap(gpgcon);
    iounmap(gpgup);

    printk("[Tv control] module exit!\n");
}

module_init(tv_control_init);
module_exit(tv_control_exit);

MODULE_AUTHOR("Fox_Benjiaming");
MODULE_LICENSE("GPL");
