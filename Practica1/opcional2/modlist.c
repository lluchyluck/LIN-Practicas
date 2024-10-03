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
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JUAN_Y_LUCAS");

struct list_item {
    int data;
    struct list_head links;
};

static LIST_HEAD(mylist);  // Nodo fantasma (cabecera) de la lista enlazada
static int list_size = 0;  // Contador para el número de elementos de la lista

// Funciones para el manejo de la lista
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
    list_size++;  // Incrementamos el contador de la lista
}

void remove_number(int num) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        if (item->data == num) {
            list_del(&item->links);
            kfree(item);
            list_size--;  // Decrementamos el contador
        }
    }
}

void cleanup_list(void) {
    struct list_item *item, *tmp;
    list_for_each_entry_safe(item, tmp, &mylist, links) {
        list_del(&item->links);
        kfree(item);
        list_size = 0;  // Reiniciamos el contador
    }
}

// Funciones para la interfaz seq_file
static void *modlist_seq_start(struct seq_file *s, loff_t *pos) {
    if (*pos == 0) {
        return &mylist;
    }

    struct list_head *current;
    current = mylist.next;
    loff_t i = 0;
    while (current != &mylist && i < *pos) {
        current = current->next;
        i++;
    }

    return (current != &mylist) ? current : NULL;
}

static void *modlist_seq_next(struct seq_file *s, void *v, loff_t *pos) {
    struct list_head *current = (struct list_head *)v;
    current = current->next;
    (*pos)++;
    
    return (current != &mylist) ? current : NULL;
}

static void modlist_seq_stop(struct seq_file *s, void *v) {
    // No se requiere acción específica aquí
}

static int modlist_seq_show(struct seq_file *s, void *v) {
    struct list_head *current = (struct list_head *)v;
    struct list_item *item = list_entry(current, struct list_item, links);

    seq_printf(s, "%d\n", item->data);
    return 0;
}

// Operaciones seq_file
static struct seq_operations modlist_seq_ops = {
    .start = modlist_seq_start,
    .next  = modlist_seq_next,
    .stop  = modlist_seq_stop,
    .show  = modlist_seq_show,
};

// Función open para seq_file
static int modlist_open(struct inode *inode, struct file *file) {
    return seq_open(file, &modlist_seq_ops);
}

// Operaciones de archivo
static const struct file_operations modlist_fops = {
    .owner   = THIS_MODULE,
    .open    = modlist_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

// Función de escritura en /proc
static ssize_t modlist_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[128];  // Buffer local para almacenar el comando
    int num;

    if (count > sizeof(kbuf) - 1)  // Verifica que el mensaje no sea mayor que el tamaño del buffer
        return -EFAULT;

    if (copy_from_user(kbuf, ubuf, count))  // Copia desde el espacio de usuario a nuestro buffer local
        return -EFAULT;

    kbuf[count] = '\0';  // Asegúrate de que el buffer tenga un final de cadena

    // Parsear el comando recibido
    if (sscanf(kbuf, "add %i", &num) == 1) {
        add_number(num);  // Si es un comando 'add', llamamos a add_number
    } else if (sscanf(kbuf, "remove %i", &num) == 1) {
        remove_number(num);  // Si es 'remove', llamamos a remove_number
    } else if (strcmp(kbuf, "cleanup\n") == 0) {
        cleanup_list();  // Si es 'cleanup', limpiamos la lista
    } else {
        printk(KERN_WARNING "Unknown command: %s\n", kbuf);
    }

    return count;  // Retornamos el tamaño de lo que se escribió
}

// Funciones de inicialización y salida del módulo
static int __init modlist_init(void) {
    proc_create("modlist", 0666, NULL, &modlist_fops);
    printk(KERN_INFO "modlist module loaded with seq_file support.\n");
    return 0;
}

static void __exit modlist_exit(void) {
    remove_proc_entry("modlist", NULL);
    cleanup_list();
    printk(KERN_INFO "modlist module unloaded.\n");
}

module_init(modlist_init);
module_exit(modlist_exit);
