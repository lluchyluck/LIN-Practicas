/*
 *  
 *  modlist.c
 * 
 *  Juan Girón Herranz
 *  Lucas Calzada del Pozo
 * 
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JUAN_Y_LUCAS");

#define BUFFER_SIZE 256

struct list_item {
    int data;
    struct list_head links;
};

static LIST_HEAD(mylist);  // Nodo fantasma (cabecera) de la lista enlazada
static spinlock_t mylist_lock;  // Spin lock para proteger el acceso a la lista

// Declaración de funciones
static ssize_t modlist_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);

// Uso de struct proc_ops
static const struct proc_ops modlist_fops = {
    .proc_read = modlist_read,
    .proc_write = modlist_write,
};

void add_number(int num) {
    struct list_item *new_item;

    new_item = kmalloc(sizeof(struct list_item), GFP_KERNEL);
    if (!new_item)
        return;

    new_item->data = num;
    INIT_LIST_HEAD(&new_item->links);

    spin_lock(&mylist_lock);  // Bloqueo
    list_add_tail(&new_item->links, &mylist);
    spin_unlock(&mylist_lock);  // Desbloqueo
}

void remove_number(int num) {
    struct list_item *item, *tmp;

    spin_lock(&mylist_lock);  // Bloqueo
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        if (item->data == num) {
            list_del(&item->links);
            kfree(item);
        }
    }
    spin_unlock(&mylist_lock);  // Desbloqueo
}

void cleanup_list(void) {
    struct list_item *item, *tmp;

    spin_lock(&mylist_lock);  // Bloqueo
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        list_del(&item->links);
        kfree(item);
    }
    spin_unlock(&mylist_lock);  // Desbloqueo
}

static ssize_t modlist_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[BUFFER_SIZE];
    int len = 0;
    struct list_item *item;
    char *dst = kbuf;

    if (*ppos > 0)
        return 0;

    if (!try_module_get(THIS_MODULE))  // Incrementa contador de referencia
        return -EBUSY;

    spin_lock(&mylist_lock);  // Bloqueo
    list_for_each_entry(item, &mylist, links) {
        len += snprintf(dst + len, sizeof(kbuf) - len, "%d\n", item->data);
        if (len >= sizeof(kbuf) - 1) {
            spin_unlock(&mylist_lock);  // Desbloqueo
            module_put(THIS_MODULE);  // Decrementa contador de referencia
            return -ENOSPC;
        }
    }
    spin_unlock(&mylist_lock);  // Desbloqueo

    if (copy_to_user(ubuf, kbuf, len)) {
        module_put(THIS_MODULE);
        return -EFAULT;
    }

    *ppos += len;
    module_put(THIS_MODULE);  // Decrementa contador de referencia
    return len;
}

static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[BUFFER_SIZE];
    int num;

    if (count > sizeof(kbuf) - 1)
        return -EFAULT;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    if (sscanf(kbuf, "add %i", &num) == 1) {
        add_number(num);
    } else if (sscanf(kbuf, "remove %i", &num) == 1) {
        remove_number(num);
    } else if (strcmp(kbuf, "cleanup\n") == 0) {
        cleanup_list();
    } else {
        printk(KERN_WARNING "Unknown command: %s\n", kbuf);
        return -EINVAL;
    }

    return count;
}

static int __init modlist_init(void) {
    proc_create("modlist", 0666, NULL, &modlist_fops);
    spin_lock_init(&mylist_lock);  // Inicializa el spin lock
    printk(KERN_INFO "modlist module loaded.\n");
    return 0;
}

static void __exit modlist_exit(void) {
    remove_proc_entry("modlist", NULL);
    cleanup_list();
    printk(KERN_INFO "modlist module unloaded.\n");
}

module_init(modlist_init);
module_exit(modlist_exit);
