#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <asm/kvm_para.h>

#define PROC_NAME "gpa_hpa"
#define BUF_SIZE 256

struct proc_data {
    char buffer[BUF_SIZE];
    bool updated; // flag
};

static struct proc_dir_entry *proc_entry;

static inline void kvm_hypercall_two_returns(unsigned long *val1, unsigned long *val2, 
                                           unsigned long input)
{
    register unsigned long rdx asm("rdx");
    register unsigned long rsi asm("rsi");
    kvm_hypercall1(60, input);
    *val2 = rdx;
    *val1 = rsi;
}

static int gpa_open(struct inode *inode, struct file *file)
{
    struct proc_data *data = kmalloc(sizeof(struct proc_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    data->updated = false; // init flag
    file->private_data = data;
    return 0;
}

static int gpa_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}

static ssize_t gpa_write(struct file *file, const char __user *ubuf, 
                        size_t count, loff_t *ppos)
{
    struct proc_data *data = file->private_data;
    char buf[64];
    unsigned long long gpa;
    unsigned long flags, pfn;

    if (count >= sizeof(buf))
        return -EINVAL;

    if (copy_from_user(buf, ubuf, count))
        return -EFAULT;

    buf[count] = '\0';

    if (kstrtoull(buf, 16, &gpa))
        return -EFAULT;

    kvm_hypercall_two_returns(&flags, &pfn, gpa);
    
    snprintf(data->buffer, BUF_SIZE, "HPA=0x%lx FLAGS=0x%lx\n", pfn, flags);
    data->updated = true; // Mark buffer as updated
    return count;
}

static ssize_t gpa_read(struct file *file, char __user *ubuf, 
                       size_t count, loff_t *ppos)
{
    struct proc_data *data = file->private_data;
    int len = strlen(data->buffer);

    // reset position ptr if buffer was updated
    if (data->updated) {
        *ppos = 0;
        data->updated = false;
    }

    if (*ppos >= len)
        return 0;

    if (copy_to_user(ubuf, data->buffer + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static const struct proc_ops gpa_fops = {
    .proc_open = gpa_open,
    .proc_release = gpa_release,
    .proc_read = gpa_read,
    .proc_write = gpa_write,
};

static int __init mod_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &gpa_fops);
    if (!proc_entry)
        return -ENOMEM;
    
    printk(KERN_INFO "gpa_hpa module loaded\n");
    return 0;
}

static void __exit mod_exit(void)
{
    proc_remove(proc_entry);
    printk(KERN_INFO "gpa_hpa module unloaded\n");
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
