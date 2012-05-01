<<<<<<< HEAD
/*
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
#ifndef _GSL_DRIVER_H
#define _GSL_DRIVER_H
=======
/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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
#ifndef __KGSL_H
#define __KGSL_H
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/wakelock.h>

<<<<<<< HEAD
#include <asm/atomic.h>

#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

#define DRIVER_NAME "kgsl"

struct kgsl_driver {
	struct miscdevice misc;
	struct platform_device *pdev;
	atomic_t open_count;
	struct mutex mutex;

	int interrupt_num;
	int have_irq;

	struct clk *grp_clk;
	struct clk *grp_pclk;
	struct clk *imem_clk;
	struct clk *ebi1_clk;

	struct kgsl_devconfig yamato_config;

	uint32_t flags_debug;

	struct kgsl_sharedmem shmem;
	struct kgsl_device yamato_device;

	struct list_head client_list;

	bool active;
	int active_cnt;
	struct timer_list standby_timer;

	struct wake_lock wake_lock;
=======
#define KGSL_NAME "kgsl"

/*cache coherency ops */
#define DRM_KGSL_GEM_CACHE_OP_TO_DEV	0x0001
#define DRM_KGSL_GEM_CACHE_OP_FROM_DEV	0x0002

/* The size of each entry in a page table */
#define KGSL_PAGETABLE_ENTRY_SIZE  4

/* Pagetable Virtual Address base */
#define KGSL_PAGETABLE_BASE	0x66000000

/* Extra accounting entries needed in the pagetable */
#define KGSL_PT_EXTRA_ENTRIES      16

#define KGSL_PAGETABLE_ENTRIES(_sz) (((_sz) >> PAGE_SHIFT) + \
				     KGSL_PT_EXTRA_ENTRIES)

#define KGSL_PAGETABLE_SIZE \
ALIGN(KGSL_PAGETABLE_ENTRIES(CONFIG_MSM_KGSL_PAGE_TABLE_SIZE) * \
KGSL_PAGETABLE_ENTRY_SIZE, PAGE_SIZE)

#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
#define KGSL_PAGETABLE_COUNT (CONFIG_MSM_KGSL_PAGE_TABLE_COUNT)
#else
#define KGSL_PAGETABLE_COUNT 1
#endif

/* Casting using container_of() for structures that kgsl owns. */
#define KGSL_CONTAINER_OF(ptr, type, member) \
		container_of(ptr, type, member)

/* A macro for memory statistics - add the new size to the stat and if
   the statisic is greater then _max, set _max
*/

#define KGSL_STATS_ADD(_size, _stat, _max) \
	do { _stat += (_size); if (_stat > _max) _max = _stat; } while (0)

struct kgsl_device;

struct kgsl_driver {
	struct cdev cdev;
	dev_t major;
	struct class *class;
	/* Virtual device for managing the core */
	struct device virtdev;
	/* Kobjects for storing pagetable and process statistics */
	struct kobject *ptkobj;
	struct kobject *prockobj;
	struct kgsl_device *devp[KGSL_DEVICE_MAX];

	/* Global lilst of open processes */
	struct list_head process_list;
	/* Global list of pagetables */
	struct list_head pagetable_list;
	/* Spinlock for accessing the pagetable list */
	spinlock_t ptlock;
	/* Mutex for accessing the process list */
	struct mutex process_mutex;

	/* Mutex for protecting the device list */
	struct mutex devlock;

	void *ptpool;

	struct {
		unsigned int vmalloc;
		unsigned int vmalloc_max;
		unsigned int coherent;
		unsigned int coherent_max;
		unsigned int mapped;
		unsigned int mapped_max;
		unsigned int histogram[16];
	} stats;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
};

extern struct kgsl_driver kgsl_driver;

struct kgsl_pagetable;
struct kgsl_memdesc_ops;

/* shared memory allocation */
struct kgsl_memdesc {
	struct kgsl_pagetable *pagetable;
	void *hostptr;
	unsigned int gpuaddr;
	unsigned int physaddr;
	unsigned int size;
	unsigned int priv;
	struct scatterlist *sg;
	unsigned int sglen;
	struct kgsl_memdesc_ops *ops;
};

/* List of different memory entry types */

#define KGSL_MEM_ENTRY_KERNEL 0
#define KGSL_MEM_ENTRY_PMEM   1
#define KGSL_MEM_ENTRY_ASHMEM 2
#define KGSL_MEM_ENTRY_USER   3
#define KGSL_MEM_ENTRY_ION    4
#define KGSL_MEM_ENTRY_MAX    5

struct kgsl_mem_entry {
	struct kref refcount;
	struct kgsl_memdesc memdesc;
<<<<<<< HEAD
	struct file *pmem_file;
=======
	int memtype;
	void *priv_data;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	struct list_head list;
	struct list_head free_list;
	uint32_t free_timestamp;

<<<<<<< HEAD
	/* back pointer to private structure under whose context this
	 * allocation is made */
	struct kgsl_file_private *priv;
};

void kgsl_remove_mem_entry(struct kgsl_mem_entry *entry);
=======
#ifdef CONFIG_MSM_KGSL_MMU_PAGE_FAULT
#define MMU_CONFIG 2
#else
#define MMU_CONFIG 1
#endif

void kgsl_mem_entry_destroy(struct kref *kref);
struct kgsl_mem_entry *kgsl_sharedmem_find_region(
	struct kgsl_process_private *private, unsigned int gpuaddr,
	size_t size);

extern const struct dev_pm_ops kgsl_pm_ops;

struct early_suspend;
int kgsl_suspend_driver(struct platform_device *pdev, pm_message_t state);
int kgsl_resume_driver(struct platform_device *pdev);
void kgsl_early_suspend_driver(struct early_suspend *h);
void kgsl_late_resume_driver(struct early_suspend *h);

#ifdef CONFIG_MSM_KGSL_DRM
extern int kgsl_drm_init(struct platform_device *dev);
extern void kgsl_drm_exit(void);
extern void kgsl_gpu_mem_flush(int op);
#else
static inline int kgsl_drm_init(struct platform_device *dev)
{
	return 0;
}

static inline void kgsl_drm_exit(void)
{
}
#endif

static inline int kgsl_gpuaddr_in_memdesc(const struct kgsl_memdesc *memdesc,
				unsigned int gpuaddr, unsigned int size)
{
	if (gpuaddr >= memdesc->gpuaddr &&
	    ((gpuaddr + size) <= (memdesc->gpuaddr + memdesc->size))) {
		return 1;
	}
	return 0;
}
static inline uint8_t *kgsl_gpuaddr_to_vaddr(const struct kgsl_memdesc *memdesc,
					     unsigned int gpuaddr)
{
	if (memdesc->hostptr == NULL || memdesc->gpuaddr == 0 ||
		(gpuaddr < memdesc->gpuaddr ||
		gpuaddr >= memdesc->gpuaddr + memdesc->size))
		return NULL;

	return memdesc->hostptr + (gpuaddr - memdesc->gpuaddr);
}

static inline int timestamp_cmp(unsigned int new, unsigned int old)
{
	int ts_diff = new - old;

	if (ts_diff == 0)
		return 0;

	return ((ts_diff > 0) || (ts_diff < -20000)) ? 1 : -1;
}

static inline void
kgsl_mem_entry_get(struct kgsl_mem_entry *entry)
{
	kref_get(&entry->refcount);
}

static inline void
kgsl_mem_entry_put(struct kgsl_mem_entry *entry)
{
	kref_put(&entry->refcount, kgsl_mem_entry_destroy);
}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#endif /* __KGSL_H */
