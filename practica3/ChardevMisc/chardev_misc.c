#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>  /* for copy_to_user */
#include <linux/miscdevice.h>

MODULE_DESCRIPTION("ChardevMisc Kernel Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

/*
 *  Prototypes
 */
int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "chardev_misc"  /* Dev name as it appears in /proc/devices   */
#define CLASS_NAME "cool"
#define BUF_LEN 80      /* Max length of the message from the device */

static int Device_Open = 0; /* Is device open?
                 * Used to prevent multiple access to device */
static char msg[BUF_LEN];   /* The msg the device will give when asked */
static char *msg_Ptr;       /* This will be initialized every time the
                   device is opened successfully */
static int counter = 0;       /* Tracks the number of times the character
                 * device has been opened */

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};


static struct miscdevice misc_chardev = {
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

    ret = misc_register(&misc_chardev);

    if (ret) {
        pr_err("Couldn't register misc device\n");
        return ret;
    }

    device = misc_chardev.this_device;

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
    misc_deregister(&misc_chardev);
    pr_info("Chardev misc driver deregistered. Bye\n");
}

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/chardev"
 */
static int device_open(struct inode *inode, struct file *file)
{
    if (Device_Open)
        return -EBUSY;

    Device_Open++;

    /* Initialize msg */
    sprintf(msg, "I already told you %d times Hello world!\n", counter++);

    /* Initially, this points to the beginning of the message */
    msg_Ptr = msg;

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
    Device_Open--;      /* We're now ready for our next caller */

    /*
     * Decrement the usage count, or else once you opened the file, you'll
     * never get get rid of the module.
     */
    module_put(THIS_MODULE);

    return 0;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,   /* see include/linux/fs.h   */
                           char *buffer,    /* buffer to fill with data */
                           size_t length,   /* length of the buffer     */
                           loff_t * offset)
{
    /*
     * Number of bytes actually written to the buffer
     */
    int bytes_to_read = length;

    /*
     * If we're at the end of the message,
     * return 0 -> end of file
     */
    if (*msg_Ptr == 0)
        return 0;

    /* Make sure we don't read more chars than
     * those remaining to read
         */
    if (bytes_to_read > strlen(msg_Ptr))
        bytes_to_read = strlen(msg_Ptr);

    /*
     * Actually transfer the data onto the userspace buffer.
     * For this task we use copy_to_user() due to security issues
     */
    if (copy_to_user(buffer, msg_Ptr, bytes_to_read))
        return -EFAULT;

    /* Update the pointer for the next read operation */
    msg_Ptr += bytes_to_read;

    /*
     * The read operation returns the actual number of bytes
     * we copied  in the user's buffer
     */
    return bytes_to_read;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/chardev
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
    return -EPERM;
}
