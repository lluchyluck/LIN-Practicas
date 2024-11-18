#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>

#define SUCCESS 0
#define DEVICE_NAME "leds"
#define CLASS_NAME "led"
#define MAX_LED_VALUE 7
#define MIN_LED_VALUE 0
#define BUF_LEN 80      /* Max length of the message from the device */
#define NR_GPIO_LEDS 3


static struct class* ledClass = NULL;
static struct device* ledDevice = NULL;
static int led_value = 0; // Estado inicial de los LEDs

const int led_gpio[NR_GPIO_LEDS]={25,27,4};
struct gpio_desc* gpio_descriptors[NR_GPIO_LEDS];

static ssize_t dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset);


// Definición de las operaciones del dispositivo
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write,
};

static struct miscdevice misc_leds = {
    .minor = MISC_DYNAMIC_MINOR,    /* kernel dynamically assigns a free minor# */
    .name = DEVICE_NAME, /* when misc_register() is invoked, the kernel*/
    .mode = 0666,     /* ... dev node perms set as specified here */
    .fops = &fops,    /* connect to this driver's 'functionality' */
};


static inline int set_pi_leds(unsigned int mask){
    int i;
    for (i=0;i<NR_GPIO_LEDS;i++)
        gpiod_set_value(gpio_descriptors[i], (mask>>i) & 0x1 );
    return 0;
}
// Función de escritura para el dispositivo /dev/leds
static ssize_t dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset) {
    int value;
    char kbuffer[4];

    if (len > 3) {  // Solo aceptamos hasta un valor de 3 caracteres
        return -EINVAL;
    }

    if (copy_from_user(kbuffer, buffer, len)) {
        return -EFAULT;
    }
    
    kbuffer[len] = '\0'; // Añadir terminador nulo para convertir en cadena C

    // Convertir la cadena a un número entero
    if (kstrtoint(kbuffer, 10, &value) != 0) {
        return -EINVAL;
    }

    // Verificar si el valor está en el rango 0-7
    if (value < MIN_LED_VALUE || value > MAX_LED_VALUE) {
        return -EINVAL;
    }

    led_value = value;

    set_pi_leds(led_value);

    return len;
}



// Función de inicialización del módulo
static int __init leds_init(void) {
    int ret;
    int i;
    int err = 0;
    char gpio_str[10];

    ret = misc_register(&misc_leds);
    if (ret) {
        pr_err("No se puede registrar misc_leds\n");
        return ret;
    }

    for (i = 0; i < NR_GPIO_LEDS; i++) {
        sprintf(gpio_str, "led_%d", i);

         if ((err = gpio_request(led_gpio[i], gpio_str))) {
            pr_err("Failed GPIO %d request\n", led_gpio[i]);
            err = -EINVAL;
            goto err_handle;
        }

        gpio_descriptors[i] = gpio_to_desc(led_gpio[i]);
        if (!gpio_descriptors[i]) {
            pr_err("GPIO %d is not valid\n", led_gpio[i]);  
            err = -EINVAL;
            goto err_handle;
        }
        gpiod_direction_output(gpio_descriptors[i], 0);
    }

    pr_info("LED device registered\n");
    return 0;

err_handle:
    while (i--) {
        gpio_free(led_gpio[i]);
    }
    misc_deregister(&misc_leds);
    return err;
}

// Función de limpieza del módulo
static void __exit leds_exit(void)
{
    int i;

    set_pi_leds(MIN_LED_VALUE);
    for (i = 0; i < NR_GPIO_LEDS; i++) {
        gpiod_put(gpio_descriptors[i]);
    }
    misc_deregister(&misc_leds);
    pr_info("LED device unregistered\n");

}

module_init(leds_init);
module_exit(leds_exit);

MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Lucas Calzada Y Juan Girón"); 
MODULE_DESCRIPTION("Driver de dispositivo de caracteres para controlar los LEDs D1-D3 de la placa Bee");

