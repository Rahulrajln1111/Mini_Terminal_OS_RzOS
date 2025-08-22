// slab-like single-page slab allocator (simple)
#include "memory.h"
#include "page.h"
#include <stdint.h>

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
    void *page = alloc_page();
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

void *kmalloc(size_t size)
{
    if (size == 0) return NULL;
    // large allocations: allocate whole pages
    if (size > PAGE_SIZE/2) {
        // round up pages
        size_t bytes = size;
        size_t npages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        // allocate npages pages (simple loop)
        void *first = alloc_page();
        if (!first) return NULL;
        uintptr_t addr = (uintptr_t)first;
        // allocate additional pages and ignore contiguity for now: return contiguous only if physically contiguous
        for (size_t i=1;i<npages;i++) {
            void *p = alloc_page();
            if (!p) { // if fail, free already allocated pages
                // free pages allocated so far
                for (size_t j=0;j<i;j++) free_page((void*)(addr + j*PAGE_SIZE));
                return NULL;
            }
        }
        return (void*)addr;
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
