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

// 定义串口1配置相关常量
// PCLK: 50MHz, baud: 115200
#define PCLK            (50*1000*1000)
#define UART_CLK        PCLK
#define UART_BAUD_RATE  115200
#define UART_BRD        ((UART_CLK/(UART_BAUD_RATE*16))-1)

// 定义串口1相关寄存器地址常量
#define GPHCON      0x56000070
#define GPHUP       0x56000078
#define ULCON1      0x50004000
#define UCON1       0x50004004
#define UFCON1      0x50004008
#define UMCON1      0x5000400C
#define UTRSTAT1    0x50004010
#define UFSTAT1     0x50004018
#define UTXH1       0x50004020
#define URXH1       0x50004024
#define UBRDIV1     0x50004028
#define TX_READY   (1<<2)
#define RX_READY   (1<<0)
#define RX_FULL    (1<<6)
#define RX_COUNT   (0x3f<<0)
#define RX_CLEAR   (1<<1)

// 其他常量
#define READ_BUFF_SIZE  32
#define WRITE_BUFF_SIZE 128
#define READ_AVAIL_SIZE 15

// 主设备号
int major;
// 创建设备相关变量
static struct class *card_uart_class;
static struct class_device *card_uart_device;
// 串口数据读取相关变量
static DECLARE_WAIT_QUEUE_HEAD(uart_read_waitq);
static volatile int ev_press = 0;
static volatile int uart_rec_count;
static volatile unsigned char uart_rec_buffer[READ_BUFF_SIZE];
// 串口数据发送相关变量
static unsigned char uart_send_buffer[WRITE_BUFF_SIZE];

// 创建串口1相关寄存器变量，用于存储虚拟地址
volatile unsigned long *gphcon = NULL;
volatile unsigned long *gphup = NULL;
volatile unsigned long *ulcon1 = NULL;
volatile unsigned long *ucon1 = NULL;
volatile unsigned long *ufcon1 = NULL;
volatile unsigned long *umcon1 = NULL;
volatile unsigned long *utrstat1 = NULL;
volatile unsigned long *ufstat1 = NULL;
volatile unsigned char *utxh1 = NULL;
volatile unsigned char *urxh1 = NULL;
volatile unsigned long *ubrdiv1 = NULL;

// uart1 RXD 中断服务程序
static irqreturn_t uart_read_irp(int irq, void * dev_instance)
{
    int i;

    uart_rec_count = *ufstat1 & RX_COUNT;
    // printk("count:%d\n", uart_rec_count);
    if (uart_rec_count == READ_AVAIL_SIZE)
    {
        for (i = 0; i < uart_rec_count; i++)
            uart_rec_buffer[i] = *urxh1;
        ev_press = 1;
        wake_up_interruptible(&uart_read_waitq);
    }
    else
    {
        *ufcon1 |= RX_CLEAR;
        // printk("unavailabe size\n");
    }
    

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int card_uart_open(struct inode *inode, struct file *file)
{
    // GPH4,5 作为 TXD,RXD
    *gphcon &= ~((0x3<<8)|(0x3<<10));
    *gphcon |= ((0x2<<8)|(0x2<<10));
    // GPH4,5 内部上拉
    *gphup  &= ~(0x3<<4);
    *gphup  |= (0x3<<4);
    // 数据格式:8个数据位,没有流控,1个停止位,无奇偶校验
    *ulcon1 = 0x03;
    // 查询方式, 使能接收超时中断, 时钟源为PCLK
    *ucon1  = 0x85;
    // TX 不使用 FIFO, RX 使用 FIFO, 不使用流控
    *ufcon1 = 0x31;
    *umcon1 = 0x00;
    // 波特率:115200 
    *ubrdiv1 = UART_BRD;
    // 清空 RX buffer
    *ufcon1 |= RX_CLEAR;
    // 关联中断源和中断服务程序
    request_irq(IRQ_S3CUART_RX1, uart_read_irp, 0, "uart1", NULL);

    printk("Card uart opened!\n");

    return 0;
}

static ssize_t card_uart_read(struct file *filp, char __user *buffer, 
    size_t count, loff_t *ppos)
{
    if (count > READ_BUFF_SIZE)
    {
        printk("Card uart read error: count couldn't more than %d!\n", READ_BUFF_SIZE);
        return -EINVAL;
    }

    wait_event_interruptible(uart_read_waitq, ev_press);

    copy_to_user(buffer, uart_rec_buffer, uart_rec_count);
    ev_press = 0;

    return uart_rec_count;
}

static ssize_t card_uart_write(struct file *filp, const char __user *buffer, 
    size_t count, loff_t *ppos)
{
    int i;

    if (count < WRITE_BUFF_SIZE)
    {
        copy_from_user(uart_send_buffer, buffer, count);   // user space to kernel space
        for (i = 0; i < count; i++)
        {
            while (!(*utrstat1 & TX_READY));
            *utxh1 = uart_send_buffer[i];
        }
        // printk("Card uart writed!\n");
    }
    else
    {
        printk("Card uart write error: count couldn't more than %d!\n", WRITE_BUFF_SIZE);
    }

    return 0;
}

static int card_uart_release(struct inode *inode, struct file *file)
{
    free_irq(IRQ_S3CUART_RX1, 1);
    // GPH4,5 作为普通IO口，防止串口继续接收数据
    // uart_rec_datas[0] = *urxh1;
    *gphcon &= ~((0x3<<8)|(0x3<<10));
    *gphcon |= ((0x1<<8)|(0x1<<10));

    printk("Card uart closed!\n");

    return 0;
}

static struct file_operations card_uart_fops = {
    .owner      =   THIS_MODULE,
    .open       =   card_uart_open,
    .write      =   card_uart_write,
    .read       =   card_uart_read,
    .release    =   card_uart_release,
};

static int card_uart_init(void)
{
    // 自动分配主设备号
    major = register_chrdev(0, "card_uart_drv", &card_uart_fops);
    // 自动创建设备
    card_uart_class = class_create(THIS_MODULE, "card_uart_drv");
    card_uart_device = class_device_create(card_uart_class, NULL, 
        MKDEV(major, 0), NULL, "card_uart");
    // 获取设备相关寄存器的虚拟地址
    gphcon = (volatile unsigned long *)ioremap(GPHCON, 1);
    gphup = (volatile unsigned long *)ioremap(GPHUP, 1);
    ulcon1 = (volatile unsigned long *)ioremap(ULCON1, 1);
    ucon1 = (volatile unsigned long *)ioremap(UCON1, 1);
    ufcon1 = (volatile unsigned long *)ioremap(UFCON1, 1);
    umcon1 = (volatile unsigned long *)ioremap(UMCON1, 1);
    utrstat1 = (volatile unsigned long *)ioremap(UTRSTAT1, 1);
    ufstat1 = (volatile unsigned long *)ioremap(UFSTAT1, 1);
    utxh1 = (volatile unsigned char *)ioremap(UTXH1, 1);
    urxh1 = (volatile unsigned char *)ioremap(URXH1, 1);
    ubrdiv1 = (volatile unsigned long *)ioremap(UBRDIV1, 1);

    printk("Card uart module loaded!\n");

    return 0;
}

static void card_uart_exit(void)
{
    // 注销主设备号
    unregister_chrdev(major, "card_uart_drv");
    // 注销设备
    class_device_unregister(card_uart_device);
    class_destroy(card_uart_class);
    // 注销与物理地址映射的虚拟地址
    iounmap(gphcon);
    iounmap(gphup);
    iounmap(ulcon1);
    iounmap(ucon1);
    iounmap(ufcon1);
    iounmap(umcon1);
    iounmap(utrstat1);
    iounmap(ufstat1);
    iounmap(utxh1);
    iounmap(urxh1);
    iounmap(ubrdiv1);

    printk("Card usar module exit!\n");
}

module_init(card_uart_init);
module_exit(card_uart_exit);

MODULE_AUTHOR("Fox_Benjiaming");
MODULE_LICENSE("GPL");
