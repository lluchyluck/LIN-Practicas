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



MODULE_LICENSE("GPL");
MODULE_AUTHOR("JUAN_Y_LUCAS");

#define PAGE_SIZE (1 << PAGE_SHIFT)

struct list_item {
    int data;
    struct list_head links;
};

static LIST_HEAD(mylist);  // Nodo fantasma (cabecera) de la lista enlazada

// Declaramos las funciones antes de su uso
static ssize_t modlist_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);

// Uso de struct proc_ops en lugar de struct file_operations para entrada /proc para versiones antiguas del kernel
static const struct proc_ops modlist_fops = { 
    .proc_read = modlist_read,
    .proc_write = modlist_write,
};

void add_number(int num) {
    struct list_item *new_item;
    new_item = kmalloc(sizeof(struct list_item), GFP_KERNEL);
    if (!new_item) {
        // kmalloc failed
        return;
    }
    new_item->data = num;
    INIT_LIST_HEAD(&new_item->links);
    list_add_tail(&new_item->links, &mylist);
}

void remove_number(int num) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links)
    {
        if (item->data == num) {
            list_del(&item->links);
            kfree(item);
        }
    }
}

void cleanup_list(void) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links)
    {
        list_del(&item->links);
        kfree(item);
    }
}

static ssize_t modlist_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[PAGE_SIZE]; //macro
    struct list_item *item;
    char *dst = kbuf;

    // Si ya hemos leído antes o desbordamiento de buffer return EOF
    if (*ppos > 0)
        return 0;

    // Iteramos por la lista y construimos el string con los datos
    list_for_each_entry(item, &mylist, links) 
    {
        dst += sprintf(dst, "%d\n", item->data);
        if (dst >= sizeof(kbuf) - 1) 
            return -ENOSPC; 
    }

    if(dst > count)
    {
        dst = count;
    }
    if (copy_to_user(ubuf, kbuf, dst))
        return -EFAULT;

    *ppos += dst; 
    return len;  
}


static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[PAGE_SIZE]; // macro
    int num;
    
    // Verifica que el tamaño del mensaje no sea mayor que el tamaño del buffer
    if (count > sizeof(kbuf) - 1)
        return -EFAULT;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    // Parsear el comando recibido
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