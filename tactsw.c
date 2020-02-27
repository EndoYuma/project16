/**
* Driver for tact switch
* File name: tactsw.c
* Target board: Raspberry pi Model 3B + Apple Pi
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/sched/signal.h>
#define N_TACTSW 1 // number of minor devices
#define MSGLEN 256 // buffer length

static int tactsw_buttons[] = { // board dependent parameters
22, // SW1 (Brown)
23,
24,
25,
26,
27 // SW6 (Blue)
};

// character device
static struct cdev tactsw_dev;
// Info for the driver
static struct {
int major; // major number
int nbuttons; // number of tact switchs
int *buttons; // hardware parameters
int used; // true when used by a process,
// this flag inhibits open twice.
int mlen; // buffer filll count
char msg[MSGLEN]; // buffer
wait_queue_head_t wq; // queue of procs waiting new input
spinlock_t slock; // for spin lock
} tactsw_info;

static int tactsw_open(struct inode *inode, struct file *filp)
{
	unsigned long irqflags;
	int retval = -EBUSY;
	printk("open 開始\n");
	spin_lock_irqsave(&(tactsw_info.slock), irqflags);
	if (tactsw_info.used == 0) {
		tactsw_info.used = 1;
		retval = 0;
		}
	spin_unlock_irqrestore(&(tactsw_info.slock), irqflags);
	return retval;
printk("open 終了\n");
}

static int tactsw_read(struct file *filp, char *buff,size_t count, loff_t *pos)
{
	char *p1, *p2;
	size_t read_size;
	int i;
	unsigned long irqflags;
	printk("read 開始\n");
	if (count <= 0) return -EFAULT;
	wait_event_interruptible(tactsw_info.wq, (tactsw_info.mlen != 0) );
	read_size = tactsw_info.mlen; // atomic, so needless to spin lock
	if (count < read_size) read_size = count;
	if (copy_to_user(buff, tactsw_info.msg, read_size)) {
		printk("tactsw: copy_to_user error\n");
// spin_unlock_irqrestore()
		return -EFAULT;
		}
// Ring buffer is better. But here, we prefer simplicity.
	p1 = tactsw_info.msg;
	p2 = p1+read_size;
	spin_lock_irqsave(&(tactsw_info.slock), irqflags);
// this subtraction is safe, since there is a single reader.
	tactsw_info.mlen -= read_size;
	for (i=tactsw_info.mlen; i>0; i--) *p1++=*p2++;
	spin_unlock_irqrestore(&(tactsw_info.slock), irqflags);
	printk("read 終了\n");
	return read_size;
}

long tactsw_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval=0;
	printk("ioctl 開始\n");
	switch(cmd){
		case _IO('T',1): // inc gcounter
		break;
		case _IO('T',2): // return gcounter
		break;
		case _IO('T',3):
		break;
		case _IO('T',4):
		break;
		case _IO('T',5):
		break;
		default:
		retval = -EFAULT; // other code may be better
	}
	printk("ioctl 終了\n");
	return retval;
}

static int tactsw_release(struct inode *inode, struct file *filp)
{
	printk("release 開始\n");
	tactsw_info.used = 0;
	printk("release 終了\n");
	return 0;
}

static irqreturn_t tactsw_intr(int irq, void *dev_id)
{
	int i;
	printk("intr 開始\n");
	int count=0;
	char buff[100];
	for(i=0;i<tactsw_info.nbuttons;i++){
		buff[i]= gpio_get_value(tactsw_info.buttons[i]);
		if(buff[i]==0) count++;
	}
	for (i = 0; i < tactsw_info.nbuttons; i++) {
		printk("i:%d\n",i);
		int gpio = tactsw_info.buttons[i];
		printk("gpio:%d\n",gpio);
		if (irq == gpio_to_irq(gpio)) {
			printk("irq%d\n",irq);
			int mlen;
			unsigned long irqflags;
			printk("irqflags:%d\n",irqflags);
			int val = gpio_get_value(gpio); // gpio_get_value(gpio)は押されたら0を返す
			int ch = (val == 0)? '1':'0'; // val=0 when key is pushed 真、偽
			printk("val%d\n",val);
			printk("ch%d\n",ch);
			if(count==1) ch='1';
			if(count==2) ch='2';
			if(count==3) ch='3';
			if(count==4) ch='4';
			if(count==5) ch='5';
			if(count==6) ch='6';
			spin_lock_irqsave(&(tactsw_info.slock), irqflags);
			printk("ch%d\n",ch);
			mlen = tactsw_info.mlen;
			printk("meln%d\n",mlen);
			if (mlen < MSGLEN) {
				printk("MSGLEN:%d\n",MSGLEN);
				tactsw_info.msg[mlen] = ch;
				tactsw_info.mlen = mlen+1;
				wake_up_interruptible(&(tactsw_info.wq));
			}
			spin_unlock_irqrestore(&(tactsw_info.slock), irqflags);
			return IRQ_HANDLED;
		}
	}
	printk("intr 開終了\n");
	return IRQ_NONE;
}

static struct file_operations tactsw_fops = {
.read = tactsw_read,
.unlocked_ioctl = tactsw_ioctl,
.open = tactsw_open,
.release = tactsw_release,
};

static int __init tactsw_setup(int major)
{
	int i, error, gpio, irq;
	printk("setup 開始\n");
	tactsw_info.major = major;
	tactsw_info.nbuttons = sizeof(tactsw_buttons)/sizeof(int);
	tactsw_info.buttons = tactsw_buttons;
	tactsw_info.used = 0;
	tactsw_info.mlen = 0;
	init_waitqueue_head(&(tactsw_info.wq));
	spin_lock_init(&(tactsw_info.slock));
	for (i = 0; i < tactsw_info.nbuttons; i++) {
		gpio = tactsw_info.buttons[i];
		error = gpio_request(gpio, "tactsw");
// 2nd arg (label) is used for debug message and sysfs.
		if (error < 0) {
			printk("tactsw: gpio_request error %d (GPIO=%d)\n", error, gpio);
			goto fail;
		}
		error = gpio_direction_input(gpio);
		if (error < 0) {
			printk("tactsw: gpio_direction_input error %d (GPIO=%d)\n", error, gpio);
			goto free_fail;
		}
		irq = gpio_to_irq(gpio);
		if (irq < 0) {
			error = irq;
			printk("tactsw: gpio_to_irq error %d (GPIO=%d)\n", error, gpio);
			goto free_fail;
		}
		error = request_irq(irq, tactsw_intr,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		"tactsw", // used for debug message
		tactsw_intr); // passed to isr's 2nd arg
		if (error) {
			printk("tactsw: request_irq error %d (IRQ=%d)\n", error, irq);
			goto free_fail;
		}
	} // end of for
	return 0;
	free_fail:
	gpio_free(gpio);
	fail:
	while (--i >= 0) {
		gpio = tactsw_info.buttons[i];
		free_irq(gpio_to_irq(gpio), tactsw_intr);
		gpio_free(gpio);
	}
	printk("setup 終了\n");
	return error;
}

// ・static と付けるとこのファイル内でのみ見えることの指示．
// 付けないと OS 全体から見えるようになる．シンボルテーブルが取られる．
// OS がリブートしない限りシンボルテーブル上で邪魔になる．
// ・init __init とすると .text.init セクションに入る．
// 不要になった時 (=init 終了後) に削除してくれる．
static int __init tactsw_init(void)
{
	int ret, major;
	dev_t dev = MKDEV(0, 0); // dev_t は単なる int
	printk("init 開始\n");
	ret = alloc_chrdev_region(&dev, 0, N_TACTSW, "tactsw");
	if (ret < 0) {
		return -1;
	}
	major = MAJOR(dev);
	printk("tactsw: Major number = %d.\n", major);
	cdev_init(&tactsw_dev, &tactsw_fops);
	tactsw_dev.owner = THIS_MODULE;
	ret = cdev_add(&tactsw_dev, MKDEV(major, 0), N_TACTSW);
	if (ret < 0) {
		printk("tactsw: cdev_add error\n");
		unregister_chrdev_region(dev, N_TACTSW);
		return -1;
	}
	ret = tactsw_setup(major);
	if (ret < 0) {
		printk("tactsw: setup error\n");
		cdev_del(&tactsw_dev);
		unregister_chrdev_region(dev, N_TACTSW);
	}
	printk("init 終了\n");
	return ret;
}

// init __exit は上と同様に .text.exit セクションに入る．
static void __exit tactsw_exit(void)
{
	dev_t dev=MKDEV(tactsw_info.major, 0);
	int i;
	printk("exit 開始\n");
// disable interrupts
	for (i = 0; i < tactsw_info.nbuttons; i++) {
		int gpio = tactsw_info.buttons[i];
		int irq = gpio_to_irq(gpio);
		free_irq(irq, tactsw_intr);
		gpio_free(gpio);
	}
// delete devices
	cdev_del(&tactsw_dev);
	unregister_chrdev_region(dev, N_TACTSW);
// wake up tasks
// This case never occurs since OS rejects rmmod when the device is open.
	if (waitqueue_active(&(tactsw_info.wq))) {
		printk("tactsw: there remains waiting tasks. waking up.\n");
		wake_up_all(&(tactsw_info.wq));
// Strictly speaking, we have to wait all processes wake up.
	}
	printk("exit 終了\n");
}

module_init(tactsw_init);
module_exit(tactsw_exit);
MODULE_AUTHOR("ProjectEnshu");
MODULE_DESCRIPTION("tact switch driver for RaspberryPi/ApplePi");
MODULE_LICENSE("GPL");
}
