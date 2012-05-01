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
<<<<<<< HEAD
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>

#include <asm/pgalloc.h>
#include <asm/pgtable.h>

#include "kgsl_mmu.h"
#include "kgsl.h"
#include "kgsl_log.h"
#include "yamato_reg.h"

struct kgsl_pte_debug {
	unsigned int read:1;
	unsigned int write:1;
	unsigned int dirty:1;
	unsigned int reserved:9;
	unsigned int phyaddr:20;
};

#define GSL_PTE_SIZE	4
#define GSL_PT_EXTRA_ENTRIES	16


#define GSL_PT_PAGE_BITS_MASK	0x00000007
#define GSL_PT_PAGE_ADDR_MASK	(~(KGSL_PAGESIZE - 1))

#define GSL_MMU_INT_MASK \
	(MH_INTERRUPT_MASK__AXI_READ_ERROR | \
	 MH_INTERRUPT_MASK__AXI_WRITE_ERROR)

uint32_t kgsl_pt_entry_get(struct kgsl_pagetable *pt, uint32_t va)
{
	return (va - pt->va_base) >> KGSL_PAGESIZE_SHIFT;
}

uint32_t kgsl_pt_map_get(struct kgsl_pagetable *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	return baseptr[pte];
}

void kgsl_pt_map_set(struct kgsl_pagetable *pt, uint32_t pte, uint32_t val)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	baseptr[pte] = val;
}
#define GSL_PT_MAP_DEBUG(pte)	((struct kgsl_pte_debug *) \
		&gsl_pt_map_get(pagetable, pte))

void kgsl_pt_map_setbits(struct kgsl_pagetable *pt, uint32_t pte, uint32_t bits)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	baseptr[pte] |= bits;
}

void kgsl_pt_map_setaddr(struct kgsl_pagetable *pt, uint32_t pte,
					uint32_t pageaddr)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	uint32_t val = baseptr[pte];
	val &= ~GSL_PT_PAGE_ADDR_MASK;
	val |= (pageaddr & GSL_PT_PAGE_ADDR_MASK);
	baseptr[pte] = val;
}

void kgsl_pt_map_resetall(struct kgsl_pagetable *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	baseptr[pte] &= GSL_PT_PAGE_DIRTY;
}

void kgsl_pt_map_resetbits(struct kgsl_pagetable *pt, uint32_t pte,
				uint32_t bits)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	baseptr[pte] &= ~(bits & GSL_PT_PAGE_BITS_MASK);
}

int kgsl_pt_map_isdirty(struct kgsl_pagetable *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	return baseptr[pte] & GSL_PT_PAGE_DIRTY;
}

uint32_t kgsl_pt_map_getaddr(struct kgsl_pagetable *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	return baseptr[pte] & GSL_PT_PAGE_ADDR_MASK;
=======
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/iommu.h>

#include "kgsl.h"
#include "kgsl_mmu.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "adreno_postmortem.h"

#define KGSL_MMU_ALIGN_SHIFT    13
#define KGSL_MMU_ALIGN_MASK     (~((1 << KGSL_MMU_ALIGN_SHIFT) - 1))

static enum kgsl_mmutype kgsl_mmu_type;

static void pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable);

static int kgsl_cleanup_pt(struct kgsl_pagetable *pt)
{
	int i;
	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device)
			device->ftbl->cleanup_pt(device, pt);
	}
	return 0;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}

static void kgsl_destroy_pagetable(struct kref *kref)
{
<<<<<<< HEAD
	unsigned int status = 0;
	unsigned int reg;
	unsigned int axi_error;
	struct kgsl_mmu_debug dbg;
=======
	struct kgsl_pagetable *pagetable = container_of(kref,
		struct kgsl_pagetable, refcount);
	unsigned long flags;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_del(&pagetable->list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

<<<<<<< HEAD
	kgsl_yamato_regread(device, REG_MH_INTERRUPT_STATUS, &status);

	if (status & MH_INTERRUPT_MASK__AXI_READ_ERROR) {
		kgsl_yamato_regread(device, REG_MH_AXI_ERROR, &axi_error);
		KGSL_MEM_FATAL("axi read error interrupt (%08x)\n", axi_error);
		kgsl_mmu_debug(&device->mmu, &dbg);
	} else if (status & MH_INTERRUPT_MASK__AXI_WRITE_ERROR) {
		kgsl_yamato_regread(device, REG_MH_AXI_ERROR, &axi_error);
		KGSL_MEM_FATAL("axi write error interrupt (%08x)\n", axi_error);
		kgsl_mmu_debug(&device->mmu, &dbg);
	} else if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT) {
		kgsl_yamato_regread(device, REG_MH_MMU_PAGE_FAULT, &reg);
		KGSL_MEM_FATAL("mmu page fault interrupt: %08x\n", reg);
		kgsl_mmu_debug(&device->mmu, &dbg);
	} else {
		KGSL_MEM_DBG("bad bits in REG_MH_INTERRUPT_STATUS %08x\n",
			     status);
	}

	kgsl_yamato_regwrite(device, REG_MH_INTERRUPT_CLEAR, status);
=======
	pagetable_remove_sysfs_objects(pagetable);

	kgsl_cleanup_pt(pagetable);

	if (pagetable->pool)
		gen_pool_destroy(pagetable->pool);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	pagetable->pt_ops->mmu_destroy_pagetable(pagetable->priv);

	kfree(pagetable);
}

<<<<<<< HEAD
#ifdef DEBUG
void kgsl_mmu_debug(struct kgsl_mmu *mmu, struct kgsl_mmu_debug *regs)
{
	memset(regs, 0, sizeof(struct kgsl_mmu_debug));

	kgsl_yamato_regread(mmu->device, REG_MH_MMU_CONFIG,
			    &regs->config);
	kgsl_yamato_regread(mmu->device, REG_MH_MMU_MPU_BASE,
			    &regs->mpu_base);
	kgsl_yamato_regread(mmu->device, REG_MH_MMU_MPU_END,
			    &regs->mpu_end);
	kgsl_yamato_regread(mmu->device, REG_MH_MMU_VA_RANGE,
			    &regs->va_range);
	kgsl_yamato_regread(mmu->device, REG_MH_MMU_PT_BASE,
			    &regs->pt_base);
	kgsl_yamato_regread(mmu->device, REG_MH_MMU_PAGE_FAULT,
			    &regs->page_fault);
	kgsl_yamato_regread(mmu->device, REG_MH_MMU_TRAN_ERROR,
			    &regs->trans_error);
	kgsl_yamato_regread(mmu->device, REG_MH_AXI_ERROR,
			    &regs->axi_error);
	kgsl_yamato_regread(mmu->device, REG_MH_INTERRUPT_MASK,
			    &regs->interrupt_mask);
	kgsl_yamato_regread(mmu->device, REG_MH_INTERRUPT_STATUS,
			    &regs->interrupt_status);

	KGSL_MEM_DBG("mmu config %08x mpu_base %08x mpu_end %08x\n",
		     regs->config, regs->mpu_base, regs->mpu_end);
	KGSL_MEM_DBG("mmu va_range %08x pt_base %08x \n",
		     regs->va_range, regs->pt_base);
	KGSL_MEM_DBG("mmu page_fault %08x tran_err %08x\n",
		     regs->page_fault, regs->trans_error);
	KGSL_MEM_DBG("mmu int mask %08x int status %08x\n",
			regs->interrupt_mask, regs->interrupt_status);
=======
static inline void kgsl_put_pagetable(struct kgsl_pagetable *pagetable)
{
	if (pagetable)
		kref_put(&pagetable->refcount, kgsl_destroy_pagetable);
}

static struct kgsl_pagetable *
kgsl_get_pagetable(unsigned long name)
{
	struct kgsl_pagetable *pt, *ret = NULL;
	unsigned long flags;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (pt->name == name) {
			ret = pt;
			kref_get(&ret->refcount);
			break;
		}
	}

	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);
	return ret;
}

static struct kgsl_pagetable *
_get_pt_from_kobj(struct kobject *kobj)
{
	unsigned long ptname;

	if (!kobj)
		return NULL;

	if (sscanf(kobj->name, "%ld", &ptname) != 1)
		return NULL;

	return kgsl_get_pagetable(ptname);
}

static ssize_t
sysfs_show_entries(struct kobject *kobj,
		   struct kobj_attribute *attr,
		   char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.entries);

	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_mapped(struct kobject *kobj,
		  struct kobj_attribute *attr,
		  char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.mapped);

	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_va_range(struct kobject *kobj,
		    struct kobj_attribute *attr,
		    char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "0x%x\n",
			CONFIG_MSM_KGSL_PAGE_TABLE_SIZE);

	kgsl_put_pagetable(pt);
	return ret;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
#endif

<<<<<<< HEAD
struct kgsl_pagetable *kgsl_mmu_createpagetableobject(struct kgsl_mmu *mmu)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;
	uint32_t flags;
=======
static ssize_t
sysfs_show_max_mapped(struct kobject *kobj,
		      struct kobj_attribute *attr,
		      char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.max_mapped);

<<<<<<< HEAD
	pagetable->mmu = mmu;
	pagetable->va_base = mmu->va_base;
	pagetable->va_range = mmu->va_range;
	pagetable->last_superpte = 0;
	pagetable->max_entries = (mmu->va_range >> KGSL_PAGESIZE_SHIFT)
				 + GSL_PT_EXTRA_ENTRIES;

	pagetable->pool = gen_pool_create(KGSL_PAGESIZE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_MEM_ERR("Unable to allocate virtualaddr pool.\n");
		goto err_gen_pool_create;
	}

	if (gen_pool_add(pagetable->pool, pagetable->va_base,
				pagetable->va_range, -1)) {
		KGSL_MEM_ERR("gen_pool_create failed for pagetable %p\n",
				pagetable);
		goto err_gen_pool_add;
	}

	/* allocate page table memory */
	flags = (KGSL_MEMFLAGS_ALIGN4K | KGSL_MEMFLAGS_CONPHYS
		 | KGSL_MEMFLAGS_STRICTREQUEST);
	status = kgsl_sharedmem_alloc(flags,
				      pagetable->max_entries * GSL_PTE_SIZE,
				      &pagetable->base);

	if (status) {
		KGSL_MEM_ERR("cannot alloc page tables\n");
		goto err_kgsl_sharedmem_alloc;
	}

	/* reset page table entries
	 * -- all pte's are marked as not dirty initially
	 */
	kgsl_sharedmem_set(&pagetable->base, 0, 0, pagetable->base.size);
	pagetable->base.gpuaddr = pagetable->base.physaddr;

	KGSL_MEM_VDBG("return %p\n", pagetable);

	return pagetable;

err_kgsl_sharedmem_alloc:
err_gen_pool_add:
	gen_pool_destroy(pagetable->pool);
err_gen_pool_create:
	kfree(pagetable);
	return NULL;
}

int kgsl_mmu_destroypagetableobject(struct kgsl_pagetable *pagetable)
=======
	kgsl_put_pagetable(pt);
	return ret;
}

static ssize_t
sysfs_show_max_entries(struct kobject *kobj,
		       struct kobj_attribute *attr,
		       char *buf)
{
	struct kgsl_pagetable *pt;
	int ret = 0;

	pt = _get_pt_from_kobj(kobj);

	if (pt)
		ret += snprintf(buf, PAGE_SIZE, "%d\n", pt->stats.max_entries);

	kgsl_put_pagetable(pt);
	return ret;
}

static struct kobj_attribute attr_entries = {
	.attr = { .name = "entries", .mode = 0444 },
	.show = sysfs_show_entries,
	.store = NULL,
};

static struct kobj_attribute attr_mapped = {
	.attr = { .name = "mapped", .mode = 0444 },
	.show = sysfs_show_mapped,
	.store = NULL,
};

static struct kobj_attribute attr_va_range = {
	.attr = { .name = "va_range", .mode = 0444 },
	.show = sysfs_show_va_range,
	.store = NULL,
};

static struct kobj_attribute attr_max_mapped = {
	.attr = { .name = "max_mapped", .mode = 0444 },
	.show = sysfs_show_max_mapped,
	.store = NULL,
};

static struct kobj_attribute attr_max_entries = {
	.attr = { .name = "max_entries", .mode = 0444 },
	.show = sysfs_show_max_entries,
	.store = NULL,
};

static struct attribute *pagetable_attrs[] = {
	&attr_entries.attr,
	&attr_mapped.attr,
	&attr_va_range.attr,
	&attr_max_mapped.attr,
	&attr_max_entries.attr,
	NULL,
};

static struct attribute_group pagetable_attr_group = {
	.attrs = pagetable_attrs,
};

static void
pagetable_remove_sysfs_objects(struct kgsl_pagetable *pagetable)
{
	if (pagetable->kobj)
		sysfs_remove_group(pagetable->kobj,
				   &pagetable_attr_group);

	kobject_put(pagetable->kobj);
}

static int
pagetable_add_sysfs_objects(struct kgsl_pagetable *pagetable)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	char ptname[16];
	int ret = -ENOMEM;

<<<<<<< HEAD
	if (pagetable) {
		if (pagetable->base.gpuaddr)
			kgsl_sharedmem_free(&pagetable->base);

		if (pagetable->pool) {
			gen_pool_destroy(pagetable->pool);
			pagetable->pool = NULL;
		}

		kfree(pagetable);

	}
	KGSL_MEM_VDBG("return 0x%08x\n", 0);

	return 0;
=======
	snprintf(ptname, sizeof(ptname), "%d", pagetable->name);
	pagetable->kobj = kobject_create_and_add(ptname,
						 kgsl_driver.ptkobj);
	if (pagetable->kobj == NULL)
		goto err;

	ret = sysfs_create_group(pagetable->kobj, &pagetable_attr_group);

err:
	if (ret) {
		if (pagetable->kobj)
			kobject_put(pagetable->kobj);

		pagetable->kobj = NULL;
	}

	return ret;
}

unsigned int kgsl_mmu_get_current_ptbase(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return 0;
	else
		return mmu->mmu_ops->mmu_get_current_ptbase(device);
}
EXPORT_SYMBOL(kgsl_mmu_get_current_ptbase);

int
kgsl_mmu_get_ptname_from_ptbase(unsigned int pt_base)
{
	struct kgsl_pagetable *pt;
	int ptid = -1;

	spin_lock(&kgsl_driver.ptlock);
	list_for_each_entry(pt, &kgsl_driver.pagetable_list, list) {
		if (pt->pt_ops->mmu_pt_equal(pt, pt_base)) {
			ptid = (int) pt->name;
			break;
		}
	}
	spin_unlock(&kgsl_driver.ptlock);

	return ptid;
}
EXPORT_SYMBOL(kgsl_mmu_get_ptname_from_ptbase);

void kgsl_mmu_setstate(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else
		mmu->mmu_ops->mmu_setstate(device,
					pagetable);
}
EXPORT_SYMBOL(kgsl_mmu_setstate);

int kgsl_mmu_init(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	mmu->device = device;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type) {
		dev_info(device->dev, "|%s| MMU type set for device is "
			"NOMMU\n", __func__);
		return 0;
	} else if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		mmu->mmu_ops = &gpummu_ops;
#ifdef CONFIG_MSM_IOMMU
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		mmu->mmu_ops = &iommu_ops;
#endif

	return mmu->mmu_ops->mmu_init(device);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
EXPORT_SYMBOL(kgsl_mmu_init);

int kgsl_mmu_start(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

<<<<<<< HEAD
	KGSL_MEM_VDBG("enter (device=%p, pagetable=%p)\n", device, pagetable);

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		/* page table not current, then setup mmu to use new
		 *  specified page table
		 */
		KGSL_MEM_INFO("from %p to %p\n", mmu->hwpagetable, pagetable);
		if (mmu->hwpagetable != pagetable) {
			mmu->hwpagetable = pagetable;

			/* call device specific set page table */
			status = kgsl_yamato_setstate(mmu->device,
				KGSL_MMUFLAGS_TLBFLUSH |
				KGSL_MMUFLAGS_PTUPDATE);
		}
	}
=======
	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		kgsl_regwrite(device, MH_MMU_CONFIG, 0);
		return 0;
	} else {
		return mmu->mmu_ops->mmu_start(device);
	}
}
EXPORT_SYMBOL(kgsl_mmu_start);

void kgsl_mh_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	unsigned int reg;

	kgsl_regread(device, MH_INTERRUPT_STATUS, &status);
	kgsl_regread(device, MH_AXI_ERROR, &reg);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	if (status & MH_INTERRUPT_MASK__AXI_READ_ERROR)
		KGSL_MEM_CRIT(device, "axi read error interrupt: %08x\n", reg);
	if (status & MH_INTERRUPT_MASK__AXI_WRITE_ERROR)
		KGSL_MEM_CRIT(device, "axi write error interrupt: %08x\n", reg);
	if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT)
		device->mmu.mmu_ops->mmu_pagefault(device);

	status &= KGSL_MMU_INT_MASK;
	kgsl_regwrite(device, MH_INTERRUPT_CLEAR, status);
}
EXPORT_SYMBOL(kgsl_mh_intrcallback);

static int kgsl_setup_pt(struct kgsl_pagetable *pt)
{
<<<<<<< HEAD
	/*
	 * intialize device mmu
	 *
	 * call this with the global lock held
	 */
	int status;
	uint32_t flags;
	struct kgsl_mmu *mmu = &device->mmu;
#ifdef _DEBUG
	struct kgsl_mmu_debug regs;
#endif /* _DEBUG */

	KGSL_MEM_VDBG("enter (device=%p)\n", device);

	if (mmu->flags & KGSL_FLAGS_INITIALIZED0) {
		KGSL_MEM_INFO("MMU already initialized.\n");
		return 0;
	}

	mmu->device = device;
=======
	int i = 0;
	int status = 0;

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device) {
			status = device->ftbl->setup_pt(device, pt);
			if (status)
				goto error_pt;
		}
	}
	return status;
error_pt:
	while (i >= 0) {
		struct kgsl_device *device = kgsl_driver.devp[i];
		if (device)
			device->ftbl->cleanup_pt(device, pt);
		i--;
	}
	return status;
}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

static struct kgsl_pagetable *kgsl_mmu_createpagetableobject(
				unsigned int name)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;
	unsigned long flags;

<<<<<<< HEAD
	/* setup MMU and sub-client behavior */
	kgsl_yamato_regwrite(device, REG_MH_MMU_CONFIG, mmu->config);

	/* enable axi interrupts */
	KGSL_MEM_DBG("enabling mmu interrupts mask=0x%08lx\n",
		     GSL_MMU_INT_MASK);
	kgsl_yamato_regwrite(device, REG_MH_INTERRUPT_MASK, GSL_MMU_INT_MASK);

	mmu->flags |= KGSL_FLAGS_INITIALIZED0;

	/* MMU not enabled */
	if ((mmu->config & 0x1) == 0) {
		KGSL_MEM_VDBG("return %d\n", 0);
		return 0;
	}

	/* idle device */
	kgsl_yamato_idle(device, KGSL_TIMEOUT_DEFAULT);

	/* make sure aligned to pagesize */
	BUG_ON(mmu->mpu_base & (KGSL_PAGESIZE - 1));
	BUG_ON((mmu->mpu_base + mmu->mpu_range) & (KGSL_PAGESIZE - 1));

	/* define physical memory range accessible by the core */
	kgsl_yamato_regwrite(device, REG_MH_MMU_MPU_BASE,
				mmu->mpu_base);
	kgsl_yamato_regwrite(device, REG_MH_MMU_MPU_END,
				mmu->mpu_base + mmu->mpu_range);

	/* enable axi interrupts */
	KGSL_MEM_DBG("enabling mmu interrupts mask=0x%08lx\n",
		     GSL_MMU_INT_MASK | MH_INTERRUPT_MASK__MMU_PAGE_FAULT);
	kgsl_yamato_regwrite(device, REG_MH_INTERRUPT_MASK,
			GSL_MMU_INT_MASK | MH_INTERRUPT_MASK__MMU_PAGE_FAULT);

	mmu->flags |= KGSL_FLAGS_INITIALIZED;

	/* sub-client MMU lookups require address translation */
	if ((mmu->config & ~0x1) > 0) {
		/*make sure virtual address range is a multiple of 64Kb */
		BUG_ON(mmu->va_range & ((1 << 16) - 1));

		/* allocate memory used for completing r/w operations that
		 * cannot be mapped by the MMU
		 */
		flags = (KGSL_MEMFLAGS_ALIGN4K | KGSL_MEMFLAGS_CONPHYS
			 | KGSL_MEMFLAGS_STRICTREQUEST);
		status = kgsl_sharedmem_alloc(flags, 64, &mmu->dummyspace);
		if (status != 0) {
			KGSL_MEM_ERR
			    ("Unable to allocate dummy space memory.\n");
			kgsl_mmu_close(device);
			return status;
		}

		kgsl_sharedmem_set(&mmu->dummyspace, 0, 0,
				   mmu->dummyspace.size);
		/* TRAN_ERROR needs a 32 byte (32 byte aligned) chunk of memory
		 * to complete transactions in case of an MMU fault. Note that
		 * we'll leave the bottom 32 bytes of the dummyspace for other
		 * purposes (e.g. use it when dummy read cycles are needed
		 * for other blocks */
		kgsl_yamato_regwrite(device,
				     REG_MH_MMU_TRAN_ERROR,
				     mmu->dummyspace.physaddr + 32);

		mmu->defaultpagetable = kgsl_mmu_createpagetableobject(mmu);
		if (!mmu->defaultpagetable) {
			KGSL_MEM_ERR("Failed to create global page table\n");
			kgsl_mmu_close(device);
			return -ENOMEM;
		}
		mmu->hwpagetable = mmu->defaultpagetable;
		kgsl_yamato_regwrite(device, REG_MH_MMU_PT_BASE,
					mmu->hwpagetable->base.gpuaddr);
		kgsl_yamato_regwrite(device, REG_MH_MMU_VA_RANGE,
				(mmu->hwpagetable->va_base |
				(mmu->hwpagetable->va_range >> 16)));
		status = kgsl_yamato_setstate(device, KGSL_MMUFLAGS_TLBFLUSH);
		if (status) {
			kgsl_mmu_close(device);
			return status;
		}

		mmu->flags |= KGSL_FLAGS_STARTED;
	}
=======
	pagetable = kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
	if (pagetable == NULL) {
		KGSL_CORE_ERR("kzalloc(%d) failed\n",
			sizeof(struct kgsl_pagetable));
		return NULL;
	}

	kref_init(&pagetable->refcount);

	spin_lock_init(&pagetable->lock);
	pagetable->name = name;
	pagetable->max_entries = KGSL_PAGETABLE_ENTRIES(
					CONFIG_MSM_KGSL_PAGE_TABLE_SIZE);

	pagetable->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		KGSL_CORE_ERR("gen_pool_create(%d) failed\n", PAGE_SHIFT);
		goto err_alloc;
	}

	if (gen_pool_add(pagetable->pool, KGSL_PAGETABLE_BASE,
				CONFIG_MSM_KGSL_PAGE_TABLE_SIZE, -1)) {
		KGSL_CORE_ERR("gen_pool_add failed\n");
		goto err_pool;
	}

	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		pagetable->pt_ops = &gpummu_pt_ops;
#ifdef CONFIG_MSM_IOMMU
	else if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		pagetable->pt_ops = &iommu_pt_ops;
#endif

	pagetable->priv = pagetable->pt_ops->mmu_create_pagetable();
	if (!pagetable->priv)
		goto err_pool;

	status = kgsl_setup_pt(pagetable);
	if (status)
		goto err_mmu_create;

	spin_lock_irqsave(&kgsl_driver.ptlock, flags);
	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	spin_unlock_irqrestore(&kgsl_driver.ptlock, flags);

	/* Create the sysfs entries */
	pagetable_add_sysfs_objects(pagetable);

	return pagetable;

err_mmu_create:
	pagetable->pt_ops->mmu_destroy_pagetable(pagetable->priv);
err_pool:
	gen_pool_destroy(pagetable->pool);
err_alloc:
	kfree(pagetable);

	return NULL;
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return (void *)(-1);

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
	if (KGSL_MMU_TYPE_IOMMU == kgsl_mmu_type)
		name = KGSL_MMU_GLOBAL_PT;
#else
		name = KGSL_MMU_GLOBAL_PT;
#endif
	pt = kgsl_get_pagetable(name);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	if (pt == NULL)
		pt = kgsl_mmu_createpagetableobject(name);

<<<<<<< HEAD
	return 0;
}

#ifdef CONFIG_MSM_KGSL_MMU
pte_t *kgsl_get_pte_from_vaddr(unsigned int vaddr)
{
	pgd_t *pgd_ptr = NULL;
	pmd_t *pmd_ptr = NULL;
	pte_t *pte_ptr = NULL;

	pgd_ptr = pgd_offset(current->mm, vaddr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		KGSL_MEM_ERR
		    ("Invalid pgd entry found while trying to convert virtual "
		     "address to physical\n");
		return 0;
	}

	pmd_ptr = pmd_offset(pgd_ptr, vaddr);
	if (pmd_none(*pmd_ptr) || pmd_bad(*pmd_ptr)) {
		KGSL_MEM_ERR
		    ("Invalid pmd entry found while trying to convert virtual "
		     "address to physical\n");
		return 0;
	}

	pte_ptr = pte_offset_map(pmd_ptr, vaddr);
	if (!pte_ptr) {
		KGSL_MEM_ERR
		    ("Unable to map pte entry while trying to convert virtual "
		     "address to physical\n");
		return 0;
	}
	return pte_ptr;
}

int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				unsigned int address,
				int range,
				unsigned int protflags,
				unsigned int *gpuaddr,
				unsigned int flags)
{
	int numpages;
	unsigned int pte, superpte, ptefirst, ptelast, physaddr;
	int flushtlb, alloc_size;
	struct kgsl_mmu *mmu = NULL;
	int phys_contiguous = flags & KGSL_MEMFLAGS_CONPHYS;
	unsigned int align = flags & KGSL_MEMFLAGS_ALIGN_MASK;

	KGSL_MEM_VDBG("enter (pt=%p, physaddr=%08x, range=%08d, gpuaddr=%p)\n",
		      pagetable, address, range, gpuaddr);

	mmu = pagetable->mmu;

	BUG_ON(mmu == NULL);
	BUG_ON(protflags & ~(GSL_PT_PAGE_RV | GSL_PT_PAGE_WV));
	BUG_ON(protflags == 0);
	BUG_ON(range <= 0);

	/* Only support 4K and 8K alignment for now */
	if (align != KGSL_MEMFLAGS_ALIGN8K && align != KGSL_MEMFLAGS_ALIGN4K) {
		KGSL_MEM_ERR("Cannot map memory according to "
			     "requested flags: %08x\n", flags);
		return -EINVAL;
	}
=======
	return pt;
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	kgsl_put_pagetable(pagetable);
}
EXPORT_SYMBOL(kgsl_mmu_putpagetable);

void kgsl_setstate(struct kgsl_device *device, uint32_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else if (device->ftbl->setstate)
		device->ftbl->setstate(device, flags);
	else if (mmu->mmu_ops->mmu_device_setstate)
		mmu->mmu_ops->mmu_device_setstate(device, flags);
}
EXPORT_SYMBOL(kgsl_setstate);

void kgsl_mmu_device_setstate(struct kgsl_device *device, uint32_t flags)
{
	struct kgsl_mmu *mmu = &device->mmu;
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return;
	else if (mmu->mmu_ops->mmu_device_setstate)
		mmu->mmu_ops->mmu_device_setstate(device, flags);
}
EXPORT_SYMBOL(kgsl_mmu_device_setstate);

void kgsl_mh_start(struct kgsl_device *device)
{
	struct kgsl_mh *mh = &device->mh;
	/* force mmu off to for now*/
	kgsl_regwrite(device, MH_MMU_CONFIG, 0);
	kgsl_idle(device,  KGSL_TIMEOUT_DEFAULT);

	/* define physical memory range accessible by the core */
	kgsl_regwrite(device, MH_MMU_MPU_BASE, mh->mpu_base);
	kgsl_regwrite(device, MH_MMU_MPU_END,
			mh->mpu_base + mh->mpu_range);
	kgsl_regwrite(device, MH_ARBITER_CONFIG, mh->mharb);

	if (mh->mh_intf_cfg1 != 0)
		kgsl_regwrite(device, MH_CLNT_INTF_CTRL_CONFIG1,
				mh->mh_intf_cfg1);

	if (mh->mh_intf_cfg2 != 0)
		kgsl_regwrite(device, MH_CLNT_INTF_CTRL_CONFIG2,
				mh->mh_intf_cfg2);

	/*
	 * Interrupts are enabled on a per-device level when
	 * kgsl_pwrctrl_irq() is called
	 */
}

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				struct kgsl_memdesc *memdesc,
				unsigned int protflags)
{
	int ret;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		memdesc->gpuaddr = memdesc->physaddr;
		return 0;
	}
	memdesc->gpuaddr = gen_pool_alloc_aligned(pagetable->pool,
		memdesc->size, KGSL_MMU_ALIGN_SHIFT);

	if (memdesc->gpuaddr == 0) {
		KGSL_CORE_ERR("gen_pool_alloc(%d) failed\n", memdesc->size);
		KGSL_CORE_ERR(" [%d] allocated=%d, entries=%d\n",
				pagetable->name, pagetable->stats.mapped,
				pagetable->stats.entries);
		return -ENOMEM;
	}

<<<<<<< HEAD
	if (align == KGSL_MEMFLAGS_ALIGN8K) {
		if (*gpuaddr & ((1 << 13) - 1)) {
			/* Not 8k aligned, align it */
			gen_pool_free(pagetable->pool, *gpuaddr, KGSL_PAGESIZE);
			*gpuaddr = *gpuaddr + KGSL_PAGESIZE;
		} else
			gen_pool_free(pagetable->pool, *gpuaddr + range,
				      KGSL_PAGESIZE);
	}

	numpages = (range >> KGSL_PAGESIZE_SHIFT);

	ptefirst = kgsl_pt_entry_get(pagetable, *gpuaddr);
	ptelast = ptefirst + numpages;

	pte = ptefirst;
	flushtlb = 0;

	superpte = ptefirst & (GSL_PT_SUPER_PTE - 1);
	for (pte = superpte; pte < ptefirst; pte++) {
		/* tlb needs to be flushed only when a dirty superPTE
		   gets backed */
		if (kgsl_pt_map_isdirty(pagetable, pte)) {
			flushtlb = 1;
			break;
		}
	}

	for (pte = ptefirst; pte < ptelast; pte++) {
#ifdef VERBOSE_DEBUG
		/* check if PTE exists */
		uint32_t val = kgsl_pt_map_getaddr(pagetable, pte);
		BUG_ON(val != 0 && val != GSL_PT_PAGE_DIRTY);
#endif
		if (kgsl_pt_map_isdirty(pagetable, pte))
			flushtlb = 1;
		/* mark pte as in use */
		if (phys_contiguous)
			physaddr = address;
		else {
			physaddr = vmalloc_to_pfn((void *)address);
			physaddr <<= PAGE_SHIFT;
		}

		if (physaddr)
			kgsl_pt_map_set(pagetable, pte, physaddr | protflags);
		else {
			KGSL_MEM_ERR
			("Unable to find physaddr for vmallloc address: %x\n",
			     address);
			kgsl_mmu_unmap(pagetable, *gpuaddr, range);
			return -EFAULT;
		}
		address += KGSL_PAGESIZE;
	}

	/* set superpte to end of next superpte */
	superpte = (ptelast + (GSL_PT_SUPER_PTE - 1))
			& (GSL_PT_SUPER_PTE - 1);
	for (pte = ptelast; pte < superpte; pte++) {
		/* tlb needs to be flushed only when a dirty superPTE
		   gets backed */
		if (kgsl_pt_map_isdirty(pagetable, pte)) {
			flushtlb = 1;
			break;
		}
	}
	KGSL_MEM_INFO("pt %p p %08x g %08x pte f %d l %d n %d f %d\n",
		      pagetable, address, *gpuaddr, ptefirst, ptelast,
		      numpages, flushtlb);

	dmb();

	/* Invalidate tlb only if current page table used by GPU is the
	 * pagetable that we used to allocate */
	if (pagetable == mmu->hwpagetable)
		kgsl_yamato_setstate(mmu->device, KGSL_MMUFLAGS_TLBFLUSH);
=======
	spin_lock(&pagetable->lock);
	ret = pagetable->pt_ops->mmu_map(pagetable->priv, memdesc, protflags);

	if (ret)
		goto err_free_gpuaddr;

	/* Keep track of the statistics for the sysfs files */

	KGSL_STATS_ADD(1, pagetable->stats.entries,
		       pagetable->stats.max_entries);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	KGSL_STATS_ADD(memdesc->size, pagetable->stats.mapped,
		       pagetable->stats.max_mapped);

	spin_unlock(&pagetable->lock);

	return 0;

err_free_gpuaddr:
	spin_unlock(&pagetable->lock);
	gen_pool_free(pagetable->pool, memdesc->gpuaddr, memdesc->size);
	memdesc->gpuaddr = 0;
	return ret;
}
EXPORT_SYMBOL(kgsl_mmu_map);

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
		struct kgsl_memdesc *memdesc)
{
<<<<<<< HEAD
	unsigned int numpages;
	unsigned int pte, ptefirst, ptelast;

	KGSL_MEM_VDBG("enter (pt=%p, gpuaddr=0x%08x, range=%d)\n",
			pagetable, gpuaddr, range);

	BUG_ON(range <= 0);

	numpages = (range >> KGSL_PAGESIZE_SHIFT);
	if (range & (KGSL_PAGESIZE - 1))
		numpages++;

	ptefirst = kgsl_pt_entry_get(pagetable, gpuaddr);
	ptelast = ptefirst + numpages;

	KGSL_MEM_INFO("pt %p gpu %08x pte first %d last %d numpages %d\n",
		      pagetable, gpuaddr, ptefirst, ptelast, numpages);

	for (pte = ptefirst; pte < ptelast; pte++) {
#ifdef VERBOSE_DEBUG
		/* check if PTE exists */
		BUG_ON(!kgsl_pt_map_getaddr(pagetable, pte));
#endif
		kgsl_pt_map_set(pagetable, pte, GSL_PT_PAGE_DIRTY);
	}

	dmb();
=======
	if (memdesc->size == 0 || memdesc->gpuaddr == 0)
		return 0;

	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE) {
		memdesc->gpuaddr = 0;
		return 0;
	}
	spin_lock(&pagetable->lock);
	pagetable->pt_ops->mmu_unmap(pagetable->priv, memdesc);
	/* Remove the statistics */
	pagetable->stats.entries--;
	pagetable->stats.mapped -= memdesc->size;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	/* Invalidate tlb only if current page table used by GPU is the
	 * pagetable that we used to allocate */
	if (pagetable == pagetable->mmu->hwpagetable)
		kgsl_yamato_setstate(pagetable->mmu->device,
					KGSL_MMUFLAGS_TLBFLUSH);

	gen_pool_free(pagetable->pool,
			memdesc->gpuaddr & KGSL_MMU_ALIGN_MASK,
			memdesc->size);

	return 0;
}
<<<<<<< HEAD
#endif
=======
EXPORT_SYMBOL(kgsl_mmu_unmap);

int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc, unsigned int protflags)
{
	int result = -EINVAL;
	unsigned int gpuaddr = 0;

	if (memdesc == NULL) {
		KGSL_CORE_ERR("invalid memdesc\n");
		goto error;
	}
	/* Not all global mappings are needed for all MMU types */
	if (!memdesc->size)
		return 0;

	gpuaddr = memdesc->gpuaddr;

	result = kgsl_mmu_map(pagetable, memdesc, protflags);
	if (result)
		goto error;

	/*global mappings must have the same gpu address in all pagetables*/
	if (gpuaddr && gpuaddr != memdesc->gpuaddr) {
		KGSL_CORE_ERR("pt %p addr mismatch phys 0x%08x"
			"gpu 0x%0x 0x%08x", pagetable, memdesc->physaddr,
			gpuaddr, memdesc->gpuaddr);
		goto error_unmap;
	}
	return result;
error_unmap:
	kgsl_mmu_unmap(pagetable, memdesc);
error:
	return result;
}
EXPORT_SYMBOL(kgsl_mmu_map_global);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

int kgsl_mmu_close(struct kgsl_device *device)
{
<<<<<<< HEAD
	/*
	 *  close device mmu
	 *
	 *  call this with the global lock held
	 */
=======
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	struct kgsl_mmu *mmu = &device->mmu;
#ifdef _DEBUG
	int i;
#endif /* _DEBUG */

<<<<<<< HEAD
	KGSL_MEM_VDBG("enter (device=%p)\n", device);

	if (mmu->flags & KGSL_FLAGS_INITIALIZED0) {
		/* disable mh interrupts */
		KGSL_MEM_DBG("disabling mmu interrupts\n");
		kgsl_yamato_regwrite(device, REG_MH_INTERRUPT_MASK, 0);

		/* disable MMU */
		kgsl_yamato_regwrite(device, REG_MH_MMU_CONFIG, 0x00000000);

		if (mmu->dummyspace.gpuaddr)
			kgsl_sharedmem_free(&mmu->dummyspace);

		mmu->flags &= ~KGSL_FLAGS_STARTED;
		mmu->flags &= ~KGSL_FLAGS_INITIALIZED;
		mmu->flags &= ~KGSL_FLAGS_INITIALIZED0;
		kgsl_mmu_destroypagetableobject(mmu->defaultpagetable);
		mmu->defaultpagetable = NULL;
	}
=======
	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return 0;
	else
		return mmu->mmu_ops->mmu_stop(device);
}
EXPORT_SYMBOL(kgsl_mmu_stop);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

int kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

<<<<<<< HEAD
	return 0;
=======
	if (kgsl_mmu_type == KGSL_MMU_TYPE_NONE)
		return 0;
	else
		return mmu->mmu_ops->mmu_close(device);
}
EXPORT_SYMBOL(kgsl_mmu_close);

int kgsl_mmu_pt_get_flags(struct kgsl_pagetable *pt,
			enum kgsl_deviceid id)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		return pt->pt_ops->mmu_pt_get_flags(pt, id);
	else
		return 0;
}
EXPORT_SYMBOL(kgsl_mmu_pt_get_flags);

void kgsl_mmu_ptpool_destroy(void *ptpool)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		kgsl_gpummu_ptpool_destroy(ptpool);
	ptpool = 0;
}
EXPORT_SYMBOL(kgsl_mmu_ptpool_destroy);

void *kgsl_mmu_ptpool_init(int ptsize, int entries)
{
	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_type)
		return kgsl_gpummu_ptpool_init(ptsize, entries);
	else
		return (void *)(-1);
}
EXPORT_SYMBOL(kgsl_mmu_ptpool_init);

int kgsl_mmu_enabled(void)
{
	if (KGSL_MMU_TYPE_NONE != kgsl_mmu_type)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(kgsl_mmu_enabled);

int kgsl_mmu_pt_equal(struct kgsl_pagetable *pt,
			unsigned int pt_base)
{
	if (KGSL_MMU_TYPE_NONE == kgsl_mmu_type)
		return true;
	else
		return pt->pt_ops->mmu_pt_equal(pt, pt_base);
}
EXPORT_SYMBOL(kgsl_mmu_pt_equal);

enum kgsl_mmutype kgsl_mmu_get_mmutype(void)
{
	return kgsl_mmu_type;
}
EXPORT_SYMBOL(kgsl_mmu_get_mmutype);

void kgsl_mmu_set_mmutype(char *mmutype)
{
	kgsl_mmu_type = iommu_found() ? KGSL_MMU_TYPE_IOMMU : KGSL_MMU_TYPE_GPU;
	if (mmutype && !strncmp(mmutype, "gpummu", 6))
		kgsl_mmu_type = KGSL_MMU_TYPE_GPU;
	if (iommu_found() && mmutype && !strncmp(mmutype, "iommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_IOMMU;
	if (mmutype && !strncmp(mmutype, "nommu", 5))
		kgsl_mmu_type = KGSL_MMU_TYPE_NONE;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
EXPORT_SYMBOL(kgsl_mmu_set_mmutype);
