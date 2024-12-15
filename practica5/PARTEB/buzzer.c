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
#include <linux/timer.h>

#define DEVICE_NAME "buzzer"
#define PWM_DEVICE_NAME "pwmchip0"

struct music_step {
    int frequency;
    int duration;
};

struct pwm_device *pwm_device = NULL;
struct pwm_state pwm_state;
static char *melody_buffer;
static int beat = 120; // Default beat value
static int current_note = 0; // Current note being played
static size_t step_count = 0;
static struct music_step steps[100]; // Parsed melody
static struct mutex buzzer_mutex;
static struct work_struct my_work;
static struct timer_list melody_timer;

static inline int calculate_delay_ms(unsigned int note_len, unsigned int qnote_ref)
{
    unsigned char duration = (note_len & 0x7f);
    unsigned char triplet = (note_len & 0x80);
    unsigned char i = 0;
    unsigned char current_duration;
    int total = 0;

    while (duration) {
        current_duration = (duration) & (1 << i);

        if (current_duration) {
            if (triplet)
                current_duration = (current_duration * 3) / 2;
            total += (240000) / (qnote_ref * current_duration);
            duration &= ~(1 << i);
        }
        i++;
    }
    return total;
}

static inline unsigned int freq_to_period_ns(unsigned int frequency)
{
    if (frequency == 0)
        return 0;
    else
        return DIV_ROUND_CLOSEST_ULL(100000000000UL, frequency);
}

static void play_next_note(struct work_struct *work);

static void timer_callback(struct timer_list *timer)
{
    if (current_note < step_count) {
        schedule_work(&my_work);
    } else {
        pwm_disable(pwm_device);
    }
}

static void play_next_note(struct work_struct *work)
{
    if (current_note >= step_count) {
        pwm_disable(pwm_device);
        return;
    }

    pwm_state.period = freq_to_period_ns(steps[current_note].frequency);
    pwm_disable(pwm_device);

    if (pwm_state.period > 0) {
        pwm_set_relative_duty_cycle(&pwm_state, 70, 100);
        pwm_state.enabled = true;
        pwm_apply_state(pwm_device, &pwm_state);
    } else {
        pwm_disable(pwm_device);
    }

    mod_timer(&melody_timer, jiffies + msecs_to_jiffies(calculate_delay_ms(steps[current_note].duration, beat)));
    current_note++;
}

static void parse_notes(const char *input_buffer, struct music_step *steps, size_t *step_count)
{
    char *local_buffer;
    char *note;
    size_t index = 0;

    local_buffer = kstrdup(input_buffer, GFP_KERNEL);
    if (!local_buffer) {
        printk(KERN_ERR "Failed to allocate memory for local buffer\n");
        *step_count = 0;
        return;
    }

    while ((note = strsep(&local_buffer, ",")) != NULL) {
        if (sscanf(note, "%d:%x", &steps[index].frequency, &steps[index].duration) == 2) {
            index++;
        }
    }

    *step_count = index;
    kfree(local_buffer);
}

static int buzzer_open(struct inode *inode, struct file *file) {
    if (!mutex_trylock(&buzzer_mutex)) {
        return -EBUSY;
    }
    return 0;
}

static int buzzer_release(struct inode *inode, struct file *file) {
    mutex_unlock(&buzzer_mutex);
    return 0;
}

static ssize_t buzzer_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    char *kbuf;

    if (len > PAGE_SIZE) {
        return -EINVAL;
    }

    kbuf = kzalloc(len + 1, GFP_KERNEL);
    if (!kbuf) {
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buffer, len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    if (strncmp(kbuf, "beat", 4) == 0) {
        int new_beat;
        if (sscanf(kbuf + 5, "%d", &new_beat) == 1) {
            beat = new_beat;
            printk(KERN_INFO "buzzer: Beat updated to %d\n", beat);
        } else {
            printk(KERN_ERR "buzzer: Invalid beat value\n");
            kfree(kbuf);
            return -EINVAL;
        }
    } else if (strncmp(kbuf, "music", 5) == 0) {
        melody_buffer = kzalloc(len - 5 + 1, GFP_KERNEL);
        if (!melody_buffer) {
            kfree(kbuf);
            return -ENOMEM;
        }
        strcpy(melody_buffer, kbuf + 6);
        printk(KERN_INFO "buzzer: Melody received\n");

        parse_notes(melody_buffer, steps, &step_count);
        current_note = 0;

        schedule_work(&my_work);
    } else {
        printk(KERN_ERR "buzzer: Unknown command\n");
        kfree(kbuf);
        return -EINVAL;
    }

    kfree(kbuf);
    return len;
}

static ssize_t buzzer_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
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
    .open = buzzer_open,
    .release = buzzer_release,
};

static struct miscdevice buzzer_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .mode = 0666,
    .fops = &fops,
};

static int __init buzzer_init(void) {
    int err;

    pwm_device = pwm_request(0, PWM_DEVICE_NAME);
    if (IS_ERR(pwm_device))
        return PTR_ERR(pwm_device);

    err = misc_register(&buzzer_misc);
    if (err) {
        pr_err("Failed to register misc device\n");
        pwm_free(pwm_device);
        return err;
    }

    INIT_WORK(&my_work, play_next_note);
    timer_setup(&melody_timer, timer_callback, 0);
    mutex_init(&buzzer_mutex);

    printk(KERN_INFO "buzzer: Module loaded\n");
    return 0;
}

static void __exit buzzer_exit(void) {
    del_timer_sync(&melody_timer);
    flush_work(&my_work);
    pwm_free(pwm_device);
    misc_deregister(&buzzer_misc);
    mutex_destroy(&buzzer_mutex);
    printk(KERN_INFO "buzzer: Module unloaded\n");
}

module_init(buzzer_init);
module_exit(buzzer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tu Nombre");
MODULE_DESCRIPTION("Driver SMP-safe para el buzzer de la placa Bee v2.0");
