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

#define DEVICE_NAME "buzzer"
#define PWM_DEVICE_NAME "pwmchip0"

struct pwm_device *pwm_device = NULL;
struct pwm_state pwm_state;
static char *melody_buffer;
static struct mutex buzzer_mutex;
static struct work_struct my_work;

struct music_step {
    int frequency;
    int duration;
};

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
	while (duration) {
		current_duration = (duration) & (1 << i);

		if (current_duration) {
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
static inline unsigned int freq_to_period_ns(unsigned int frequency)
{
	if (frequency == 0)
		return 0;
	else
		return DIV_ROUND_CLOSEST_ULL(100000000000UL, frequency);
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
static void my_wq_function(struct work_struct *work)
{
    static struct music_step steps[100];
    size_t step_count;
    const int beat = 120;

    parse_notes(melody_buffer, steps, &step_count);

    pwm_init_state(pwm_device, &pwm_state);
    size_t i;
    for (i = 0; i < step_count; i++) {
        pwm_state.period = freq_to_period_ns(steps[i].frequency);

        pwm_disable(pwm_device);

        if (pwm_state.period > 0) {
            pwm_set_relative_duty_cycle(&pwm_state, 70, 100);
            pwm_state.enabled = true;
            pwm_apply_state(pwm_device, &pwm_state);
        } else {
            pwm_disable(pwm_device);
        }

        msleep(calculate_delay_ms(steps[i].duration, beat));
        printk(KERN_INFO "Printeando nota: %d\n", steps[i].frequency);
    }

    pwm_disable(pwm_device);
}


static ssize_t buzzer_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    if (len > PAGE_SIZE) {
        return -EINVAL;
    }

    melody_buffer = kzalloc(len + 1, GFP_KERNEL);
    if (!melody_buffer) {
        return -ENOMEM;
    }

    if (copy_from_user(melody_buffer, buffer, len)) {
        kfree(melody_buffer);
        return -EFAULT;
    }
    printk(KERN_INFO "Se ha detectado escritura");
    schedule_work(&my_work);
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = buzzer_write,
    .open = buzzer_open,
    .release = buzzer_release,
};
static struct miscdevice buzzer_misc = {
	.minor = MISC_DYNAMIC_MINOR, /* kernel dynamically assigns a free minor# */
	.name = DEVICE_NAME,		 /* when misc_register() is invoked, the kernel
								  * will auto-create device file;
								  * also populated within /sys/class/misc/ and /sys/devices/virtual/misc/ */
	.mode = 0666,				 /* ... dev node perms set as specified here */
	.fops = &fops,				 /* connect to this driver's 'functionality' */
};

static int __init buzzer_init(void) {
    int result;
    int err = 0;
    struct device *device;

    /* Request utilization of PWM0 device */
	pwm_device = pwm_request(0, PWM_DEVICE_NAME);

	if (IS_ERR(pwm_device))
		return PTR_ERR(pwm_device);


    err = misc_register(&buzzer_misc);
	if (err) {
        pr_err("Failed to register misc device\n");
        pwm_free(pwm_device);
        return err;
    }


	device = buzzer_misc.this_device;

	dev_info(device, "Display7s driver registered succesfully. To talk to\n");
	dev_info(device, "the driver try to cat and echo to /dev/%s.\n", DEVICE_NAME);
	dev_info(device, "Remove the module when done.\n");

    /* Initialize work structure (with function) */
	INIT_WORK(&my_work, my_wq_function);

    mutex_init(&buzzer_mutex);

    printk(KERN_INFO "buzzer: Module loaded\n");
    return 0;
}

static void __exit buzzer_exit(void) {
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
