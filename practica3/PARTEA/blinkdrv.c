#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,2,1)
#define __cconst__ const
#else
#define __cconst__ 
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lucas Calzada Y Juan Girón"); 

/* Get a minor range for your devices from the usb maintainer */
#define USB_BLINK_MINOR_BASE	0 

/* Structure to hold all of our device specific stuff */
struct usb_blink {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct kref		kref;
};
#define to_blink_dev(d) container_of(d, struct usb_blink, kref)

static struct usb_driver blink_driver;

/* 
 * Free up the usb_blink structure and
 * decrement the usage count associated with the usb device 
 */
static void blink_delete(struct kref *kref)
{
	struct usb_blink *dev = to_blink_dev(kref);

	usb_put_dev(dev->udev);
	kfree(dev);
}

/* Called when a user program invokes the open() system call on the device */
static int blink_open(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	
	/* Obtain reference to USB interface from minor number */
	interface = usb_find_interface(&blink_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
			__func__, subminor);
		return -ENODEV;
	}

	/* Obtain driver data associated with the USB interface */
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* save our object in the file's private structure */
	file->private_data = dev;

	return retval;
}

/* Called when a user program invokes the close() system call on the device */
static int blink_release(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* decrement the count on our device */
	kref_put(&dev->kref, blink_delete);
	return 0;
}


#define NR_LEDS 8
#define NR_BYTES_BLINK_MSG 6



#define NR_SAMPLE_COLORS 4

unsigned int sample_colors[]={0x000011, 0x110000, 0x001100, 0x000000};

static int parse_led_string(const char *buffer, size_t len, unsigned int *colors)
{
    int led_num;
    unsigned int color;
    char *token, *ptr, *input;
    int i;

    // Inicialmente, apagar todos los LEDs (asignar color negro)
    for (i = 0; i < NR_LEDS; i++) {
        colors[i] = 0x000000;
    }
	
	if (len == 1){ // Si es espacio en blanco se para la ejecucion de la funcion aqui para que siga
		return 0; 
	}

    // Copiar buffer a una cadena temporal, ya que `strsep` modifica la cadena
    input = kmalloc(len + 1, GFP_KERNEL);
    if (!input) return -ENOMEM;

    strncpy(input, buffer, len);
    input[len] = '\0';
	
    // Parsear cada token "numled:color"
    while ((token = strsep(&input, ",")) != NULL) {
        // Comprobar que el token tiene el formato correcto
        ptr = strchr(token, ':');
        if (!ptr) {
            kfree(input);
            return -EINVAL; // Formato incorrecto
        }

        *ptr = '\0';
        
        // Convertir el número de LED y el color
        if (kstrtoint(token, 10, &led_num) < 0 || led_num < 0 || led_num >= NR_LEDS) {
            kfree(input);
            return -EINVAL; // Número de LED fuera de rango o error de conversión
        }

        if (kstrtouint(ptr + 1, 16, &color) < 0) {
            kfree(input);
            return -EINVAL; // Error en el formato del color
        }

        // Asignar el color al LED correspondiente
        colors[led_num] = color;
    }

    kfree(input);
    return 0; // Éxito
}

/* Called when a user program invokes the write() system call on the device */
static ssize_t blink_write(struct file *file, const char *user_buffer,
                           size_t len, loff_t *off)
{
    struct usb_blink *dev = file->private_data;
    int retval = 0, i;
    unsigned int colors[NR_LEDS]; // Array para los colores de cada LED
    unsigned char *message;
    
    // Reservar memoria para el mensaje que enviaremos a Blinkstick
    message = kmalloc(NR_BYTES_BLINK_MSG, GFP_DMA);
    if (!message) return -ENOMEM;

    // Copiar buffer del usuario a una cadena temporal
    char *kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) {
        kfree(message);
        return -ENOMEM;
    }
    
    if (copy_from_user(kbuf, user_buffer, len)) {
        kfree(message);
        kfree(kbuf);
        return -EFAULT;
    }
    kbuf[len] = '\0';

    // Llamar a la función de parseo
    retval = parse_led_string(kbuf, len, colors);
    kfree(kbuf);
    if (retval) {
        kfree(message);
        return retval;
    }

    //Enviar colores
    for (i = 0; i < NR_LEDS; i++) {
        unsigned int color = colors[i];

        memset(message, 0, NR_BYTES_BLINK_MSG);
        message[0] = 0x05;
        message[1] = 0x00;
        message[2] = i; // Número de LED
        message[3] = (color >> 16) & 0xFF; 
        message[4] = (color >> 8) & 0xFF;  
        message[5] = color & 0xFF;         

        // Enviar mensaje a Blinkstick
        retval = usb_control_msg(dev->udev,usb_sndctrlpipe(dev->udev, 0),USB_REQ_SET_CONFIGURATION,USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_DEVICE,0x5, 0, message, NR_BYTES_BLINK_MSG, 0);

        if (retval < 0) {
            printk(KERN_ALERT "Error en usb_control_msg: %d\n", retval);
            kfree(message);
            return retval;
        }
    }

    kfree(message);
    (*off) += len;
    return len;
}



/*
 * Operations associated with the character device 
 * exposed by driver
 * 
 */
static const struct file_operations blink_fops = {
	.owner =	THIS_MODULE,
	.write =	blink_write,	 	/* write() operation on the file */
	.open =		blink_open,			/* open() operation on the file */
	.release =	blink_release, 		/* close() operation on the file */
};

/* 
 * Return permissions and pattern enabling udev 
 * to create device file names under /dev
 * 
 * For each blinkstick connected device a character device file
 * named /dev/usb/blinkstick<N> will be created automatically  
 */
char* set_device_permissions(__cconst__ struct device *dev, umode_t *mode) 
{
	if (mode)
		(*mode)=0666; /* RW permissions */
 	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev)); /* Return formatted string */
}


/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver blink_class = {
	.name =		"blinkstick%d",  /* Pattern used to create device files */	
	.devnode=	set_device_permissions,	
	.fops =		&blink_fops,
	.minor_base =	USB_BLINK_MINOR_BASE,
};

/*
 * Invoked when the USB core detects a new
 * blinkstick device connected to the system.
 */
static int blink_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_blink *dev;
	int retval = -ENOMEM;

	/*
 	 * Allocate memory for a usb_blink structure.
	 * This structure represents the device state.
	 * The driver assigns a separate structure to each blinkstick device
 	 *
	 */
	dev = kmalloc(sizeof(struct usb_blink),GFP_KERNEL);

	if (!dev) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	/* Initialize the various fields in the usb_blink structure */
	kref_init(&dev->kref);
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &blink_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
			"Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */	
	dev_info(&interface->dev,
		 "Blinkstick device now attached to blinkstick-%d",
		 interface->minor);
	return 0;

error:
	if (dev)
		/* this frees up allocated memory */
		kref_put(&dev->kref, blink_delete);
	return retval;
}

/*
 * Invoked when a blinkstick device is 
 * disconnected from the system.
 */
static void blink_disconnect(struct usb_interface *interface)
{
	struct usb_blink *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &blink_class);

	/* prevent more I/O from starting */
	dev->interface = NULL;

	/* decrement our usage count */
	kref_put(&dev->kref, blink_delete);

	dev_info(&interface->dev, "Blinkstick device #%d has been disconnected", minor);
}

/* Define these values to match your devices */
#define BLINKSTICK_VENDOR_ID	0X20A0
#define BLINKSTICK_PRODUCT_ID	0X41E5

/* table of devices that work with this driver */
static const struct usb_device_id blink_table[] = {
	{ USB_DEVICE(BLINKSTICK_VENDOR_ID,  BLINKSTICK_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, blink_table);

static struct usb_driver blink_driver = {
	.name =		"blinkstick",
	.probe =	blink_probe,
	.disconnect =	blink_disconnect,
	.id_table =	blink_table,
};

/* Module initialization */
int blinkdrv_module_init(void)
{
   return usb_register(&blink_driver);
}

/* Module cleanup function */
void blinkdrv_module_cleanup(void)
{
  usb_deregister(&blink_driver);
}

module_init(blinkdrv_module_init);
module_exit(blinkdrv_module_cleanup);

