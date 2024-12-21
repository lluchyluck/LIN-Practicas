#include <linux/module.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h> /* For fg_console */
#include <linux/kd.h>  /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/spinlock.h>

#define DEVICE_NAME "buzzer"
#define PWM_DEVICE_NAME "pwmchip0"

struct pwm_device *pwm_device = NULL;
struct pwm_state pwm_state;

/* Work descriptor */
struct work_struct my_work;

/* Structure to represent a note or rest in a melodic line  */
struct music_step
{
    int frequency; /* Frequency in centihertz */
    int duration;  /* Duration of the note */
};

/* Melody */
static int beat = 120;       // Default beat value
static struct music_step steps[100]; // Parsed melody

static spinlock_t lock; /* Cerrojo para proteger actualización/consulta de variables buzzer_state y buzzer_request */
static struct music_step *next = NULL;

typedef enum
{
    BUZZER_STOPPED, /* Buzzer no reproduce nada (la melodía terminó o no ha comenzado) */
    BUZZER_PAUSED,  /* Reproducción pausada por el usuario */
    BUZZER_PLAYING  /* Buzzer reproduce actualmente la melodía */
} buzzer_state_t;

static buzzer_state_t buzzer_state = BUZZER_STOPPED; /* Estado actual de la reproducción */

typedef enum
{
    REQUEST_START,  /* Usuario pulsó SW1 durante estado BUZZER_STOPPED */
    REQUEST_RESUME, /* Usuario pulsó SW1 durante estado BUZZER_PAUSED */
    REQUEST_PAUSE,  /* Usuario pulsó SW1 durante estado BUZZER_PLAYING */
    REQUEST_CONFIG, /* Usuario está configurando actualmente una nueva melodía vía /dev/buzzer */
    REQUEST_NONE    /* Indicador de petición ya gestionada (a establecer por tarea diferida) */
} buzzer_request_t;
static buzzer_request_t buzzer_request = REQUEST_NONE;

/* BUTTON */

#define MANUAL_DEBOUNCE
#define GPIO_BUTTON 22
struct gpio_desc *desc_button = NULL;
static int gpio_button_irqn = -1;

/* TIMER */

static struct timer_list my_timer;

/************************************ DECLARATIONS *************************************/

static inline unsigned int freq_to_period_ns(unsigned int frequency);
static inline int is_end_marker(struct music_step *step);
static inline int calculate_delay_ms(unsigned int note_len, unsigned int qnote_ref);
static void parse_notes(const char *input_buffer, struct music_step *steps, size_t *step_count);
static void timer_handler(struct timer_list *timer);
static irqreturn_t button_irq_handler(int irq, void *dev_id);
static void my_wq_function(struct work_struct *work);
static ssize_t buzzer_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset);
static ssize_t buzzer_read(struct file *file, char __user *buffer, size_t len, loff_t *offset);
static int __init buzzer_init(void);
static void __exit buzzer_exit(void);

/************************************ DEFINITIONS *************************************/

/* Transform frequency in centiHZ into period in nanoseconds */
static inline unsigned int freq_to_period_ns(unsigned int frequency)
{
    if (frequency == 0)
        return 0;
    else
        return DIV_ROUND_CLOSEST_ULL(100000000000UL, frequency);
}

/* Check if the current step is and end marker */
static inline int is_end_marker(struct music_step *step)
{
    return (step->frequency == 0 && step->duration == 0);
}

/**
 *  Transform note length into ms,
 * taking the beat of a quarter note as reference
 */
static inline int calculate_delay_ms(unsigned int note_len, unsigned int qnote_ref)
{
    unsigned char duration = (note_len & 0x7f);
    unsigned char triplet = (note_len & 0x80);
    unsigned char i = 0;
    unsigned char current_duration;
    int total = 0;

    /* Calculate the total duration of the note
     * as the summation of the figures that make
     * up this note (bits 0-6)
     */
    while (duration)
    {
        current_duration = (duration) & (1 << i);

        if (current_duration)
        {
            /* Scale note accordingly */
            if (triplet)
                current_duration = (current_duration * 3) / 2;
            /*
             * 24000/qnote_ref denote number of ms associated
             * with a whole note (redonda)
             */
            total += (240000) / (qnote_ref * current_duration);
            /* Clear bit */
            duration &= ~(1 << i);
        }
        i++;
    }
    return total;
}

static void parse_notes(const char *input_buffer, struct music_step *steps, size_t *step_count)
{
    char *local_buffer;
    char *note;
    size_t index = 0;

    local_buffer = kstrdup(input_buffer, GFP_KERNEL);
    if (!local_buffer)
    {
        printk(KERN_ERR "Failed to allocate memory for local buffer\n");
        *step_count = 0;
        return;
    }

    while ((note = strsep(&local_buffer, ",")) != NULL)
    {
        if (sscanf(note, "%d:%x", &steps[index].frequency, &steps[index].duration) == 2)
        {
            index++;
        }
    }

    steps[index+1].frequency = 0;
    steps[index+1].duration = 0;

    *step_count = index;
    kfree(local_buffer);
}

static void timer_handler(struct timer_list *timer)
{
    unsigned long flags;

    spin_lock_irqsave(&lock, flags);
    if (buzzer_state == BUZZER_PLAYING)
    {
        schedule_work(&my_work);
    }
    spin_unlock_irqrestore(&lock, flags);
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    unsigned long flags;

#ifdef MANUAL_DEBOUNCE
    static unsigned long last_interrupt = 0;
    unsigned long diff = jiffies - last_interrupt;
    if (diff < 20)
        return IRQ_HANDLED;

    last_interrupt = jiffies;
#endif

    spin_lock_irqsave(&lock, flags);
    if (buzzer_state == BUZZER_STOPPED)
    {
        buzzer_request = REQUEST_START;
        schedule_work(&my_work);
    }
    else if (buzzer_state == BUZZER_PAUSED)
    {
        buzzer_request = REQUEST_RESUME;
        schedule_work(&my_work);
    }
    else if (buzzer_state == BUZZER_PLAYING)
    {
        buzzer_request = REQUEST_PAUSE;
        schedule_work(&my_work);
    }
    spin_unlock_irqrestore(&lock, flags);

    return IRQ_HANDLED;
}

/* Work's handler function */
static void my_wq_function(struct work_struct *work)
{
    unsigned long flags;

    spin_lock_irqsave(&lock, flags);

    if (buzzer_request == REQUEST_CONFIG)
    {
        pwm_disable(pwm_device);
        buzzer_state = BUZZER_STOPPED;
    }
    else if (buzzer_request == REQUEST_START)
    {
        next = steps;
        buzzer_state = BUZZER_PLAYING;
    }
    else if (buzzer_request == REQUEST_PAUSE)
    {
        pwm_disable(pwm_device);
        buzzer_state = BUZZER_PAUSED;
    }
    else if (buzzer_request == REQUEST_RESUME)
    {
        buzzer_state = BUZZER_PLAYING;
    }

    buzzer_request = REQUEST_NONE;

    if (buzzer_state == BUZZER_PLAYING)
    {
        if (is_end_marker(next))
        {
            buzzer_state = BUZZER_STOPPED;
            pwm_disable(pwm_device);
        }
        else
        {

            /* Obtain period from frequency */
            pwm_state.period = freq_to_period_ns(next->frequency);

            /**
             * Disable temporarily to allow repeating the same consecutive
             * notes in the melodic line
             **/
            pwm_disable(pwm_device);

            /* If period==0, its a rest (silent note) */
            if (pwm_state.period > 0)
            {
                /* Set duty cycle to 70 to maintain the same timbre */
                pwm_set_relative_duty_cycle(&pwm_state, 70, 100);
                pwm_state.enabled = true;
                /* Apply state */
                pwm_apply_state(pwm_device, &pwm_state);
            }
            else
            {
                /* Disable for rest */
                pwm_disable(pwm_device);
            }

            mod_timer(&my_timer, jiffies + msecs_to_jiffies(calculate_delay_ms(next->duration, beat)));

            next++;
        }
    }

    spin_unlock_irqrestore(&lock, flags);
}

static ssize_t buzzer_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset)
{
    char *kbuf;
    unsigned long flags;
    char *melody_buffer;
    size_t step_count = 0;

    if (len > PAGE_SIZE)
    {
        return -EINVAL;
    }

    if (buzzer_state == BUZZER_PLAYING)
    {
        return -EBUSY;
    }

    spin_lock_irqsave(&lock, flags);

    buzzer_request = REQUEST_CONFIG;

    kbuf = kzalloc(len + 1, GFP_KERNEL);
    if (!kbuf)
    {
        spin_unlock_irqrestore(&lock, flags);
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buffer, len))
    {
        kfree(kbuf);
        spin_unlock_irqrestore(&lock, flags);
        return -EFAULT;
    }

    if (strncmp(kbuf, "beat", 4) == 0)
    {
        int new_beat;
        if (sscanf(kbuf + 5, "%d", &new_beat) == 1)
        {
            beat = new_beat;
            printk(KERN_INFO "buzzer: Beat updated to %d\n", beat);
        }
        else
        {
            printk(KERN_ERR "buzzer: Invalid beat value\n");
            kfree(kbuf);
            spin_unlock_irqrestore(&lock, flags);
            return -EINVAL;
        }
    }
    else if (strncmp(kbuf, "music", 5) == 0)
    {
        melody_buffer = kzalloc(len - 5 + 1, GFP_KERNEL);
        if (!melody_buffer)
        {
            kfree(kbuf);
            spin_unlock_irqrestore(&lock, flags);
            return -ENOMEM;
        }
        strcpy(melody_buffer, kbuf + 6);
        printk(KERN_INFO "buzzer: Melody received\n");

        parse_notes(melody_buffer, steps, &step_count);

        schedule_work(&my_work);
    }
    else
    {
        printk(KERN_ERR "buzzer: Unknown command\n");
        kfree(kbuf);
        spin_unlock_irqrestore(&lock, flags);
        return -EINVAL;
    }

    kfree(kbuf);
    spin_unlock_irqrestore(&lock, flags);

    return len;
}

static ssize_t buzzer_read(struct file *file, char __user *buffer, size_t len, loff_t *offset)
{
    char kbuf[32];
    int ret;

    snprintf(kbuf, sizeof(kbuf), "beat=%d\n", beat);
    ret = simple_read_from_buffer(buffer, len, offset, kbuf, strlen(kbuf));

    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = buzzer_write,
    .read = buzzer_read,
    //.open = buzzer_open,
    //.release = buzzer_release,
};

static struct miscdevice buzzer_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .mode = 0666,
    .fops = &fops,
};

static int __init buzzer_init(void)
{
    int err;
    int gpio_out_ok = 0;

    spin_lock_init(&lock);

    pwm_device = pwm_request(0, PWM_DEVICE_NAME);
    if (IS_ERR(pwm_device))
        return PTR_ERR(pwm_device);

    err = misc_register(&buzzer_misc);
    if (err)
    {
        pr_err("Failed to register misc device\n");
        pwm_free(pwm_device);
        return err;
    }

    /* Requesting Button's GPIO */
    if ((err = gpio_request(GPIO_BUTTON, "button")))
    {
        pr_err("ERROR: GPIO %d request\n", GPIO_BUTTON);
        goto err_handle;
    }

    /* Configure Button */
    if (!(desc_button = gpio_to_desc(GPIO_BUTTON)))
    {
        pr_err("GPIO %d is not valid\n", GPIO_BUTTON);
        err = -EINVAL;
        goto err_handle;
    }

    gpio_out_ok = 1;

    // configure the BUTTON GPIO as input
    gpiod_direction_input(desc_button);

    INIT_WORK(&my_work, my_wq_function);
    timer_setup(&my_timer, timer_handler, 0);

#ifndef MANUAL_DEBOUNCE
    // Debounce the button with a delay of 200ms
    if (gpiod_set_debounce(desc_button, 200) < 0)
    {
        pr_err("ERROR: gpio_set_debounce - %d\n", GPIO_BUTTON);
        goto err_handle;
    }
#endif

    // Get the IRQ number for our GPIO
    gpio_button_irqn = gpiod_to_irq(desc_button);
    pr_info("IRQ Number = %d\n", gpio_button_irqn);

    if (request_irq(gpio_button_irqn,    // IRQ number
                    button_irq_handler,  // IRQ handler
                    IRQF_TRIGGER_RISING, // Handler will be called in raising edge
                    "button_leds",       // used to identify the device name using this IRQ
                    NULL))
    { // device id for shared IRQ
        pr_err("my_device: cannot register IRQ ");
        goto err_handle;
    }

    printk(KERN_INFO "buzzer: Module loaded\n");

    return 0;
err_handle:

    if (gpio_out_ok)
        gpiod_put(desc_button);

    return err;
}

static void __exit buzzer_exit(void)
{
    del_timer_sync(&my_timer);
    flush_work(&my_work);
    pwm_free(pwm_device);
    misc_deregister(&buzzer_misc);
    free_irq(gpio_button_irqn, NULL);
    gpiod_put(desc_button);
    printk(KERN_INFO "buzzer: Module unloaded\n");
}

module_init(buzzer_init);
module_exit(buzzer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Juan Girón y Lucas Calzada");
MODULE_DESCRIPTION("Driver SMP-safe para el buzzer de la placa Bee v2.0");