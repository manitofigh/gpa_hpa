#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#define PAGE_SIZE (1<<12)
#define HUGE_PAGE_SIZE (1 << 21)
#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8

typedef unsigned int u32;
typedef int i32;

unsigned long get_pfn(void *addr) {
    unsigned long pfn = 0;
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open pagemap");
        exit(1);
    }

    off_t offset = ((unsigned long)addr >> PAGE_SHIFT) * PAGEMAP_LENGTH;
    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek");
        close(fd);
        exit(1);
    }

    if (read(fd, &pfn, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) {
        perror("read");
        close(fd);
        exit(1);
    }

    close(fd);
    return pfn & ((1ULL << 55) - 1);
}

void translate_address(int proc_fd, void *addr) {
    char buf[256];
    unsigned long offset = (unsigned long) addr & 0xFFF;
    unsigned long pfn = get_pfn(addr);
    unsigned long gpa = (pfn << PAGE_SHIFT) | offset;

    // proc write
    snprintf(buf, sizeof(buf), "%lx", gpa);
    if (write(proc_fd, buf, strlen(buf)) < 0) {
        perror("failed write to proc");
        exit(1);
    }

    // read res right after
    memset(buf, 0, sizeof(buf));
    ssize_t ret = read(proc_fd, buf, sizeof(buf) - 1);
    if (ret < 0) {
        perror("failed read from proc");
        exit(1);
    }

    // extract HPA from response (idc about flags rn)
    unsigned long hpa;
    if (sscanf(buf, "HPA=0x%lx", &hpa) != 1) {
        fprintf(stderr, "Failed to parse HPA from response\n");
        exit(1);
    }

    // Print only the HPA
    printf("For VA %p (GPA 0x%lx): Full HPA=0x%lx\n", addr, gpa, 
           hpa << PAGE_SHIFT | offset); 
}

int main() {
    int proc_fd = open("/proc/gpa_hpa", O_RDWR);
    if (proc_fd < 0) {
        perror("open proc");
        exit(1);
    }

    // void *reg_page = NULL;

    for (u32 i=0; i<10; i++) {

        //regular page 4KiB
        void *reg_page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, 
                             MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

        if (reg_page == MAP_FAILED) {
            perror("failed mmap regular");
            exit(1);
        }

        memset(reg_page, 0xaa, PAGE_SIZE); // prevent lazy alloc
        //translate_address(proc_fd, reg_page);

        if (reg_page != MAP_FAILED) {
            translate_address(proc_fd, reg_page+12);
            // puts("");
        }

        // HP
        /*
        void *huge_page = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ|PROT_WRITE,
                              MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
        if (huge_page != MAP_FAILED) {
            memset(huge_page, 0xbb, HUGE_PAGE_SIZE); //0xbb could be anything else. it's just to prevent lazy alloc
            translate_address(proc_fd, huge_page);
            getchar();
            getchar();
            //munmap(huge_page, HUGE_PAGE_SIZE);
        }
         
        if (huge_page != MAP_FAILED) {
            for (unsigned int i=0; i< (1<<21); i+= (1<<20)) {
                translate_address(proc_fd, huge_page);
                puts("");
            }
            getchar();
        }
        */

        // munmap(reg_page, PAGE_SIZE);
    }
    getchar();
    getchar();

    close(proc_fd);
    return 0;
}
