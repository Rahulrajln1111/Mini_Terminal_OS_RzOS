// slab-like single-page slab allocator (simple)
#include "memory.h"
#include "page.h"
#include <stdint.h>


#define MAX_PAGES 16384   // e.g. support 64MB if PAGE_SIZE=4K
uintptr_t next_heap_va = KHEAP_START;
uintptr_t kheap_end_va = KHEAP_START + KHEAP_SIZE;


static uint8_t page_bitmap[MAX_PAGES];  // 1 = used, 0 = free
static uintptr_t memory_base = 0;
static size_t total_pages = 0;



void memory_init(uintptr_t kernel_end,uint32_t mem_size) {
    memory_base = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); // align after kernel
    total_pages = mem_size / PAGE_SIZE;
    for (size_t i = 0; i < total_pages; i++) {
        page_bitmap[i] = 0; // all free initially
    }
}

void *alloc_page(void) {
    for (size_t i = 0; i < total_pages; i++) {
        if (!page_bitmap[i]) {
            page_bitmap[i] = 1;
            return (void*)(memory_base + i * PAGE_SIZE);
        }
    }
    return NULL; // OOM
}

void free_page(void *p) {
    uintptr_t addr = (uintptr_t)p;
    size_t index = (addr - memory_base) / PAGE_SIZE;
    if (index < total_pages) {
        page_bitmap[index] = 0;
    }
}



size_t mem_total_pages(void) {
    return total_pages;
}

size_t mem_free_pages(void) {
    size_t free = 0;
    for (size_t i = 0; i < total_pages; i++) {
        if (!page_bitmap[i]) free++;
    }
    return free;
}




uintptr_t page_to_phys(void *p) {
    return (uintptr_t)p;
}

void *phys_to_virt(uintptr_t p) {
    return (void*)p;
}

void* memset(void* dest, int value, size_t count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count-- > 0) {
        *ptr++ = (unsigned char)value;
    }
    return dest;
}

// size classes (bytes)
static const size_t sizes[] = {8,16,32,64,128,256,512,1024};
#define SIZE_CLASSES (sizeof(sizes)/sizeof(sizes[0]))

typedef struct slab_page {
    struct slab_page *next;
    uint32_t free_count;
    void *free_list; // pointer to next free object (within page)
    size_t obj_size;
    // objects follow within the same page
} slab_page_t;

// per-class head
static slab_page_t *slab_heads[SIZE_CLASSES];



/* allocate a physical page and map it at the next heap virtual address.
   returns virtual address on success, NULL on failure. */
void *alloc_page_mapped(void)
{
    void *pa = alloc_page();                /* physical page address from bitmap allocator */
    if (!pa) return NULL;

    /* ensure we don't walk past heap end */
    if (next_heap_va >= kheap_end_va) {
        /* optionally: expand heap by growing page tables / updating kheap_end_va */
        free_page(pa);
        return NULL;
    }

    uintptr_t va = next_heap_va;
    /* map_page(va, pa) returns 0 on success in our page.c */
    if (map_page(va, (uintptr_t)pa) < 0) {
        free_page(pa);
        return NULL;
    }
    next_heap_va += PAGE_SIZE;
    return (void*)va;
}





void kheap_init(uintptr_t kernel_end)
{
    (void)kernel_end;
    for (size_t i=0;i<SIZE_CLASSES;i++) slab_heads[i] = NULL;
}

// helper: find class index
static int class_for_size(size_t s)
{
    for (int i=0;i<(int)SIZE_CLASSES;i++) if (s <= sizes[i]) return i;
    return -1;
}

static slab_page_t *alloc_slab_for_class(int cls)
{
    void *page =  alloc_page_mapped();
    if (!page) return NULL;
    slab_page_t *sp = (slab_page_t*)page;
    sp->next = NULL;
    sp->obj_size = sizes[cls];
    // compute how many objects fit
    size_t header = sizeof(slab_page_t);
    size_t usable = PAGE_SIZE - header;
    uint32_t nobj = usable / sp->obj_size;
    sp->free_count = nobj;
    // build free list within the page (addresses after header)
    uint8_t *p = (uint8_t*)page + header;
    sp->free_list = NULL;
    for (uint32_t i=0;i<nobj;i++) {
        *(void**)p = sp->free_list;
        sp->free_list = p;
        p += sp->obj_size;
    }
    return sp;
}


// in memory.c
void *kmalloc(size_t size)
{
    if (size == 0) return NULL;
    // large allocations: allocate whole pages
    if (size > PAGE_SIZE/2) {
        size_t npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        uintptr_t first_va = (uintptr_t)alloc_page_mapped();
        if (!first_va) return NULL;

        for (size_t i = 1; i < npages; ++i) {
            void *va = alloc_page_mapped();
            if (!va) {
                // failure: free already allocated virtual pages and their backing phys pages
                size_t freed_pages = i;
                for (size_t j = 0; j < freed_pages; ++j) {
                    uintptr_t pva = first_va + j * PAGE_SIZE;
                    uintptr_t pa = virt_to_phys(pva);
                    if (pa) {
                        free_page((void*)pa);
                    }
                    unmap_page(pva);
                }
                return NULL;
            }
        }
        return (void*)first_va;
    }

    int cls = class_for_size(size);
    if (cls < 0) return NULL;

    slab_page_t *head = slab_heads[cls];
    // find slab with free objects
    slab_page_t *s = head;
    while (s && s->free_count == 0) s = s->next;
    if (!s) {
        // allocate new slab page
        s = alloc_slab_for_class(cls);
        if (!s) return NULL;
        // push to head
        s->next = slab_heads[cls];
        slab_heads[cls] = s;
    }
    // pop object from s->free_list
    void *obj = s->free_list;
    s->free_list = *(void**)obj;
    s->free_count--;
    // zero the object for safety (optional)
    memset(obj, 0, s->obj_size);
    return obj;
}
void kfree(void *ptr)
{
    if (!ptr) return;
    // find page start
    uintptr_t pa = (uintptr_t)ptr & ~(PAGE_SIZE - 1);
    slab_page_t *sp = (slab_page_t*)pa;
    // sanity check obj_size (if sp->obj_size is zero, this may be a large allocation)
    if (sp->obj_size >= 8 && sp->obj_size <= 1024) {
        // push back into free list
        *(void**)ptr = sp->free_list;
        sp->free_list = ptr;
        sp->free_count++;
        // if all objects free, we could free the page (optional)
        // if (sp->free_count * sp->obj_size + sizeof(slab_page_t) == PAGE_SIZE) free_page(sp);
        return;
    } else {
        // treat as large allocation (whole pages). We don't store npages; assume single page
        free_page((void*)pa);
        return;
    }
}
