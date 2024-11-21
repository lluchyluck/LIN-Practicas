#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/errno.h>

MODULE_DESCRIPTION("Display7s sincro");
MODULE_AUTHOR("Lucas Calzada y Juan Girón");
MODULE_LICENSE("GPL");

#define DS_A 0x80
#define DS_B 0x40
#define DS_C 0x20
#define DS_D 0x10
#define DS_E 0x08
#define DS_F 0x04
#define DS_G 0x02
#define DS_DP 0x01
#define SEGMENT_COUNT 8

static const unsigned char hex_to_segments[16] = {
    DS_A | DS_B | DS_C | DS_D | DS_E | DS_F,
    DS_B | DS_C,
    DS_A | DS_B | DS_D | DS_E | DS_G,
    DS_A | DS_B | DS_C | DS_D | DS_G,
    DS_B | DS_C | DS_F | DS_G,
    DS_A | DS_C | DS_D | DS_F | DS_G,
    DS_A | DS_C | DS_D | DS_E | DS_F | DS_G,
    DS_A | DS_B | DS_C,
    DS_A | DS_B | DS_C | DS_D | DS_E | DS_F | DS_G,
    DS_A | DS_B | DS_C | DS_D | DS_F | DS_G,
    DS_A | DS_B | DS_C | DS_E | DS_F | DS_G,
    DS_C | DS_D | DS_E | DS_F | DS_G,
    DS_A | DS_D | DS_E | DS_F,
    DS_B | DS_C | DS_D | DS_E | DS_G,
    DS_A | DS_D | DS_E | DS_F | DS_G,
    DS_A | DS_E | DS_F | DS_G
};

/* GPIOs del display */
const int display_gpio[] = {18, 23, 24};
#define NR_GPIO_DISPLAY ARRAY_SIZE(display_gpio)
struct gpio_desc *gpio_descriptors[NR_GPIO_DISPLAY];

/* Sincronización */
static DEFINE_SEMAPHORE(sem_OR);      // Control de acceso exclusivo
static DEFINE_MUTEX(write_mutex);         // Exclusión mutua para `write()`
static DEFINE_SPINLOCK(spin_count);   // Protección para contador de referencias
static int contador_referencias = 0;          // Contador de referencias


static void update_7sdisplay(unsigned char data)
{
    int i;

    gpiod_set_value(gpio_descriptors[1], 0);
    gpiod_set_value(gpio_descriptors[2], 0);

    for (i = 0; i < SEGMENT_COUNT; i++) {
        gpiod_set_value(gpio_descriptors[0], (data & (0x80 >> i)) ? 1 : 0);
        gpiod_set_value(gpio_descriptors[2], 1);
        msleep(1);
        gpiod_set_value(gpio_descriptors[2], 0);
    }

    gpiod_set_value(gpio_descriptors[1], 1);
    msleep(1);
    gpiod_set_value(gpio_descriptors[1], 0);
}

/* Implementación de operaciones */
static ssize_t display7s_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
    unsigned char digit;
    unsigned char segment_data;

    if (len != 2)
        return -EINVAL;

    if (copy_from_user(&digit, buff, 1))
        return -EFAULT;

    if (digit >= 'a' && digit <= 'f')
        digit -= 32;

    if (digit >= '0' && digit <= '9')
        segment_data = hex_to_segments[digit - '0'];
    else if (digit >= 'A' && digit <= 'F')
        segment_data = hex_to_segments[digit - 'A' + 10];
    else
        return -EINVAL;

    mutex_lock(&write_mutex);
    update_7sdisplay(segment_data);
    mutex_unlock(&write_mutex);

    return len;
}

static int display7s_open(struct inode *inode, struct file *file)
{
    if(!try_module_get(THIS_MODULE))
        return -EBUSY;
    if (down_interruptible(&sem_OR))
        return -EBUSY;

    spin_lock(&spin_count);
    contador_referencias++;
    spin_unlock(&spin_count);

    return 0;
}

static int display7s_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    spin_lock(&spin_count);
    contador_referencias--;
    spin_unlock(&spin_count);
    
    up(&sem_OR);
    return 0;
}

/* Operaciones de dispositivo */
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = display7s_write,
    .open = display7s_open,
    .release = display7s_release,
};

/* Dispositivo misc */
static struct miscdevice display7s_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "display7s",
    .fops = &fops,
    .mode = 0666,
};


static int __init display7s_misc_init(void)
{
    int i, err;

    for (i = 0; i < NR_GPIO_DISPLAY; i++) {
        err = gpio_request(display_gpio[i], "display7s");
        if (err)
            return err;
        gpio_descriptors[i] = gpio_to_desc(display_gpio[i]);
        gpiod_direction_output(gpio_descriptors[i], 0);
    }

    err = misc_register(&display7s_misc);
    if (err)
        return err;

    return 0;
}

static void __exit display7s_misc_exit(void)
{
    int i;

    spin_lock(&spin_count);
    if (contador_referencias > 0) {
        spin_unlock(&spin_count);
        pr_err("Device is in use. Cannot unload module.\n");
        return;
    }
    spin_unlock(&spin_count);

    misc_deregister(&display7s_misc);

    for (i = 0; i < NR_GPIO_DISPLAY; i++)
        gpio_free(display_gpio[i]);
}

module_init(display7s_misc_init);
module_exit(display7s_misc_exit);
