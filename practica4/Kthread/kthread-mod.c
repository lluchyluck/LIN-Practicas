#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>

MODULE_DESCRIPTION("Kthread-mod Module - FDI-UCM");
MODULE_AUTHOR("Juan Carlos Saez");
MODULE_LICENSE("GPL");

struct task_struct* kthread = NULL;

static int kthread_periodic_fun(void *arg) {

	static char flag = 0;
	char* message[] = {"Tic", "Tac"};

	/* Associate FIFO policy with kthread  */
	sched_set_fifo(current);

	while (!kthread_should_stop()) {

		if (flag == 0)
			printk(KERN_INFO "%s\n", message[0]);
		else
			printk(KERN_INFO "%s\n", message[1]);

		flag = ~flag;

		/* Go to sleep for 1 second */
		msleep(1000);
	}

	pr_info("Periodic k-thread terminated.\n");

	return 0;
}


int kthread_module_init(void)
{
	/* Create kthread (asleep initially) */
	kthread = kthread_create(kthread_periodic_fun, NULL, "hello_kthread");

	if (IS_ERR(kthread))
		return PTR_ERR(kthread);

	/* Set thread as runnable */
	wake_up_process(kthread);

	pr_info("Periodic kthread has been started succesfully\n");

	return 0;
}

void kthread_module_cleanup(void)
{
	kthread_stop(kthread);

	pr_info("Module removed\n");

}

module_init(kthread_module_init);
module_exit(kthread_module_cleanup);

