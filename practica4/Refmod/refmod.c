#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>

MODULE_DESCRIPTION("Refmod Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

/*
 *  Prototypes
 */
int init_module(void);
void cleanup_module(void);
static int refmod_open(struct inode *, struct file *);
static int refmod_release(struct inode *, struct file *);
static ssize_t refmod_read(struct file *, char *, size_t, loff_t *);

#define DEVICE_NAME "refmod"  /* Dev name as it appears in /proc/devices   */
#define CLASS_NAME "cool"

static struct file_operations fops = {
    .read = refmod_read,
    .open = refmod_open,
    .release = refmod_release
};

int stime_ms=5000; /* 5 seconds */
module_param(stime_ms, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(stime_ms, "Time to sleep for when reading from device file");

static struct miscdevice misc_refmod = {
    .minor = MISC_DYNAMIC_MINOR,    /* kernel dynamically assigns a free minor# */
    .name = DEVICE_NAME, /* when misc_register() is invoked, the kernel
                        * will auto-create device file as /dev/chardev ;
                        * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
    .mode = 0666,     /* ... dev node perms set as specified here */
    .fops = &fops,    /* connect to this driver's 'functionality' */
};

/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
    int major;      /* Major number assigned to our device driver */
    int minor;      /* Minor number assigned to the associated character device */
    int ret;
    struct device *device;

    ret = misc_register(&misc_refmod);

    if (ret) {
        pr_err("Couldn't register misc device\n");
        return ret;
    }

    device = misc_refmod.this_device;

    /* Access devt field in device structure to retrieve (major,minor) */
    major = MAJOR(device->devt);
    minor = MINOR(device->devt);

    dev_info(device, "I was assigned major number %d. To talk to\n", major);
    dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
    dev_info(device, "Remove the module when done.\n");

    return 0;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    misc_deregister(&misc_refmod);
    pr_info("refmod misc driver deregistered. Bye\n");
}

/*
 * Called when a process tries to open the device file.
 */
static int refmod_open(struct inode *inode, struct file *file)
{
    return 0;
}

/*
 * Called when a process closes the device file.
 */
static int refmod_release(struct inode *inode, struct file *file)
{
    return 0;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t refmod_read(struct file *filp,   /* see include/linux/fs.h   */
                           char *buffer,    /* buffer to fill with data */
                           size_t len,   /* length of the buffer     */
                           loff_t * off)
{  
    /*
     * If we're at the end of the message,
     * return 0 -> end of file
     */
    if (*off > 0)
        return 0;

    msleep(stime_ms);

    *off+=len;
    return len;
}
