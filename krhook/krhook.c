/**
 * @file		sys_call_hook
 * @brief		Linux Kernel Module Template
 *
 * @author		yhnu
 * @copyright	Copyright (c) 2016-2021 T. yhnu
 *
 * @par License
 *	Released under the MIT and GPL Licenses.
 *	- https://github.com/ngtkt0909/linux-kernel-module-template/blob/master/LICENSE-MIT
 *	- https://github.com/ngtkt0909/linux-kernel-module-template/blob/master/LICENSE-GPL
 */

#include <linux/module.h>	/* MODULE_*, module_* */
#include <linux/fs.h>		/* file_operations, alloc_chrdev_region, unregister_chrdev_region */
#include <linux/cdev.h>		/* cdev, dev_init(), cdev_add(), cdev_del() */
#include <linux/device.h>	/* class_create(), class_destroy(), device_create(), device_destroy() */
#include <linux/slab.h>		/* kmalloc(), kfree() */
#include <asm/uaccess.h>	/* copy_from_user(), copy_to_user() */
#include <linux/uaccess.h>  /* copy_from_user(), copy_to_user() */
#include <linux/syscalls.h>

/*------------------------------------------------------------------------------
	Prototype Declaration
------------------------------------------------------------------------------*/
static int helloInit(void);
static void helloExit(void);
static int helloOpen(struct inode *inode, struct file *filep);
static int helloRelease(struct inode *inode, struct file *filep);
static ssize_t helloWrite(struct file *filep, const char __user *buf, size_t count, loff_t *f_pos);
static ssize_t helloRead(struct file *filep, char __user *buf, size_t count, loff_t *f_pos);

static int sRegisterDev(void);
static void sUnregisterDev(void);

//
int syscall_hook_init(void);
void syscall_hook_exit(void);

/*------------------------------------------------------------------------------
	Defined Macros
------------------------------------------------------------------------------*/
#define D_DEV_NAME		"mypid"			    /**< device name */
#define D_DEV_MAJOR		(0)					/**< major# */
#define D_DEV_MINOR		(0)					/**< minor# */
#define D_DEV_NUM		(1)					/**< number of device */
#define D_BUF_SIZE		(PAGE_SIZE)			/**< buffer size (for sample-code) */

/*------------------------------------------------------------------------------
	Type Definition
------------------------------------------------------------------------------*/
/** @brief private data */
typedef struct t_private_data {
	int minor;								/**< minor# */
	int pid;
} T_PRIVATE_DATA;

/*------------------------------------------------------------------------------
	Global Variables
------------------------------------------------------------------------------*/
static struct class *g_class;				/**< device class */
static struct cdev *g_cdev_array;			/**< charactor devices */
static int g_dev_major = D_DEV_MAJOR;		/**< major# */
static int g_dev_minor = D_DEV_MINOR;		/**< minor# */
static char g_buf[D_DEV_NUM][D_BUF_SIZE];	/**< buffer (for sample-code) */

/** file operations */
static struct file_operations g_fops = {
	.open    = helloOpen,
	.release = helloRelease,
	.write   = helloWrite,
	.read    = helloRead,
};

/*------------------------------------------------------------------------------
	Macro Calls
------------------------------------------------------------------------------*/
MODULE_AUTHOR("yhnu");
MODULE_LICENSE("Dual MIT/GPL");

module_init(helloInit);
module_exit(helloExit);

module_param(g_dev_major, int, S_IRUSR | S_IRGRP | S_IROTH);
module_param(g_dev_minor, int, S_IRUSR | S_IRGRP | S_IROTH);

/*------------------------------------------------------------------------------
	Functions (External)
------------------------------------------------------------------------------*/
/**
 * @brief Kernel Module Init
 *
 * @param nothing
 *
 * @retval 0		success
 * @retval others	failure
 */
static int helloInit(void)
{
	int ret;

	printk(KERN_ALERT "%s loading ...\n", D_DEV_NAME);

	/* register devices */
	if ((ret = sRegisterDev()) != 0) {
		printk(KERN_ERR "register_dev() failed\n");
		return ret;
	}

	return 0;
}

/**
 * @brief Kernel Module Exit
 *
 * @param nothing
 *
 * @retval nothing
 */
static void helloExit(void)
{
	printk(KERN_ALERT "%s unloading ...\n", D_DEV_NAME);

	/* unregister devices */
	sUnregisterDev();
}

/**
 * @brief Kernel Module Open : open()
 *
 * @param [in]		inode	inode structure
 * @param [in,out]	filep	file structure
 *
 * @retval 0		success
 * @retval others	failure
 */
static int helloOpen(struct inode *inode, struct file *filep)
{
	T_PRIVATE_DATA *info;


	/* allocate private data */
	info = (T_PRIVATE_DATA *) kmalloc(sizeof(T_PRIVATE_DATA), GFP_KERNEL);

	/* store minor# into private data */
	info->minor = MINOR(inode->i_rdev);
	filep->private_data = (void *)info;

	printk(KERN_ALERT "%s opening minor=%d...\n", D_DEV_NAME, info->minor);

	return 0;
}

/**
 * @brief Kernel Module Release : close()
 *
 * @param [in]	inode	inode structure
 * @param [in]	filep	file structure
 *
 * @retval 0		success
 * @retval others	failure
 */
static int helloRelease(struct inode *inode, struct file *filep)
{
	T_PRIVATE_DATA *info = (T_PRIVATE_DATA *)filep->private_data;

	printk(KERN_ALERT "%s releasing ...\n", D_DEV_NAME);

	/* deallocate private data */
	kfree(info);

	return 0;
}

/**
 * @brief Kernel Module Write : write()
 *
 * @param [in]		filep	file structure
 * @param [in]		buf		buffer address (user)
 * @param [in]		count	write data size
 * @param [in,out]	f_pos	file position
 *
 * @return	number of write byte
 */
static ssize_t helloWrite(struct file *filep, const char __user *buf, size_t count, loff_t *f_pos)
{
	int minor = ((T_PRIVATE_DATA *)(filep->private_data))->minor;
	char* buffer = g_buf[minor];

	printk(KERN_ALERT "%s writing %d...\n", D_DEV_NAME, count);

	if (count > D_BUF_SIZE) {
		printk(KERN_ALERT "%s write data overflow\n", D_DEV_NAME);
		count = D_BUF_SIZE;
	}

	if (copy_from_user(g_buf[minor], buf, count)) {
		return -EFAULT;
	}
	buffer[strcspn(buffer, "\n")] = 0; // Remove End of line character

	return count;
}

/**
 * @brief Kernel Module Read : read()
 *
 * @param [in]		filep	file structure
 * @param [out]		buf		buffer address (user)
 * @param [in]		count	read data size
 * @param [in,out]	f_pos	file position
 *
 * @return	number of read byte
 */

// https://stackoverflow.com/questions/60045498/why-does-cat-call-read-twice-when-once-was-enough
// count is 4096 is use cat, so the buffer size is 4096
static ssize_t helloRead(struct file *filep, char __user *buf, size_t count, loff_t *f_pos)
{
	int minor = ((T_PRIVATE_DATA *)(filep->private_data))->minor;
	char* buffer = g_buf[minor];
	if(*f_pos >= PAGE_SIZE)
	{
		printk(KERN_ALERT "%s reading end of buffer.\n", D_DEV_NAME);
		return 0;
	}
	
	printk(KERN_ALERT "%s reading %d f_post=%ld...\n", D_DEV_NAME, count, *f_pos);

	if (count > D_BUF_SIZE) {
		printk(KERN_ALERT "%s read data overflow\n", D_DEV_NAME);
		count = D_BUF_SIZE;
	}
	buffer[strcspn(buffer, "\n")] = 0; // Remove End of line character
	if (copy_to_user(buf, g_buf[minor], count)) {
		return -EFAULT;
	}

	*f_pos += count;

	return count;
}

/*------------------------------------------------------------------------------
	Functions (Internal)
------------------------------------------------------------------------------*/
/**
 * @brief Register Devices
 *
 * @param nothing
 *
 * @retval 0		success
 * @retval others	failure
 */
static int sRegisterDev(void)
{
	//

	dev_t dev, dev_tmp;
	int ret, i;
	
	// hook_init
	syscall_hook_init();

	/* acquire major#, minor# */
	if ((ret = alloc_chrdev_region(&dev, D_DEV_MINOR, D_DEV_NUM, D_DEV_NAME)) < 0) {
		printk(KERN_ERR "alloc_chrdev_region() failed\n");
		return ret;
	}

	g_dev_major = MAJOR(dev);
	g_dev_minor = MINOR(dev);

	/* create device class */
	g_class = class_create(THIS_MODULE, D_DEV_NAME);
	if (IS_ERR(g_class)) {
		return PTR_ERR(g_class);
	}

	/* allocate charactor devices */
	g_cdev_array = (struct cdev *)kmalloc(sizeof(struct cdev) * D_DEV_NUM, GFP_KERNEL);

	for (i = 0; i < D_DEV_NUM; i++) {
		dev_tmp = MKDEV(g_dev_major, g_dev_minor + i);
		/* initialize charactor devices */
		cdev_init(&g_cdev_array[i], &g_fops);
		g_cdev_array[i].owner = THIS_MODULE;
		/* register charactor devices */
		if (cdev_add(&g_cdev_array[i], dev_tmp, 1) < 0) {
			printk(KERN_ERR "cdev_add() failed: minor# = %d\n", g_dev_minor + i);
			continue;
		}
		/* create device node */
		if(D_DEV_NUM == 1) {
			device_create(g_class, NULL, dev_tmp, NULL, D_DEV_NAME);
		} else {
			device_create(g_class, NULL, dev_tmp, NULL, D_DEV_NAME "%u", g_dev_minor + i);
		}
	}
	return 0;
}

/**
 * @brief Unregister Devices
 *
 * @param nothing
 *
 * @return nothing
 */
static void sUnregisterDev(void)
{
	dev_t dev_tmp;
	int i;

	for (i = 0; i < D_DEV_NUM; i++) {
		dev_tmp = MKDEV(g_dev_major, g_dev_minor + i);
		/* delete charactor devices */
		cdev_del(&g_cdev_array[i]);
		/* destroy device node */
		device_destroy(g_class, dev_tmp);
	}

	/* release major#, minor# */
	dev_tmp = MKDEV(g_dev_major, g_dev_minor);
	unregister_chrdev_region(dev_tmp, D_DEV_NUM);

	/* destroy device class */
	class_destroy(g_class);

	/* deallocate charactor device */
	kfree(g_cdev_array);

	//
	syscall_hook_exit();
}

typedef void (* TYPE_update_mapping_prot)(phys_addr_t phys, unsigned long virt, phys_addr_t size, pgprot_t prot);
//typedef asmlinkage long (* TYPE_openat)(const struct pt_regs *pt_regs);
typedef int (* TYPE_openat)(int dirfd, const char *pathname, int flags, mode_t mode);


static unsigned long start_rodata;
static unsigned long end_rodata;
static unsigned long init_begin;
static void ** sys_call_table_ptr;
#define section_size  (init_begin - start_rodata)


TYPE_update_mapping_prot update_mapping_prot;
static TYPE_openat old_openat;

static void disable_wirte_protection(void)
{
    update_mapping_prot(__pa_symbol(start_rodata), (unsigned long)start_rodata, section_size, PAGE_KERNEL);
    return ;
}
 
static void enable_wirte_protection(void)
{
    update_mapping_prot(__pa_symbol(start_rodata), (unsigned long)start_rodata, section_size, PAGE_KERNEL_RO);
    return ;
}

/*
static atomic_t ref_count = ATOMIC_INIT(0);
asmlinkage long my_stub_openat(const struct pt_regs *pt_regs)
{
        atomic_inc(&ref_count);
        long value = -1;
        char kfilename[80] = {0};
 
        int dfd = (int)pt_regs->regs[0];
        char __user *filename = (char*)pt_regs->regs[1];
        int flags = (int)pt_regs->regs[2];
        int mode = (int)pt_regs->regs[3];
 
        value = old_openat_func(pt_regs);
 
        copy_from_user(kfilename, filename, 80);
        printk("%s. process:[%d:%s] open file:%s.\n\t-----> open flags:0x%0x, open %s, fd:%d.\n", __FUNCTION__,
           current->tgid, current->group_leader->comm, kfilename, flags, value>=0?"sucess":"fail", value);
 
openat_return:
        atomic_dec(&ref_count);
        return value;
}
*/

bool inline isUserPid(void)
{
   const struct cred * m_cred = current_cred();
   if(m_cred->uid.val > 10000)
   {
      return true;
   }
   return false;
}

int new_openat(int dirfd, const char *pathname, int flags, mode_t mode)
{
	/*
	if(isUserPid())
    {
		char bufname[256] = {0};
		int pid = get_current()->pid;
		strncpy_from_user(bufname, pathname, 255);

		printk("myLog::openat64 pathname:[%s] current->pid:[%d]\n", bufname, pid);
    }
	*/
	const int minor = 0;
	const int pid = get_current()->pid;
	long mypid = 0;

	if(!kstrtol(g_buf[minor], 0, &mypid) && (pid == mypid || mypid == -1))
	{
		char bufname[256] = {0};
		int pid = get_current()->pid;
		strncpy_from_user(bufname, pathname, 255);
		printk("myLog::openat64 pathname:[%s] current->pid:[%d]\n", bufname, pid);
	}
	return old_openat(dirfd, pathname, flags, mode);
}

int syscall_hook_init(void)
{
	printk(KERN_ALERT "defined Macro USE_IMMEDIATE\n");
	printk(KERN_ALERT "hello world!\n");
	start_rodata = (unsigned long)kallsyms_lookup_name("__start_rodata");
	init_begin = (unsigned long)kallsyms_lookup_name("__init_begin");
	end_rodata = (unsigned long)kallsyms_lookup_name("__end_rodata");
	
	update_mapping_prot = (TYPE_update_mapping_prot)kallsyms_lookup_name("update_mapping_prot");

	sys_call_table_ptr = (void **)kallsyms_lookup_name("sys_call_table");
	printk("sys_call_table=%lx. update_mapping_prot:%lx, start_rodata:%lx, end_rodata:%lx init_begin:%lx.\n", sys_call_table_ptr, update_mapping_prot, start_rodata, end_rodata, init_begin);


	preempt_disable();
	disable_wirte_protection();
	old_openat = (TYPE_openat)sys_call_table_ptr[__NR_openat];
	sys_call_table_ptr[__NR_openat] = (TYPE_openat)new_openat;
	enable_wirte_protection();
	preempt_enable();
	return 0;
}

static void cleanup(void)
{
	preempt_disable();
	disable_wirte_protection();

	if(sys_call_table_ptr[__NR_openat] == new_openat)
	{
		sys_call_table_ptr[__NR_openat] = old_openat;
	}

	enable_wirte_protection();
	preempt_enable();

	return ;
}

void syscall_hook_exit(void)
{
	cleanup();
	printk(KERN_ALERT "I am back.kernel in planet Linux!\n");
}

// module_exit(syscall_hook_exit);
// module_init(syscall_hook_init);
// MODULE_LICENSE("Dual BSD/GPL");

//https://tldp.org/LDP/lkmpg/2.6/html/x351.html
//https://bbs.pediy.com/thread-267004.htm