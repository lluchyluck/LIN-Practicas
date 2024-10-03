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

// Definición de la estructura de la lista
#ifdef PARTE_OPCIONAL
// Para la parte opcional (lista de cadenas de caracteres)
struct list_item {
    char *data;
    struct list_head links;
};
#else
// Para la parte básica (lista de enteros)
struct list_item {
    int data;
    struct list_head links;
};
#endif

static LIST_HEAD(mylist);  // Nodo fantasma (cabecera) de la lista enlazada

// Declaramos las funciones antes de su uso
static ssize_t modlist_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);

// Uso de struct proc_ops en lugar de struct file_operations para entrada /proc para versiones antiguas del kernel
static const struct proc_ops modlist_fops = { 
    .proc_read = modlist_read,
    .proc_write = modlist_write,
};

// Función para agregar un elemento a la lista
#ifdef PARTE_OPCIONAL
void add_string(const char *str) {
    struct list_item *new_item;
    new_item = kmalloc(sizeof(struct list_item), GFP_KERNEL);
    if (!new_item) {
        pr_err("Memory allocation failed\n");
        return;
    }
    new_item->data = kmalloc(strlen(str) + 1, GFP_KERNEL);  // Reservar memoria para la cadena
    if (!new_item->data) {
        pr_err("Memory allocation for string failed\n");
        kfree(new_item);
        return;
    }
    strcpy(new_item->data, str);  // Copiar la cadena
    INIT_LIST_HEAD(&new_item->links);
    list_add_tail(&new_item->links, &mylist);
}
#else
void add_number(int num) {
    struct list_item *new_item;
    new_item = kmalloc(sizeof(struct list_item), GFP_KERNEL);
    if (!new_item) {
        pr_err("Memory allocation failed\n");
        return;
    }
    new_item->data = num;
    INIT_LIST_HEAD(&new_item->links);
    list_add_tail(&new_item->links, &mylist);
}
#endif

// Función para eliminar un elemento de la lista
#ifdef PARTE_OPCIONAL
void remove_string(const char *str) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        if (strcmp(item->data, str) == 0) {
            list_del(&item->links);
            kfree(item->data);
            kfree(item);
        }
    }
}
#else
void remove_number(int num) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        if (item->data == num) {
            list_del(&item->links);
            kfree(item);
        }
    }
}
#endif

// Función para limpiar la lista
#ifdef PARTE_OPCIONAL
void cleanup_list(void) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        list_del(&item->links);
        kfree(item->data);
        kfree(item);
    }
}
#else
void cleanup_list(void) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        list_del(&item->links);
        kfree(item);
    }
}
#endif

// Función para leer desde el archivo /proc
static ssize_t modlist_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[256];  // Buffer local para almacenar los datos de la lista
    int len = 0;
    struct list_item *item;
    char *dst = kbuf;

    if (*ppos > 0 || count < sizeof(kbuf))
        return 0;

    list_for_each_entry(item, &mylist, links) {
#ifdef PARTE_OPCIONAL
        len += sprintf(dst + len, "%s\n", item->data);
#else
        len += sprintf(dst + len, "%d\n", item->data);
#endif
        if (len >= sizeof(kbuf) - 1)
            return -ENOSPC;
    }

    if (copy_to_user(ubuf, kbuf, len))
        return -EFAULT;

    *ppos = len;
    return len;
}

// Función para escribir en el archivo /proc
static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[128];  // Buffer local para almacenar el comando
    int num;
    char str[128];  // Para la parte opcional

    if (count > sizeof(kbuf) - 1)
        return -EFAULT;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';

#ifdef PARTE_OPCIONAL
    if (sscanf(kbuf, "add %s", str) == 1) {
        add_string(str);  // Agregar cadena
    } else if (sscanf(kbuf, "remove %s", str) == 1) {
        remove_string(str);  // Eliminar cadena
    } else if (strcmp(kbuf, "cleanup\n") == 0) {
        cleanup_list();
    } else {
        printk(KERN_WARNING "Unknown command: %s\n", kbuf);
    }
#else
    if (sscanf(kbuf, "add %i", &num) == 1) {
        add_number(num);  // Agregar número
    } else if (sscanf(kbuf, "remove %i", &num) == 1) {
        remove_number(num);  // Eliminar número
    } else if (strcmp(kbuf, "cleanup\n") == 0) {
        cleanup_list();
    } else {
        printk(KERN_WARNING "Unknown command: %s\n", kbuf);
    }
#endif

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
