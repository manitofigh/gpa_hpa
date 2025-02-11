#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kvm.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/memory.h>
static struct proc_dir_entry *proc_entry;
#include <linux/memblock.h>
#include <asm/pgtable.h>
#include <linux/slab.h>
#include <linux/khugepaged.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/mm_types.h>
#include <linux/hashtable.h>

typedef int i32;

static struct proc_dir_entry *proc_entry;

static struct file_operations gpa_fops =
{
    .owner = THIS_MODULE,
    .read = gpa_read,
    .write = gpa_write
};

static struct proc_dir_entry *proc_entry;

i32 __init mod_init(void)
{
    proc_entry = proc_create("gpa_hpa", 0666, NULL, &gpa_fops);

    if (!proc_entry)
        RETURN -ENOMEM;

    printk(KERN_INFO "gpa_hpa module loaded");

    return 0;
}

void __exit mod_exit(void)
{
    printk(KERN_INFO "unloading gpa_hpa module");
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
