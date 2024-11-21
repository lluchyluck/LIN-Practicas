#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Juan y Lucas");
MODULE_DESCRIPTION("Módulo ProdCons con buffer circular y semáforos");

#define DEVICE_NAME "prodcons"
#define BUFFER_SIZE 4 // Tamaño máximo del buffer en enteros (4 enteros x 4 bytes)

static struct kfifo fifo_buffer;
static struct semaphore mutex;     // Exclusión mutua
static struct semaphore espacios;  // Espacios disponibles en el buffer
static struct semaphore elementos; // Elementos disponibles en el buffer

static int prodcons_major;
static struct class *prodcons_class = NULL;
static struct device *prodcons_device = NULL;
static atomic_t open_count = ATOMIC_INIT(0);

// Inserta un número en el buffer
static void insertar_entero(int num) {
    kfifo_in(&fifo_buffer, &num, sizeof(int));
}

// Extrae un número del buffer
static int extraer_entero(void) {
    int num;
    kfifo_out(&fifo_buffer, &num, sizeof(int));
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

    // Bloquear si no hay espacio en el buffer
    if (down_interruptible(&espacios))
        return -ERESTARTSYS;

    // Acceso exclusivo al buffer
    if (down_interruptible(&mutex))
        return -ERESTARTSYS;

    insertar_entero(num);

    up(&mutex);      // Liberar el mutex
    up(&elementos);  // Incrementar el número de elementos disponibles

    return count;
}

// Operación de lectura (extracción del buffer)
static ssize_t prodcons_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) {
    int num;
    char kbuf[16];
    int len;

    if(kfifo_is_empty(&fifo_buffer)!= 0)
    {
        return -ERESTARTSYS;
    }

    // Bloquear si no hay elementos en el buffer
    if (down_interruptible(&elementos))
        return -ERESTARTSYS;

    // Acceso exclusivo al buffer
    if (down_interruptible(&mutex))
        return -ERESTARTSYS;

    num = extraer_entero();

    up(&mutex);     // Liberar el mutex
    up(&espacios);  // Incrementar los espacios disponibles

    len = snprintf(kbuf, sizeof(kbuf), "%d\n", num);
    if (copy_to_user(ubuf, kbuf, len))
        return -EFAULT;

    return len;
}

// Operación open para controlar procesos
static int prodcons_open(struct inode *inode, struct file *file) {
    atomic_inc(&open_count);
    return 0;
}

// Operación release para liberar recursos
static int prodcons_release(struct inode *inode, struct file *file) {
    atomic_dec(&open_count);
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

// Inicialización del módulo
static int __init prodcons_init(void) {
    int ret;

    // Inicializar el buffer circular
    if (kfifo_alloc(&fifo_buffer, BUFFER_SIZE * sizeof(int), GFP_KERNEL)) {
        printk(KERN_ERR "Error al inicializar el buffer circular\n");
        return -ENOMEM;
    }

    // Inicializar los semáforos
    sema_init(&mutex, 1);            // Mutex inicializado en 1
    sema_init(&espacios, BUFFER_SIZE); // Espacios inicializados al tamaño máximo
    sema_init(&elementos, 0);        // No hay elementos inicialmente

    // Registrar el dispositivo de caracteres
    prodcons_major = register_chrdev(0, DEVICE_NAME, &prodcons_fops);
    if (prodcons_major < 0) {
        printk(KERN_ERR "Error al registrar el dispositivo\n");
        kfifo_free(&fifo_buffer);
        return prodcons_major;
    }

    // Crear clase y dispositivo
    prodcons_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(prodcons_class)) {
        unregister_chrdev(prodcons_major, DEVICE_NAME);
        kfifo_free(&fifo_buffer);
        return PTR_ERR(prodcons_class);
    }

    prodcons_device = device_create(prodcons_class, NULL, MKDEV(prodcons_major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(prodcons_device)) {
        class_destroy(prodcons_class);
        unregister_chrdev(prodcons_major, DEVICE_NAME);
        kfifo_free(&fifo_buffer);
        return PTR_ERR(prodcons_device);
    }

    printk(KERN_INFO "ProdCons: módulo cargado con éxito\n");
    return 0;
}

// Finalización del módulo
static void __exit prodcons_exit(void) {
    if (atomic_read(&open_count) > 0) {
        printk(KERN_ERR "No se puede descargar el módulo: procesos abiertos\n");
        return;
    }

    device_destroy(prodcons_class, MKDEV(prodcons_major, 0));
    class_destroy(prodcons_class);
    unregister_chrdev(prodcons_major, DEVICE_NAME);
    kfifo_free(&fifo_buffer);

    printk(KERN_INFO "ProdCons: módulo descargado con éxito\n");
}

module_init(prodcons_init);
module_exit(prodcons_exit);
