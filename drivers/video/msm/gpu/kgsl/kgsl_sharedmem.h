<<<<<<< HEAD
/*
 * (C) Copyright Advanced Micro Devices, Inc. 2002, 2007
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
 * 
 * All source code in this file is licensed under the following license
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
=======
/* Copyright (c) 2002,2007-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
 */
#ifndef __KGSL_SHAREDMEM_H
#define __KGSL_SHAREDMEM_H

<<<<<<< HEAD
#include <linux/types.h>
#include <linux/msm_kgsl.h>

#define KGSL_PAGESIZE           0x1000
#define KGSL_PAGESIZE_SHIFT     12
#define KGSL_PAGEMASK           (~(KGSL_PAGESIZE - 1))

struct kgsl_pagetable;

struct platform_device;
struct gen_pool;

/* memory allocation flags */
#define KGSL_MEMFLAGS_ANY	0x00000000 /*dont care*/

#define KGSL_MEMFLAGS_APERTUREANY 0x00000000
#define KGSL_MEMFLAGS_EMEM	0x00000000
#define KGSL_MEMFLAGS_CONPHYS 	0x00001000

#define KGSL_MEMFLAGS_ALIGNANY	0x00000000
#define KGSL_MEMFLAGS_ALIGN32	0x00000000
#define KGSL_MEMFLAGS_ALIGN64	0x00060000
#define KGSL_MEMFLAGS_ALIGN128	0x00070000
#define KGSL_MEMFLAGS_ALIGN256	0x00080000
#define KGSL_MEMFLAGS_ALIGN512	0x00090000
#define KGSL_MEMFLAGS_ALIGN1K	0x000A0000
#define KGSL_MEMFLAGS_ALIGN2K	0x000B0000
#define KGSL_MEMFLAGS_ALIGN4K	0x000C0000
#define KGSL_MEMFLAGS_ALIGN8K	0x000D0000
#define KGSL_MEMFLAGS_ALIGN16K	0x000E0000
#define KGSL_MEMFLAGS_ALIGN32K	0x000F0000
#define KGSL_MEMFLAGS_ALIGN64K	0x00100000
#define KGSL_MEMFLAGS_ALIGNPAGE	KGSL_MEMFLAGS_ALIGN4K

/* fail the alloc if the flags cannot be honored */
#define KGSL_MEMFLAGS_STRICTREQUEST 0x80000000

#define KGSL_MEMFLAGS_APERTURE_MASK	0x0000F000
#define KGSL_MEMFLAGS_ALIGN_MASK 	0x00FF0000

#define KGSL_MEMFLAGS_APERTURE_SHIFT	12
#define KGSL_MEMFLAGS_ALIGN_SHIFT	16


/* shared memory allocation */
struct kgsl_memdesc {
	struct kgsl_pagetable *pagetable;
	void  *hostptr;
	unsigned int gpuaddr;
	unsigned int physaddr;
	unsigned int size;
	unsigned int priv;
};

struct kgsl_sharedmem {
	void *baseptr;
	unsigned int physbase;
	unsigned int size;
	struct gen_pool *pool;
};

int kgsl_sharedmem_alloc(uint32_t flags, int size,
			struct kgsl_memdesc *memdesc);

/*TODO: add protection flags */
int kgsl_sharedmem_import(struct kgsl_pagetable *,
				uint32_t phys_addr,
				uint32_t size,
				struct kgsl_memdesc *memdesc);

=======
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include "kgsl_mmu.h"

struct kgsl_device;
struct kgsl_process_private;

#define KGSL_CACHE_OP_INV       0x01
#define KGSL_CACHE_OP_FLUSH     0x02
#define KGSL_CACHE_OP_CLEAN     0x03

/** Set if the memdesc describes cached memory */
#define KGSL_MEMFLAGS_CACHED    0x00000001

struct kgsl_memdesc_ops {
	int (*vmflags)(struct kgsl_memdesc *);
	int (*vmfault)(struct kgsl_memdesc *, struct vm_area_struct *,
		       struct vm_fault *);
	void (*free)(struct kgsl_memdesc *memdesc);
};

extern struct kgsl_memdesc_ops kgsl_vmalloc_ops;

int kgsl_sharedmem_vmalloc(struct kgsl_memdesc *memdesc,
			   struct kgsl_pagetable *pagetable, size_t size);

int kgsl_sharedmem_vmalloc_user(struct kgsl_memdesc *memdesc,
				struct kgsl_pagetable *pagetable,
				size_t size, int flags);

int kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size);

int kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			     struct kgsl_pagetable *pagetable,
			     size_t size, int flags);

int kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc);


<<<<<<< HEAD
int kgsl_sharedmem_read(const struct kgsl_memdesc *memdesc, void *dst,
			unsigned int offsetbytes, unsigned int sizebytes);

int kgsl_sharedmem_write(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes, void *value,
			unsigned int sizebytes);
=======
int kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

int kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes, unsigned int value,
			unsigned int sizebytes);

<<<<<<< HEAD
int kgsl_sharedmem_init(struct kgsl_sharedmem *shmem);

int kgsl_sharedmem_close(struct kgsl_sharedmem *shmem);
=======
void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op);

void kgsl_process_init_sysfs(struct kgsl_process_private *private);
void kgsl_process_uninit_sysfs(struct kgsl_process_private *private);

int kgsl_sharedmem_init_sysfs(void);
void kgsl_sharedmem_uninit_sysfs(void);

static inline int
memdesc_sg_phys(struct kgsl_memdesc *memdesc,
		unsigned int physaddr, unsigned int size)
{
	struct page *page = phys_to_page(physaddr);

	memdesc->sg = vmalloc(sizeof(struct scatterlist) * 1);
	if (memdesc->sg == NULL)
		return -ENOMEM;

	memdesc->sglen = 1;
	sg_init_table(memdesc->sg, 1);
	sg_set_page(&memdesc->sg[0], page, size, 0);
	return 0;
}

static inline int
kgsl_allocate(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable, size_t size)
{
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem(memdesc, pagetable, size);
	return kgsl_sharedmem_vmalloc(memdesc, pagetable, size);
}

static inline int
kgsl_allocate_user(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable,
		size_t size, unsigned int flags)
{
	if (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE)
		return kgsl_sharedmem_ebimem_user(memdesc, pagetable, size,
						  flags);
	return kgsl_sharedmem_vmalloc_user(memdesc, pagetable, size, flags);
}

static inline int
kgsl_allocate_contiguous(struct kgsl_memdesc *memdesc, size_t size)
{
	int ret  = kgsl_sharedmem_alloc_coherent(memdesc, size);
	if (!ret && (kgsl_mmu_get_mmutype() == KGSL_MMU_TYPE_NONE))
		memdesc->gpuaddr = memdesc->physaddr;
	return ret;
}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#endif /* __KGSL_SHAREDMEM_H */
