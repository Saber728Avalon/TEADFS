#ifndef MEM_H
#define MEM_H

#include <linux/slab.h>

//memory alloc
static void* teadfs_zalloc(size_t size, gfp_t flags) {
	return kzalloc(size, flags);
}

//free
static void teadfs_free(void* mem) {
	return kfree(mem);
}

#endif // !MEM_H
