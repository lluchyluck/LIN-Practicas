
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
#include <linux/kfifo.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Juan y Lucas");
MODULE_DESCRIPTION("Módulo ProdCons con buffer circular y semáforos");

#define DEVICE_NAME "prodcons"
#define BUFFER_SIZE 4 // Tamaño máximo del buffer en enteros (4 enteros x 4 bytes)

static struct kfifo fifo_buffer;

struct semaphore elementos,huecos, mtx;

static DEFINE_SPINLOCK(spin_count);   // Protección para contador de referencias
static int contador_referencias = 0;          // Contador de referencias


// Inserta un número en el buffer
static void insertar_entero(int num) {

    kfifo_in(&fifo_buffer, &num, sizeof(int));
}

// Extrae un número del buffer
static int extraer_entero(void) {
    int num, ret;
    ret = kfifo_out(&fifo_buffer, &num, sizeof(int));
    return num;
}

// Operación de escritura (inserción en el buffer)
static ssize_t prodcons_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) {
    char kbuf[16];
    int num;

    if (count > sizeof(kbuf) - 1)
        return -EINVAL;

    if (copy_from_user(kbuf, ubuf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    if (kstrtoint(kbuf, 10, &num) != 0)
        return -EINVAL;

    if (down_interruptible(&huecos))
        return -EINTR;

    /* Entrar a la SC */
    if (down_interruptible(&mtx)) {
    up(&huecos);
    return -EINTR;
    }

    insertar_entero(num);

    /* Salir de la SC */
    up(&mtx);
    up(&elementos);

    return count;
}

// Operación de lectura (extracción del buffer)
static ssize_t prodcons_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    int num;
    char kbuf[16];
    int len;

    if ((*ppos) > 0) /* Tell the application that there is nothing left to read */
        return 0;

    /*if(kfifo_is_empty(&fifo_buffer))
    {
        return 0;
    }*/

    if (down_interruptible(&elementos))
        return -EINTR;

    /* Entrar a la SC */
    if (down_interruptible(&mtx)){
        up(&elementos);
        return -EINTR;
    }

    num = extraer_entero();

    /* Salir de la SC */
    up(&mtx);
    up(&huecos);

    len = snprintf(kbuf, sizeof(kbuf), "%d\n", num);
    if (copy_to_user(ubuf, kbuf, len))
        return -EFAULT;

    (*ppos) += len; /* Update the file pointer */

    return len;
}

// Operación open para controlar procesos
static int prodcons_open(struct inode *inode, struct file *file) {
    if (!try_module_get(THIS_MODULE))  // Incrementa contador de referencia
        return -EBUSY;

    spin_lock(&spin_count);
    contador_referencias++;
    spin_unlock(&spin_count);
    return 0;
}

// Operación release para liberar recursos
static int prodcons_release(struct inode *inode, struct file *file) {
    spin_lock(&spin_count);
    contador_referencias--;
    spin_unlock(&spin_count);

    module_put(THIS_MODULE);  // Decrementa contador de referencia
    return 0;
}

// Operaciones soportadas por el dispositivo
static const struct file_operations prodcons_fops = {
    .owner = THIS_MODULE,
    .write = prodcons_write,
    .read = prodcons_read,
    .open = prodcons_open,
    .release = prodcons_release,
};

/* Dispositivo misc */
static struct miscdevice prodcons_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "prodcons",
    .fops = &prodcons_fops,
    .mode = 0666,
};

// Inicialización del módulo
static int __init prodcons_init(void) {

    int err;

    sema_init(&huecos,BUFFER_SIZE);
    sema_init(&elementos, 0);
    sema_init(&mtx,1);

    // Inicializar el buffer circular
    if (kfifo_alloc(&fifo_buffer, BUFFER_SIZE * sizeof(int), GFP_KERNEL)) {
        printk(KERN_ERR "Error al inicializar el buffer circular\n");
        return -ENOMEM;
    }

    err = misc_register(&prodcons_misc);
    if (err)
    {
        kfifo_free(&fifo_buffer);
        return err;
    }

    printk(KERN_INFO "ProdCons: módulo cargado con éxito\n");
    return 0;
}

// Finalización del módulo
static void __exit prodcons_exit(void) {

    spin_lock(&spin_count);
    if (contador_referencias > 0) {
        spin_unlock(&spin_count);
        pr_err("Device is in use. Cannot unload module.\n");
        return;
    }
    spin_unlock(&spin_count);


    misc_deregister(&prodcons_misc);
    kfifo_free(&fifo_buffer);

    printk(KERN_INFO "ProdCons: módulo descargado con éxito\n");
}

module_init(prodcons_init);
module_exit(prodcons_exit);
