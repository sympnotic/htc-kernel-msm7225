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
 */
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
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
 */
#include <linux/vmalloc.h>
#include <linux/memory_alloc.h>
#include <linux/android_pmem.h>
#include <asm/cacheflush.h>
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#include "kgsl.h"
<<<<<<< HEAD
#include "kgsl_log.h"

/*  block alignment shift count */
static inline unsigned int
kgsl_memarena_get_order(uint32_t flags)
{
	unsigned int alignshift;
	alignshift = ((flags & KGSL_MEMFLAGS_ALIGN_MASK)
		      >> KGSL_MEMFLAGS_ALIGN_SHIFT);
	return alignshift;
}

/*  block alignment shift count */
static inline unsigned int
kgsl_memarena_align(unsigned int address, unsigned int shift)
{
	unsigned int alignedbaseaddr = ((address) >> shift) << shift;
	if (alignedbaseaddr < address)
		alignedbaseaddr += (1 << shift);

	return alignedbaseaddr;
}

int
kgsl_sharedmem_init(struct kgsl_sharedmem *shmem)
=======
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "kgsl_device.h"

/* An attribute for showing per-process memory statistics */
struct kgsl_mem_entry_attribute {
	struct attribute attr;
	int memtype;
	ssize_t (*show)(struct kgsl_process_private *priv,
		int type, char *buf);
};

#define to_mem_entry_attr(a) \
container_of(a, struct kgsl_mem_entry_attribute, attr)

#define __MEM_ENTRY_ATTR(_type, _name, _show) \
{ \
	.attr = { .name = __stringify(_name), .mode = 0444 }, \
	.memtype = _type, \
	.show = _show, \
}

/*
 * A structure to hold the attributes for a particular memory type.
 * For each memory type in each process we store the current and maximum
 * memory usage and display the counts in sysfs.  This structure and
 * the following macro allow us to simplify the definition for those
 * adding new memory types
 */

struct mem_entry_stats {
	int memtype;
	struct kgsl_mem_entry_attribute attr;
	struct kgsl_mem_entry_attribute max_attr;
};


#define MEM_ENTRY_STAT(_type, _name) \
{ \
	.memtype = _type, \
	.attr = __MEM_ENTRY_ATTR(_type, _name, mem_entry_show), \
	.max_attr = __MEM_ENTRY_ATTR(_type, _name##_max, \
		mem_entry_max_show), \
}


/**
 * Given a kobj, find the process structure attached to it
 */

static struct kgsl_process_private *
_get_priv_from_kobj(struct kobject *kobj)
{
	struct kgsl_process_private *private;
	unsigned long name;

	if (!kobj)
		return NULL;

	if (sscanf(kobj->name, "%ld", &name) != 1)
		return NULL;

	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == name)
			return private;
	}

	return NULL;
}

/**
 * Show the current amount of memory allocated for the given memtype
 */

static ssize_t
mem_entry_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", priv->stats[type].cur);
}

/**
 * Show the maximum memory allocated for the given memtype through the life of
 * the process
 */

static ssize_t
mem_entry_max_show(struct kgsl_process_private *priv, int type, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", priv->stats[type].max);
}


static void mem_entry_sysfs_release(struct kobject *kobj)
{
}

static ssize_t mem_entry_sysfs_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	struct kgsl_mem_entry_attribute *pattr = to_mem_entry_attr(attr);
	struct kgsl_process_private *priv;
	ssize_t ret;

	mutex_lock(&kgsl_driver.process_mutex);
	priv = _get_priv_from_kobj(kobj);

	if (priv && pattr->show)
		ret = pattr->show(priv, pattr->memtype, buf);
	else
		ret = -EIO;

	mutex_unlock(&kgsl_driver.process_mutex);
	return ret;
}

static const struct sysfs_ops mem_entry_sysfs_ops = {
	.show = mem_entry_sysfs_show,
};

static struct kobj_type ktype_mem_entry = {
	.sysfs_ops = &mem_entry_sysfs_ops,
	.default_attrs = NULL,
	.release = mem_entry_sysfs_release
};

static struct mem_entry_stats mem_stats[] = {
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_KERNEL, kernel),
#ifdef CONFIG_ANDROID_PMEM
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_PMEM, pmem),
#endif
#ifdef CONFIG_ASHMEM
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_ASHMEM, ashmem),
#endif
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_USER, user),
#ifdef CONFIG_ION
	MEM_ENTRY_STAT(KGSL_MEM_ENTRY_USER, ion),
#endif
};

void
kgsl_process_uninit_sysfs(struct kgsl_process_private *private)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		sysfs_remove_file(&private->kobj, &mem_stats[i].attr.attr);
		sysfs_remove_file(&private->kobj,
			&mem_stats[i].max_attr.attr);
	}

	kobject_put(&private->kobj);
}

void
kgsl_process_init_sysfs(struct kgsl_process_private *private)
{
	unsigned char name[16];
	int i, ret;

	snprintf(name, sizeof(name), "%d", private->pid);

	if (kobject_init_and_add(&private->kobj, &ktype_mem_entry,
		kgsl_driver.prockobj, name))
		return;

	for (i = 0; i < ARRAY_SIZE(mem_stats); i++) {
		/* We need to check the value of sysfs_create_file, but we
		 * don't really care if it passed or not */

		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].attr.attr);
		ret = sysfs_create_file(&private->kobj,
			&mem_stats[i].max_attr.attr);
	}
}

static int kgsl_drv_memstat_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	unsigned int val = 0;

	if (!strncmp(attr->attr.name, "vmalloc", 7))
		val = kgsl_driver.stats.vmalloc;
	else if (!strncmp(attr->attr.name, "vmalloc_max", 11))
		val = kgsl_driver.stats.vmalloc_max;
	else if (!strncmp(attr->attr.name, "coherent", 8))
		val = kgsl_driver.stats.coherent;
	else if (!strncmp(attr->attr.name, "coherent_max", 12))
		val = kgsl_driver.stats.coherent_max;
	else if (!strncmp(attr->attr.name, "mapped", 6))
		val = kgsl_driver.stats.mapped;
	else if (!strncmp(attr->attr.name, "mapped_max", 10))
		val = kgsl_driver.stats.mapped_max;

	return snprintf(buf, PAGE_SIZE, "%u\n", val);
}

static int kgsl_drv_histogram_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int len = 0;
	int i;

	for (i = 0; i < 16; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d ",
			kgsl_driver.stats.histogram[i]);

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

DEVICE_ATTR(vmalloc, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(vmalloc_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(coherent, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(coherent_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(mapped, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(mapped_max, 0444, kgsl_drv_memstat_show, NULL);
DEVICE_ATTR(histogram, 0444, kgsl_drv_histogram_show, NULL);

static const struct device_attribute *drv_attr_list[] = {
	&dev_attr_vmalloc,
	&dev_attr_vmalloc_max,
	&dev_attr_coherent,
	&dev_attr_coherent_max,
	&dev_attr_mapped,
	&dev_attr_mapped_max,
	&dev_attr_histogram,
	NULL
};

void
kgsl_sharedmem_uninit_sysfs(void)
{
	kgsl_remove_device_sysfs_files(&kgsl_driver.virtdev, drv_attr_list);
}

int
kgsl_sharedmem_init_sysfs(void)
{
	return kgsl_create_device_sysfs_files(&kgsl_driver.virtdev,
		drv_attr_list);
}

#ifdef CONFIG_OUTER_CACHE
static void _outer_cache_range_op(int op, unsigned long addr, size_t size)
{
	switch (op) {
	case KGSL_CACHE_OP_FLUSH:
		outer_flush_range(addr, addr + size);
		break;
	case KGSL_CACHE_OP_CLEAN:
		outer_clean_range(addr, addr + size);
		break;
	case KGSL_CACHE_OP_INV:
		outer_inv_range(addr, addr + size);
		break;
	}
}

static void outer_cache_range_op_sg(struct scatterlist *sg, int sglen, int op)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, sglen, i) {
		unsigned int paddr = sg_phys(s);
		_outer_cache_range_op(op, paddr, s->length);
	}
}

#else
static void outer_cache_range_op_sg(struct scatterlist *sg, int sglen, int op)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	int result = -EINVAL;

	if (!request_mem_region(shmem->physbase, shmem->size, DRIVER_NAME)) {
		KGSL_MEM_ERR("request_mem_region failed\n");
		goto error;
	}

	shmem->baseptr = ioremap(shmem->physbase, shmem->size);
	KGSL_MEM_INFO("ioremap(shm) = %p\n", shmem->baseptr);

	if (shmem->baseptr == NULL) {
		KGSL_MEM_ERR("ioremap failed for address %08x size %d\n",
				shmem->physbase, shmem->size);
		result = -ENODEV;
		goto error_release_mem;
	}

	shmem->pool = gen_pool_create(KGSL_PAGESIZE_SHIFT, -1);
	if (shmem->pool == NULL) {
		KGSL_MEM_ERR("gen_pool_create failed\n");
		result = -ENOMEM;
		goto error_iounmap;
	}

	if (gen_pool_add(shmem->pool, shmem->physbase, shmem->size, -1)) {
		KGSL_MEM_ERR("gen_pool_create failed\n");
		result = -ENOMEM;
		goto error_pool_destroy;
	}
	result = 0;
	KGSL_MEM_INFO("physbase 0x%08x size 0x%08x baseptr 0x%p\n",
			shmem->physbase, shmem->size, shmem->baseptr);
	return 0;

error_pool_destroy:
	gen_pool_destroy(shmem->pool);
error_iounmap:
	iounmap(shmem->baseptr);
	shmem->baseptr = NULL;
error_release_mem:
	release_mem_region(shmem->physbase, shmem->size);
error:
	return result;
}

<<<<<<< HEAD
int
kgsl_sharedmem_close(struct kgsl_sharedmem *shmem)
{
	if (shmem->pool) {
		gen_pool_destroy(shmem->pool);
		shmem->pool = NULL;
	}

	if (shmem->baseptr != NULL) {
		KGSL_MEM_INFO("iounmap(shm) = %p\n", shmem->baseptr);
		iounmap(shmem->baseptr);
		shmem->baseptr = NULL;
		release_mem_region(shmem->physbase, shmem->size);
	}

	return 0;
}
/*
* get the host mapped address for a hardware device address
*/
static void *kgsl_memarena_gethostptr(struct kgsl_sharedmem *shmem,
					uint32_t physaddr)
{
	void *result;

	KGSL_MEM_VDBG("enter (memarena=%p, physaddr=0x%08x)\n",
			shmem, physaddr);

	BUG_ON(shmem == NULL);

	/* check address range */
	if (physaddr < shmem->physbase)
		return NULL;

	if (physaddr >= shmem->physbase + shmem->size)
		return NULL;

	if (shmem->baseptr == NULL) {
		KGSL_MEM_VDBG("return: %p\n", NULL);
		return NULL;
	}

	result = ((physaddr - shmem->physbase) + shmem->baseptr);

	KGSL_MEM_VDBG("return: %p\n", result);

	return result;
=======
static int kgsl_vmalloc_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	unsigned long offset, pg;
	struct page *page;

	offset = (unsigned long) vmf->virtual_address - vma->vm_start;
	pg = (unsigned long) memdesc->hostptr + offset;

	page = vmalloc_to_page((void *) pg);
	if (page == NULL)
		return VM_FAULT_SIGBUS;

	get_page(page);

	vmf->page = page;
	return 0;
}

static int kgsl_vmalloc_vmflags(struct kgsl_memdesc *memdesc)
{
	return VM_RESERVED | VM_DONTEXPAND;
}

static void kgsl_vmalloc_free(struct kgsl_memdesc *memdesc)
{
	kgsl_driver.stats.vmalloc -= memdesc->size;
	vfree(memdesc->hostptr);
}

static int kgsl_contiguous_vmflags(struct kgsl_memdesc *memdesc)
{
	return VM_RESERVED | VM_IO | VM_PFNMAP | VM_DONTEXPAND;
}

static int kgsl_contiguous_vmfault(struct kgsl_memdesc *memdesc,
				struct vm_area_struct *vma,
				struct vm_fault *vmf)
{
	unsigned long offset, pfn;
	int ret;

	offset = ((unsigned long) vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;

	pfn = (memdesc->physaddr >> PAGE_SHIFT) + offset;
	ret = vm_insert_pfn(vma, (unsigned long) vmf->virtual_address, pfn);

	if (ret == -ENOMEM || ret == -EAGAIN)
		return VM_FAULT_OOM;
	else if (ret == -EFAULT)
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

static void kgsl_ebimem_free(struct kgsl_memdesc *memdesc)

{
	kgsl_driver.stats.coherent -= memdesc->size;
	if (memdesc->hostptr)
		iounmap(memdesc->hostptr);

	free_contiguous_memory_by_paddr(memdesc->physaddr);
}

static void kgsl_coherent_free(struct kgsl_memdesc *memdesc)
{
	kgsl_driver.stats.coherent -= memdesc->size;
	dma_free_coherent(NULL, memdesc->size,
			  memdesc->hostptr, memdesc->physaddr);
}

/* Global - also used by kgsl_drm.c */
struct kgsl_memdesc_ops kgsl_vmalloc_ops = {
	.free = kgsl_vmalloc_free,
	.vmflags = kgsl_vmalloc_vmflags,
	.vmfault = kgsl_vmalloc_vmfault,
};
EXPORT_SYMBOL(kgsl_vmalloc_ops);

static struct kgsl_memdesc_ops kgsl_ebimem_ops = {
	.free = kgsl_ebimem_free,
	.vmflags = kgsl_contiguous_vmflags,
	.vmfault = kgsl_contiguous_vmfault,
};

static struct kgsl_memdesc_ops kgsl_coherent_ops = {
	.free = kgsl_coherent_free,
};

void kgsl_cache_range_op(struct kgsl_memdesc *memdesc, int op)
{
	void *addr = memdesc->hostptr;
	int size = memdesc->size;

	switch (op) {
	case KGSL_CACHE_OP_FLUSH:
		dmac_flush_range(addr, addr + size);
		break;
	case KGSL_CACHE_OP_CLEAN:
		dmac_clean_range(addr, addr + size);
		break;
	case KGSL_CACHE_OP_INV:
		dmac_inv_range(addr, addr + size);
		break;
	}

	outer_cache_range_op_sg(memdesc->sg, memdesc->sglen, op);
}
EXPORT_SYMBOL(kgsl_cache_range_op);

static int
_kgsl_sharedmem_vmalloc(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			void *ptr, size_t size, unsigned int protflags)
{
	int order, ret = 0;
	int sglen = PAGE_ALIGN(size) / PAGE_SIZE;
	int i;

	memdesc->size = size;
	memdesc->pagetable = pagetable;
	memdesc->priv = KGSL_MEMFLAGS_CACHED;
	memdesc->ops = &kgsl_vmalloc_ops;
	memdesc->hostptr = (void *) ptr;

	memdesc->sg = vmalloc(sglen * sizeof(struct scatterlist));
	if (memdesc->sg == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	memdesc->sglen = sglen;
	sg_init_table(memdesc->sg, sglen);

	for (i = 0; i < memdesc->sglen; i++, ptr += PAGE_SIZE) {
		struct page *page = vmalloc_to_page(ptr);
		if (!page) {
			ret = -EINVAL;
			goto done;
		}
		sg_set_page(&memdesc->sg[i], page, PAGE_SIZE, 0);
	}

	kgsl_cache_range_op(memdesc, KGSL_CACHE_OP_INV);

	ret = kgsl_mmu_map(pagetable, memdesc, protflags);

	if (ret)
		goto done;

	KGSL_STATS_ADD(size, kgsl_driver.stats.vmalloc,
		kgsl_driver.stats.vmalloc_max);

	order = get_order(size);

	if (order < 16)
		kgsl_driver.stats.histogram[order]++;

done:
	if (ret)
		kgsl_sharedmem_free(memdesc);

	return ret;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}


int
kgsl_sharedmem_alloc(uint32_t flags, int size,
			struct kgsl_memdesc *memdesc)
{
<<<<<<< HEAD
	struct kgsl_sharedmem *shmem;
	int result = -ENOMEM;
	unsigned int blksize;
	unsigned int baseaddr;
	unsigned int alignshift;
	unsigned int alignedbaseaddr;

	KGSL_MEM_VDBG("enter (flags=0x%08x, size=%d, memdesc=%p)\n",
					flags, size, memdesc);

	shmem = &kgsl_driver.shmem;
	BUG_ON(memdesc == NULL);
	BUG_ON(size <= 0);

	alignshift = kgsl_memarena_get_order(flags);

	size = ALIGN(size, KGSL_PAGESIZE);
	blksize = size;
	if (alignshift > KGSL_PAGESIZE_SHIFT)
		blksize += (1 << alignshift) - KGSL_PAGESIZE;

	baseaddr = gen_pool_alloc(shmem->pool, blksize);
	if (baseaddr == 0) {
		KGSL_MEM_ERR("gen_pool_alloc failed\n");
		result = -ENOMEM;
		goto done;
=======
	void *ptr;

	BUG_ON(size == 0);

	size = ALIGN(size, PAGE_SIZE * 2);
	ptr = vmalloc(size);

	if (ptr  == NULL) {
		KGSL_CORE_ERR("vmalloc(%d) failed\n", size);
		return -ENOMEM;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}
	result = 0;

	if (alignshift > KGSL_PAGESIZE_SHIFT) {
		alignedbaseaddr = ALIGN(baseaddr, (1 << alignshift));

<<<<<<< HEAD
		KGSL_MEM_VDBG("ba %x al %x as %d m->as %d bs %x s %x\n",
				baseaddr, alignedbaseaddr, alignshift,
				KGSL_PAGESIZE_SHIFT, blksize, size);
		if (alignedbaseaddr > baseaddr) {
			KGSL_MEM_VDBG("physaddr %x free before %x size %x\n",
					alignedbaseaddr,
					baseaddr, alignedbaseaddr - baseaddr);
			gen_pool_free(shmem->pool, baseaddr,
					alignedbaseaddr - baseaddr);
			blksize -= alignedbaseaddr - baseaddr;
		}
		if (blksize > size) {
			KGSL_MEM_VDBG("physaddr %x free after %x size %x\n",
					alignedbaseaddr,
					alignedbaseaddr + size,
					blksize - size);
			gen_pool_free(shmem->pool,
					alignedbaseaddr + size,
					blksize - size);
		}
	} else {
		alignedbaseaddr = baseaddr;
	}

	memdesc->physaddr = alignedbaseaddr;
	memdesc->hostptr = kgsl_memarena_gethostptr(shmem, memdesc->physaddr);
	memdesc->size = size;

	KGSL_MEM_VDBG("ashift %d m->ashift %d blksize %d base %x abase %x\n",
			alignshift, KGSL_PAGESIZE_SHIFT, blksize, baseaddr,
			alignedbaseaddr);

done:
	if (result)
		memset(memdesc, 0, sizeof(*memdesc));


	KGSL_MEM_VDBG("return: %d\n", result);
=======
	return _kgsl_sharedmem_vmalloc(memdesc, pagetable, ptr, size,
		GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);
}
EXPORT_SYMBOL(kgsl_sharedmem_vmalloc);

int
kgsl_sharedmem_vmalloc_user(struct kgsl_memdesc *memdesc,
			    struct kgsl_pagetable *pagetable,
			    size_t size, int flags)
{
	void *ptr;
	unsigned int protflags;

	BUG_ON(size == 0);
	ptr = vmalloc_user(size);

	if (ptr == NULL) {
		KGSL_CORE_ERR("vmalloc_user(%d) failed: allocated=%d\n",
			      size, kgsl_driver.stats.vmalloc);
		return -ENOMEM;
	}

	protflags = GSL_PT_PAGE_RV;
	if (!(flags & KGSL_MEMFLAGS_GPUREADONLY))
		protflags |= GSL_PT_PAGE_WV;

	return _kgsl_sharedmem_vmalloc(memdesc, pagetable, ptr, size,
		protflags);
}
EXPORT_SYMBOL(kgsl_sharedmem_vmalloc_user);

int
kgsl_sharedmem_alloc_coherent(struct kgsl_memdesc *memdesc, size_t size)
{
	int result = 0;

	size = ALIGN(size, PAGE_SIZE);

	memdesc->size = size;
	memdesc->ops = &kgsl_coherent_ops;

	memdesc->hostptr = dma_alloc_coherent(NULL, size, &memdesc->physaddr,
					      GFP_KERNEL);
	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR("dma_alloc_coherent(%d) failed\n", size);
		result = -ENOMEM;
		goto err;
	}

	result = memdesc_sg_phys(memdesc, memdesc->physaddr, size);
	if (result)
		goto err;

	/* Record statistics */

	KGSL_STATS_ADD(size, kgsl_driver.stats.coherent,
		       kgsl_driver.stats.coherent_max);

err:
	if (result)
		kgsl_sharedmem_free(memdesc);

>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	return result;
}
EXPORT_SYMBOL(kgsl_sharedmem_alloc_coherent);

<<<<<<< HEAD
void
kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	struct kgsl_sharedmem  *shmem =  &kgsl_driver.shmem;

	KGSL_MEM_VDBG("enter (shmem=%p, memdesc=%p, physaddr=%08x, size=%d)\n",
			shmem, memdesc, memdesc->physaddr, memdesc->size);

	BUG_ON(memdesc == NULL);
	BUG_ON(memdesc->size <= 0);
	BUG_ON(shmem->physbase > memdesc->physaddr);
	BUG_ON((shmem->physbase + shmem->size)
	       < (memdesc->physaddr + memdesc->size));

	gen_pool_free(shmem->pool, memdesc->physaddr, memdesc->size);
=======
void kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	if (memdesc == NULL || memdesc->size == 0)
		return;

	if (memdesc->gpuaddr)
		kgsl_mmu_unmap(memdesc->pagetable, memdesc);

	if (memdesc->ops && memdesc->ops->free)
		memdesc->ops->free(memdesc);

	vfree(memdesc->sg);

	memset(memdesc, 0, sizeof(*memdesc));
}
EXPORT_SYMBOL(kgsl_sharedmem_free);

static int
_kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable, size_t size)
{
	int result = 0;

	memdesc->size = size;
	memdesc->pagetable = pagetable;
	memdesc->ops = &kgsl_ebimem_ops;
	memdesc->physaddr = allocate_contiguous_memory_nomap(size, PMEM_MEMTYPE_EBI1, SZ_8K);

	if (memdesc->physaddr == 0) {
		KGSL_CORE_ERR("allocate_contiguous_ebi_nomap(%d) failed\n",
			size);
		return -ENOMEM;
	}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	result = memdesc_sg_phys(memdesc, memdesc->physaddr, size);

	if (result)
		goto err;

	result = kgsl_mmu_map(pagetable, memdesc,
		GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);

	if (result)
		goto err;

	KGSL_STATS_ADD(size, kgsl_driver.stats.coherent,
		kgsl_driver.stats.coherent_max);

err:
	if (result)
		kgsl_sharedmem_free(memdesc);

	return result;
}

int
<<<<<<< HEAD
kgsl_sharedmem_read(const struct kgsl_memdesc *memdesc, void *dst,
			unsigned int offsetbytes, unsigned int sizebytes)
{
	if (memdesc == NULL || memdesc->hostptr == NULL || dst == NULL) {
		KGSL_MEM_ERR("bad ptr memdesc %p hostptr %p dst %p\n",
				memdesc,
				(memdesc ? memdesc->hostptr : NULL),
				dst);
		return -EINVAL;
	}
	if (offsetbytes + sizebytes > memdesc->size) {
		KGSL_MEM_ERR("bad range: offset %d size %d memdesc %d\n",
				offsetbytes, sizebytes, memdesc->size);
		return -ERANGE;
=======
kgsl_sharedmem_ebimem_user(struct kgsl_memdesc *memdesc,
			struct kgsl_pagetable *pagetable,
			size_t size, int flags)
{
	size = ALIGN(size, PAGE_SIZE);
	return _kgsl_sharedmem_ebimem(memdesc, pagetable, size);
}
EXPORT_SYMBOL(kgsl_sharedmem_ebimem_user);

int
kgsl_sharedmem_ebimem(struct kgsl_memdesc *memdesc,
		struct kgsl_pagetable *pagetable, size_t size)
{
	int result;
	size = ALIGN(size, 8192);
	result = _kgsl_sharedmem_ebimem(memdesc, pagetable, size);

	if (result)
		return result;

	memdesc->hostptr = ioremap(memdesc->physaddr, size);

	if (memdesc->hostptr == NULL) {
		KGSL_CORE_ERR("ioremap failed\n");
		kgsl_sharedmem_free(memdesc);
		return -ENOMEM;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}

	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_ebimem);

int
<<<<<<< HEAD
kgsl_sharedmem_write(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			void *value, unsigned int sizebytes)
{
	if (memdesc == NULL || memdesc->hostptr == NULL) {
		KGSL_MEM_ERR("bad ptr memdesc %p hostptr %p\n", memdesc,
				(memdesc ? memdesc->hostptr : NULL));
		return -EINVAL;
	}
	if (offsetbytes + sizebytes > memdesc->size) {
		KGSL_MEM_ERR("bad range: offset %d size %d memdesc %d\n",
				offsetbytes, sizebytes, memdesc->size);
		return -ERANGE;
	}
	memcpy(memdesc->hostptr + offsetbytes, value, sizebytes);
=======
kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			unsigned int offsetbytes)
{
	uint32_t *src;
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL || dst == NULL);
	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes + sizeof(uint32_t) > memdesc->size);
	if (offsetbytes + sizeof(uint32_t) > memdesc->size)
		return -ERANGE;
	src = (uint32_t *)(memdesc->hostptr + offsetbytes);
	*dst = *src;
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_readl);

int
kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src)
{
	uint32_t *dst;
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL);
	WARN_ON(offsetbytes % sizeof(uint32_t) != 0);
	if (offsetbytes % sizeof(uint32_t) != 0)
		return -EINVAL;

	WARN_ON(offsetbytes + sizeof(uint32_t) > memdesc->size);
	if (offsetbytes + sizeof(uint32_t) > memdesc->size)
		return -ERANGE;
	kgsl_cffdump_setmem(memdesc->gpuaddr + offsetbytes,
		src, sizeof(uint32_t));
	dst = (uint32_t *)(memdesc->hostptr + offsetbytes);
	*dst = src;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_writel);

int
kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes,
			unsigned int value, unsigned int sizebytes)
{
<<<<<<< HEAD
	if (memdesc == NULL || memdesc->hostptr == NULL) {
		KGSL_MEM_ERR("bad ptr memdesc %p hostptr %p\n", memdesc,
				(memdesc ? memdesc->hostptr : NULL));
		return -EINVAL;
	}
	if (offsetbytes + sizebytes > memdesc->size) {
		KGSL_MEM_ERR("bad range: offset %d size %d memdesc %d\n",
				offsetbytes, sizebytes, memdesc->size);
		return -ERANGE;
	}
=======
	BUG_ON(memdesc == NULL || memdesc->hostptr == NULL);
	BUG_ON(offsetbytes + sizebytes > memdesc->size);

	kgsl_cffdump_setmem(memdesc->gpuaddr + offsetbytes, value,
			    sizebytes);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	memset(memdesc->hostptr + offsetbytes, value, sizebytes);
	return 0;
}
EXPORT_SYMBOL(kgsl_sharedmem_set);
