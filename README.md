# `gpa_hpa` kernel module

This kernel module, combined with [Edward Guo](https://github.com/FarWrong/)'s custom KVM Hyper Call (HC) enables user space processes
in Virtual Machines (AKA: VM or Guest) to retrieve the physical address used on the host to back their emulated address.

> [!NOTE]
> Only tested on Intel-based machines.

In order to achieve this functionality, multiple (quite a few) modifications are required:

1. In your linux source tree, add the following custom HC (Derived from [Ed Guo](https://github.com/FarWrong/)'s implementation):
You need to add this new case for the HC under `kvm_emulate_hypercall` function's switch/case statement 
at `/arch/x86/kvm/x86.c`:
```c
int kvm_emulate_hypercall(struct kvm_vcpu *vcpu)
{
    // ... function's existing implementations ...
    switch(nr) {
        case 60:
            u64 gpa = a0;
            gfn_t gfn = (gfn_t)(gpa >> PAGE_SHIFT);
            printk("Given GFN: 0x%lx",(unsigned long)gfn); // printed in hex
            // Flags to be returned to rdx
            unsigned long flags = 0;
            unsigned long offset = gpa & 0xFFF;
            unsigned long hva = gfn_to_hva(vcpu->kvm, gfn);
            unsigned long ret;
            unsigned long order = 0;
            struct page *pages[1] = { NULL };

            if (!hva) {
                printk(KERN_ERR "Invalid HVA: 0x%lx\n", hva); // hex
                ret = -EFAULT;
                break;
            }

            // printk(KERN_ERR "HVA: 0x%lx\n", hva);

            unsigned long base_va = hva & PAGE_MASK;
            // printk(KERN_ERR "Base VA: 0x%lx\n", base_va);
            // unsigned long sec_offset = hva & 0xFFF;
            hva = offset;
            ret = get_user_pages_remote(vcpu->kvm->mm, base_va, 1, FOLL_GET, pages, NULL);
            if (ret <= 0) {
                printk(KERN_ERR "Failed to get user pages, ret: %ld\n", ret);
                ret = -EFAULT;
                break;
            }

            struct page *page = pages[0];
            if (!page) {
                printk(KERN_ERR "Failed to retrieve page structure\n");
                ret = -EFAULT;
                break;
            }

            if (page_mapped(page))
                flags |= (1 << 1);

            // Check if page is a huge page and its order
            if (PageHuge(page) || PageTransHuge(page) || PageCompound(page)) {
                flags |= (1 << 0);
                printk(KERN_INFO "Page is part of a Huge/Compound page\n");
                order = folio_order(page_folio(page));

                if (order > 31) {
                    printk(KERN_WARNING "Order value too high: %lu\n", order);
                    order = 31;
                }

                flags |= ((order & 0x1F) << 2);
            } else {
                printk(KERN_INFO "Page is not a huge page\n");
            }

            unsigned long pfn = page_to_pfn(page);
            // printk(KERN_ERR "PFN base: 0x%lx\n", pfn);
            phys_addr_t phys_base = PFN_PHYS(pfn);
            printk(KERN_ERR "Host Phys Base: 0x%llx\n", phys_base);
            // phys_addr_t exact_phys = phys_base | sec_offset;

            put_page(page);

            kvm_rdx_write(vcpu,pfn);
            kvm_rsi_write(vcpu,flags);
            ret = (unsigned long)phys_base;
            break;
        case // ... rest of the cases ...
        // ... rest of the file ...
    }
}
```

In this case, the HC is numbered `60`. This can be any HC number of your choice.

You need to save these changes, recompile the source tree, and boot into the newly built kernel.

2. Kernel Module in the VM to invoke the HC on the host.
You need to `make` and load the `gpa_hpa.c` kernel module in the VM.

This module is given the Guest Physical Address (GPA) of a user space program in the VM, and passes it to the HC.
After the HC finds the Host Physical Address used to back the given address, it passes results back.
Results include the translated address and flags indicating the properties of the frame i.e., compound/huge page, etc.

Interaction with this module for process in the VM is through the `/proc` interface.
User space programs write their retrieved GPA to the proc entry created by the module, 
the module passes it to the HC, retrieves the results (Host Physical Address or HPA)
of the address, and makes it available to user space program. User space program can then 
retrieve results by reading from the proc entry.

As already mentioned, user space programs need to pass the translated GPA of their addresses
to the kernel module, and not their Virtual Addresses (VA). So a function to translate your 
process's VA to GPA from `pagemap` is necessary before invoking the module.

An example of such user space program that end-to-end interacts and successfully retrieves results
is available under the `test.c` file. The `make` does not deal with `test.c` , so use typical
`gcc` commands to compile and run and test the program.
