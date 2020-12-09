#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
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

// 其他常量
#define WRITE_BUFF_SIZE 128
#define READ_AVAIL_SIZE 12
#define READ_BUFF_SIZE  READ_AVAIL_SIZE

// 主设备号
int major;
// 创建设备相关变量
static struct class *tv_uart_class;
static struct class_device *tv_uart_device;
// tv_uart 读取相关变量
static DECLARE_WAIT_QUEUE_HEAD(uart_read_waitq);
static volatile int ev_press = 0;
static volatile int uart_rec_count;
static volatile unsigned char uart_rec_buffer[READ_BUFF_SIZE];
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

unsigned char get_crc(unsigned char *data, int len)
{
    int i = 0;
    unsigned char crc = 0;

    for (i = 0; i < len; i++)
        crc += *(data+i);

    return crc;
}

// uart2 RXD 中断服务程序
static irqreturn_t uart_read_irp(int irq, void * dev_instance)
{
    int i = 0;
    int crc = 0;

    uart_rec_count = *ufstat2 & RX_COUNT;
    // printk("count:%d\n", uart_rec_count);
    if (uart_rec_count == READ_AVAIL_SIZE)
    {
        for (i = 0; i < uart_rec_count; i++)
            uart_rec_buffer[i] = *urxh2;
        crc = get_crc(uart_rec_buffer, READ_AVAIL_SIZE-1);
        if (crc == uart_rec_buffer[READ_AVAIL_SIZE-1])
        {
            ev_press = 1;
            wake_up_interruptible(&uart_read_waitq);
        }
        // printk("[Tv uart] crc: %d\n", crc);
    }
    else
    {
        *ufcon2 |= RX_CLEAR;
        // printk("unavailabe size\n");
    }

	return IRQ_RETVAL(IRQ_HANDLED);
}

static void device_init(void)
{
    // 串口2 相关初始化
    // GPH6,7 作为 TXD,RXD
    *gphcon &= ~((0x3<<12)|(0x3<<14));
    *gphcon |= ((0x2<<12)|(0x2<<14));
    // GPH6,7 内部上拉
    *gphup  &= ~(0x3<<6);
    *gphup  |= (0x3<<6);
    // 数据格式:8个数据位,没有流控,1个停止位,无奇偶校验
    *ulcon2 = 0x03;
    // 查询方式, 使能接收超时中断, 时钟源为PCLK
    *ucon2  = 0x85;
    // TX 不使用 FIFO, RX 使用 FIFO
    *ufcon2 = 0x31;
    // 波特率:115200 
    *ubrdiv2 = UART_BRD;
    // 清空 RX buffer
    *ufcon2 |= RX_CLEAR;

    // 关联中断源和中断服务程序
    request_irq(IRQ_S3CUART_RX2, uart_read_irp, 0, "uart2", NULL);
}

static void device_uinit(void)
{
    // GPH6,7 作为普通IO口，防止串口继续接收数据
    *gphcon &= ~((0x3<<12)|(0x3<<14));
    *gphcon |= ((0x1<<12)|(0x1<<14));

    free_irq(IRQ_S3CUART_RX2, NULL);
}

static int tv_uart_open(struct inode *inode, struct file *file)
{

    return 0;
}

static ssize_t tv_uart_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    // if (count < READ_BUFF_SIZE)
    // {
    //     printk("[Tv uart] read error: count is %d\n", count);
    //     return -EINVAL;
    // }

    wait_event_interruptible(uart_read_waitq, ev_press);

    // copy_to_user(buffer, uart_rec_buffer, READ_BUFF_SIZE);
    ev_press = 0;

    return 0;
    // return uart_rec_count;
}

static unsigned int tv_uart_poll(struct file *file, poll_table *wait)
{
    unsigned int mask = 0;

    poll_wait(file, &uart_read_waitq, wait);
    if (ev_press)
        mask |= POLLIN | POLLRDNORM;
    return mask;
}

static ssize_t tv_uart_write(struct file *filp, const char __user *buffer, 
    size_t count, loff_t *ppos)
{
    int i;

    if (count < WRITE_BUFF_SIZE && count > 0)
    {
        if (!copy_from_user(uart_send_buffer, buffer, count))   // user space to kernel space
        {
            for (i = 0; i < count; i++)
            {
                while (!(*utrstat2 & TX_READY));
                *utxh2 = uart_send_buffer[i];
            }
        }
    }
    else
    {
        printk("[Tv uart] write error: count is %d!\n", WRITE_BUFF_SIZE);
    }

    return 0;
}

static int tv_uart_release(struct inode *inode, struct file *file)
{
    // printk("[Tv control] closed!\n");

    return 0;
}

static struct file_operations tv_uart_fops = {
    .owner      =   THIS_MODULE,
    .open       =   tv_uart_open,
    .write      =   tv_uart_write,
    .read       =   tv_uart_read,
    .poll       =   tv_uart_poll,
    .release    =   tv_uart_release,
};

static int tv_uart_init(void)
{
    // 自动分配主设备号
    major = register_chrdev(0, "tv_uart_drv", &tv_uart_fops);
    // 自动创建设备
    tv_uart_class = class_create(THIS_MODULE, "tv_uart_drv");
    tv_uart_device = class_device_create(tv_uart_class, NULL, 
        MKDEV(major, 0), NULL, "tv_uart");
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

    device_init();

    printk("[Tv uart] module loaded!\n");

    return 0;
}

static void tv_uart_exit(void)
{
    // 注销主设备号
    unregister_chrdev(major, "tv_uart_drv");
    // 注销设备
    class_device_unregister(tv_uart_device);
    class_destroy(tv_uart_class);

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

    printk("[Tv uart] module exit!\n");
}

module_init(tv_uart_init);
module_exit(tv_uart_exit);

MODULE_AUTHOR("Fox_Benjiaming");
MODULE_LICENSE("GPL");
