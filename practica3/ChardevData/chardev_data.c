#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>  /* for copy_to_user */
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1)
#define __cconst__ const
#else
#define __cconst__ 
#endif

MODULE_DESCRIPTION("ChardevData Kernel Module - FDI-UCM");
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
#define DEVICE_NAME "chardev"   /* Dev name as it appears in /proc/devices   */
#define CLASS_NAME "cool"
#define BUF_LEN 80      /* Max length of the message from the device */

/*
 * Global variables are declared as static, so are global within the file.
 */

static dev_t start;
static struct cdev* chardev = NULL;
static struct class* class = NULL;

struct device_data {
    int Device_Open; /* Is device open?  Used to prevent multiple access to device */
    char msg[BUF_LEN];   /* The msg the device will give when asked */
    char *msg_Ptr;       /* This will be initialized every time the
                            device is opened successfully */
    int counter;       /* Tracks the number of times the character
                             device has been opened */
    struct device* device;
    dev_t major_minor;
};

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};


/**
 * Set up permissions for device files created by this driver
 *
 * The permission mode is returned using the mode parameter.
 *
 * The return value (unused here) could be used to indicate the directory
 * name under /dev where to create the device file. If used, it should be
 * a string whose memory must be allocated dynamically.
 **/
static char *cool_devnode(__cconst__ struct device *dev, umode_t *mode)
{
    if (!mode)
        return NULL;
    if (MAJOR(dev->devt) == MAJOR(start))
        *mode = 0666;
    return NULL;
}

/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
    int major;      /* Major number assigned to our device driver */
    int minor;      /* Minor number assigned to the associated character device */
    int ret;
    struct device_data* ddata;

    /* Get available (major,minor) range */
    if ((ret = alloc_chrdev_region (&start, 0, 1, DEVICE_NAME))) {
        printk(KERN_INFO "Can't allocate chrdev_region()");
        return ret;
    }

    /* Create associated cdev */
    if ((chardev = cdev_alloc()) == NULL) {
        printk(KERN_INFO "cdev_alloc() failed ");
        ret = -ENOMEM;
        goto error_alloc;
    }

    cdev_init(chardev, &fops);

    if ((ret = cdev_add(chardev, start, 1))) {
        printk(KERN_INFO "cdev_add() failed ");
        goto error_add;
    }

    /* Create custom class */
    class = class_create(THIS_MODULE, CLASS_NAME);

    if (IS_ERR(class)) {
        pr_err("class_create() failed \n");
        ret = PTR_ERR(class);
        goto error_class;
    }

    /* Establish function that will take care of setting up permissions for device file */
    class->devnode = cool_devnode;

    /* Allocate device state structure and zero fill it */
    if ((ddata = kzalloc(sizeof(struct device_data), GFP_KERNEL)) == NULL) {
        ret = -ENOMEM;
        goto error_alloc_state;
    }

    /* Proper initialization */
    ddata->Device_Open = 0;
    ddata->counter = 0;
    ddata->msg_Ptr = NULL;
    ddata->major_minor = start; /* Only one device */

    /* Creating device */
    ddata->device = device_create(class, NULL, start, ddata, "%s%d", DEVICE_NAME, 0);

    if (IS_ERR(ddata->device)) {
        pr_err("Device_create failed\n");
        ret = PTR_ERR(ddata->device);
        goto error_device;
    }

    major = MAJOR(start);
    minor = MINOR(start);

    printk(KERN_INFO "I was assigned major number %d. To talk to\n", major);
    printk(KERN_INFO "the driver try to cat and echo to /dev/%s%d.\n", DEVICE_NAME, 0);
    printk(KERN_INFO "Remove the module when done.\n");

    return 0;

error_device:
    if (ddata)
        kfree(ddata);
error_alloc_state:
    class_destroy(class);
error_class:
    /* Destroy chardev */
    if (chardev) {
        cdev_del(chardev);
        chardev = NULL;
    }
error_add:
    /* Destroy partially initialized chardev */
    if (chardev)
        kobject_put(&chardev->kobj);
error_alloc:
    unregister_chrdev_region(start, 1);

    return ret;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
    struct device* device = class_find_device_by_devt(class, start);
    struct device_data* ddata;

    /**
     * Unregister/destroy the device
     *
     * Free up device private data too.
     * **/
    if (device) {
        /* Retrieve device private data */
        ddata = dev_get_drvdata(device);

        if (ddata)
            kfree(ddata);

        device_destroy(class, device->devt);
    }

    class_destroy(class);

    /* Destroy chardev */
    if (chardev)
        cdev_del(chardev);

    /*
     * Release major minor pair
     */
    unregister_chrdev_region(start, 1);
}

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/chardev"
 */
static int device_open(struct inode *inode, struct file *file)
{
    struct device* device;
    struct device_data* ddata;

    /* Retrieve device from major minor of the device file */
    device = class_find_device_by_devt(class, inode->i_rdev);

    if (!device)
        return -ENODEV;

    /* Retrieve driver's private data from device */
    ddata = dev_get_drvdata(device);

    if (!ddata)
        return -ENODEV;

    if (ddata->Device_Open)
        return -EBUSY;

    ddata->Device_Open++;

    /* Initialize msg */
    sprintf(ddata->msg, "I already told you %d times Hello world!\n", ddata->counter++);

    /* Initially, this points to the beginning of the message */
    ddata->msg_Ptr = ddata->msg;

    /* save our object in the file's private structure */
    file->private_data = ddata;

    /* Increment the module's reference counter */
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
    struct device_data* ddata = file->private_data;

    if (ddata == NULL)
        return -ENODEV;

    ddata->Device_Open--;       /* We're now ready for our next caller */

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
    struct device_data* ddata = filp->private_data;

    if (ddata == NULL)
        return -ENODEV;

    /*
     * If we're at the end of the message,
     * return 0 -> end of file
     */
    if (*(ddata->msg_Ptr) == 0)
        return 0;

    /* Make sure we don't read more chars than
     * those remaining to read
         */
    if (bytes_to_read > strlen(ddata->msg_Ptr))
        bytes_to_read = strlen(ddata->msg_Ptr);

    /*
     * Actually transfer the data onto the userspace buffer.
     * For this task we use copy_to_user() due to security issues
     */
    if (copy_to_user(buffer, ddata->msg_Ptr, bytes_to_read))
        return -EFAULT;

    /* Update the pointer for the next read operation */
    ddata->msg_Ptr += bytes_to_read;

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
