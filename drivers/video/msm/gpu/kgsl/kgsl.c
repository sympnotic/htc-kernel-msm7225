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
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
=======
/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/fb.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/android_pmem.h>
#include <linux/vmalloc.h>
<<<<<<< HEAD
#include <asm/cacheflush.h>

#include <asm/atomic.h>

#include "kgsl.h"
#include "kgsl_drawctxt.h"
#include "kgsl_ringbuffer.h"
#include "kgsl_cmdstream.h"
#include "kgsl_log.h"

struct kgsl_file_private {
	struct list_head	list;
	struct list_head	mem_list;
	uint32_t		ctxt_id_mask;
	struct kgsl_pagetable	*pagetable;
	unsigned long		vmalloc_size;
};
=======
#include <linux/pm_runtime.h>
#include <linux/genlock.h>

#include <linux/ashmem.h>
#include <linux/major.h>
#ifdef CONFIG_ION_MSM
#include <linux/ion.h>
#endif
#include <mach/socinfo.h>

#include "kgsl.h"
#include "kgsl_debugfs.h"
#include "kgsl_cffdump.h"
#include "kgsl_log.h"
#include "kgsl_sharedmem.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

static int kgsl_pagetable_count = KGSL_PAGETABLE_COUNT;
static char *ksgl_mmu_type;
module_param_named(ptcount, kgsl_pagetable_count, int, 0);
MODULE_PARM_DESC(kgsl_pagetable_count,
"Minimum number of pagetables for KGSL to allocate at initialization time");
module_param_named(mmutype, ksgl_mmu_type, charp, 0);
MODULE_PARM_DESC(ksgl_mmu_type,
"Type of MMU to be used for graphics. Valid values are 'iommu' or 'gpummu' or 'nommu'");

static struct ion_client *kgsl_ion_client;

/**
 * kgsl_add_event - Add a new timstamp event for the KGSL device
 * @device - KGSL device for the new event
 * @ts - the timestamp to trigger the event on
 * @cb - callback function to call when the timestamp expires
 * @priv - private data for the specific event type
 * @owner - driver instance that owns this event
 *
 * @returns - 0 on success or error code on failure
 */

static int kgsl_add_event(struct kgsl_device *device, u32 ts,
	void (*cb)(struct kgsl_device *, void *, u32), void *priv,
	struct kgsl_device_private *owner)
{
	struct kgsl_event *event;
	struct list_head *n;
	unsigned int cur = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	if (cb == NULL)
		return -EINVAL;

	/* Check to see if the requested timestamp has already fired */

	if (timestamp_cmp(cur, ts) >= 0) {
		cb(device, priv, cur);
		return 0;
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	event->timestamp = ts;
	event->priv = priv;
	event->func = cb;
	event->owner = owner;

	/* Add the event in order to the list */

	for (n = device->events.next ; n != &device->events; n = n->next) {
		struct kgsl_event *e =
			list_entry(n, struct kgsl_event, list);

		if (timestamp_cmp(e->timestamp, ts) > 0) {
			list_add(&event->list, n->prev);
			break;
		}
	}

	if (n == &device->events)
		list_add_tail(&event->list, &device->events);

	queue_work(device->work_queue, &device->ts_expired_ws);
	return 0;
}

/**
 * kgsl_cancel_events - Cancel all events for a process
 * @device - KGSL device for the events to cancel
 * @owner - driver instance that owns the events to cancel
 *
 */
static void kgsl_cancel_events(struct kgsl_device *device,
	struct kgsl_device_private *owner)
{
	struct kgsl_event *event, *event_tmp;
	unsigned int cur = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	list_for_each_entry_safe(event, event_tmp, &device->events, list) {
		if (event->owner != owner)
			continue;
		/*
		 * "cancel" the events by calling their callback.
		 * Currently, events are used for lock and memory
		 * management, so if the process is dying the right
		 * thing to do is release or free.
		 */
		if (event->func)
			event->func(device, event->priv, cur);

		list_del(&event->list);
		kfree(event);
	}
}

static inline struct kgsl_mem_entry *
kgsl_mem_entry_create(void)
{
	struct kgsl_mem_entry *entry = kzalloc(sizeof(*entry), GFP_KERNEL);

	if (!entry)
		KGSL_CORE_ERR("kzalloc(%d) failed\n", sizeof(*entry));
	else
		kref_init(&entry->refcount);

	return entry;
}

void
kgsl_mem_entry_destroy(struct kref *kref)
{
	struct kgsl_mem_entry *entry = container_of(kref,
						    struct kgsl_mem_entry,
						    refcount);

	entry->priv->stats[entry->memtype].cur -= entry->memdesc.size;

	if (entry->memtype != KGSL_MEM_ENTRY_KERNEL)
		kgsl_driver.stats.mapped -= entry->memdesc.size;

	/*
	 * Ion takes care of freeing the sglist for us (how nice </sarcasm>) so
	 * unmap the dma before freeing the sharedmem so kgsl_sharedmem_free
	 * doesn't try to free it again
	 */
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#ifdef CONFIG_ION_MSM
	if (entry->memtype == KGSL_MEM_ENTRY_ION) {
		ion_unmap_dma(kgsl_ion_client, entry->priv_data);
		entry->memdesc.sg = NULL;
	}
#endif

	kgsl_sharedmem_free(&entry->memdesc);

	switch (entry->memtype) {
	case KGSL_MEM_ENTRY_PMEM:
	case KGSL_MEM_ENTRY_ASHMEM:
		if (entry->priv_data)
			fput(entry->priv_data);
		break;
#ifdef CONFIG_ION_MSM
	case KGSL_MEM_ENTRY_ION:
		ion_free(kgsl_ion_client, entry->priv_data);
		break;
#endif
	}

	kfree(entry);
}
EXPORT_SYMBOL(kgsl_mem_entry_destroy);

static
void kgsl_mem_entry_attach_process(struct kgsl_mem_entry *entry,
				   struct kgsl_process_private *process)
{
	spin_lock(&process->mem_lock);
	list_add(&entry->list, &process->mem_list);
	spin_unlock(&process->mem_lock);

	entry->priv = process;
}

<<<<<<< HEAD
#ifdef CONFIG_MSM_KGSL_MMU
static long flush_l1_cache_range(unsigned long addr, int size)
=======
/* Allocate a new context id */

static struct kgsl_context *
kgsl_create_context(struct kgsl_device_private *dev_priv)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	struct page *page;
	pte_t *pte_ptr;
	unsigned long end;

	for (end = addr; end < (addr + size); end += KGSL_PAGESIZE) {
		pte_ptr = kgsl_get_pte_from_vaddr(end);
		if (!pte_ptr)
			return -EINVAL;

		page = pte_page(pte_val(*pte_ptr));
		if (!page) {
			KGSL_DRV_ERR("could not find page for pte\n");
			pte_unmap(pte_ptr);
			return -EINVAL;
		}

<<<<<<< HEAD
		pte_unmap(pte_ptr);
		flush_dcache_page(page);
=======
		ret = idr_get_new(&dev_priv->device->context_idr,
				  context, &id);

		if (ret != -EAGAIN)
			break;
	}

	if (ret) {
		kfree(context);
		return NULL;
	}

	context->id = id;
	context->dev_priv = dev_priv;

	return context;
}

static void
kgsl_destroy_context(struct kgsl_device_private *dev_priv,
		     struct kgsl_context *context)
{
	int id;

	if (context == NULL)
		return;

	/* Fire a bug if the devctxt hasn't been freed */
	BUG_ON(context->devctxt);

	id = context->id;
	kfree(context);

	idr_remove(&dev_priv->device->context_idr, id);
}

static void kgsl_timestamp_expired(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work, struct kgsl_device,
		ts_expired_ws);
	struct kgsl_event *event, *event_tmp;
	uint32_t ts_processed;

	mutex_lock(&device->mutex);

	/* get current EOP timestamp */
	ts_processed = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	/* Process expired events */
	list_for_each_entry_safe(event, event_tmp, &device->events, list) {
		if (timestamp_cmp(ts_processed, event->timestamp) < 0)
			break;

		if (event->func)
			event->func(device, event->priv, ts_processed);

		list_del(&event->list);
		kfree(event);
	}

	mutex_unlock(&device->mutex);
}

static void kgsl_check_idle_locked(struct kgsl_device *device)
{
	if (device->pwrctrl.nap_allowed == true &&
	    device->state == KGSL_STATE_ACTIVE &&
		device->requested_state == KGSL_STATE_NONE) {
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NAP);
		if (kgsl_pwrctrl_sleep(device) != 0)
			mod_timer(&device->idle_timer,
				  jiffies +
				  device->pwrctrl.interval_timeout);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}

	return 0;
}

<<<<<<< HEAD
static long flush_l1_cache_all(struct kgsl_file_private *private)
{
	int result = 0;
	struct kgsl_mem_entry *entry = NULL;

	kgsl_yamato_runpending(&kgsl_driver.yamato_device);
	list_for_each_entry(entry, &private->mem_list, list) {
		if (KGSL_MEMFLAGS_MEM_REQUIRES_FLUSH & entry->memdesc.priv) {
			result =
			    flush_l1_cache_range((unsigned long)entry->
						 memdesc.hostptr,
						 entry->memdesc.size);
			if (result)
				goto done;
		}
	}
done:
	return result;
}
#else
static inline long flush_l1_cache_range(unsigned long addr, int size)
{ return 0; }

static inline long flush_l1_cache_all(struct kgsl_file_private *private)
{ return 0; }
#endif
=======
struct kgsl_device *kgsl_get_device(int dev_idx)
{
	int i;
	struct kgsl_device *ret = NULL;

	mutex_lock(&kgsl_driver.devlock);

	for (i = 0; i < KGSL_DEVICE_MAX; i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->id == dev_idx) {
			ret = kgsl_driver.devp[i];
			break;
		}
	}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	mutex_unlock(&kgsl_driver.devlock);
	return ret;
}
EXPORT_SYMBOL(kgsl_get_device);

<<<<<<< HEAD
/* the hw and clk enable/disable funcs must be either called from softirq or
 * with mutex held */
static void kgsl_clk_enable(void)
=======
static struct kgsl_device *kgsl_get_minor(int minor)
{
	struct kgsl_device *ret = NULL;

	if (minor < 0 || minor >= KGSL_DEVICE_MAX)
		return NULL;

	mutex_lock(&kgsl_driver.devlock);
	ret = kgsl_driver.devp[minor];
	mutex_unlock(&kgsl_driver.devlock);

	return ret;
}

int kgsl_register_ts_notifier(struct kgsl_device *device,
			      struct notifier_block *nb)
{
	BUG_ON(device == NULL);
	return atomic_notifier_chain_register(&device->ts_notifier_list,
					      nb);
}
EXPORT_SYMBOL(kgsl_register_ts_notifier);

int kgsl_unregister_ts_notifier(struct kgsl_device *device,
				struct notifier_block *nb)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	clk_set_rate(kgsl_driver.ebi1_clk, 128000000);
	clk_enable(kgsl_driver.imem_clk);
	if (kgsl_driver.grp_pclk)
		clk_enable(kgsl_driver.grp_pclk);
	clk_enable(kgsl_driver.grp_clk);
}
EXPORT_SYMBOL(kgsl_unregister_ts_notifier);

static void kgsl_clk_disable(void)
{
<<<<<<< HEAD
	clk_disable(kgsl_driver.grp_clk);
	if (kgsl_driver.grp_pclk)
		clk_disable(kgsl_driver.grp_pclk);
	clk_disable(kgsl_driver.imem_clk);
	clk_set_rate(kgsl_driver.ebi1_clk, 0);
=======
	unsigned int ts_processed;

	ts_processed = device->ftbl->readtimestamp(device,
		KGSL_TIMESTAMP_RETIRED);

	return (timestamp_cmp(ts_processed, timestamp) >= 0);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
EXPORT_SYMBOL(kgsl_check_timestamp);

<<<<<<< HEAD
static void kgsl_hw_disable(void)
{
	kgsl_driver.active = false;
	disable_irq(kgsl_driver.interrupt_num);
	kgsl_clk_disable();
	pr_debug("kgsl: hw disabled\n");
	wake_unlock(&kgsl_driver.wake_lock);
}

static void kgsl_hw_enable(void)
{
	wake_lock(&kgsl_driver.wake_lock);
	kgsl_clk_enable();
	enable_irq(kgsl_driver.interrupt_num);
	kgsl_driver.active = true;
	pr_debug("kgsl: hw enabled\n");
}

static void kgsl_hw_get_locked(void)
{
	/* active_cnt is protected by driver mutex */
	if (kgsl_driver.active_cnt++ == 0) {
		if (kgsl_driver.active) {
			del_timer_sync(&kgsl_driver.standby_timer);
			barrier();
		}
		if (!kgsl_driver.active)
			kgsl_hw_enable();
	}
}

static void kgsl_hw_put_locked(bool start_timer)
{
	if ((--kgsl_driver.active_cnt == 0) && start_timer) {
		mod_timer(&kgsl_driver.standby_timer,
			  jiffies + msecs_to_jiffies(20));
	}
=======
static int kgsl_suspend_device(struct kgsl_device *device, pm_message_t state)
{
	int status = -EINVAL;
	unsigned int nap_allowed_saved;
	struct kgsl_pwrscale_policy *policy_saved;

	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "suspend start\n");

	mutex_lock(&device->mutex);
	nap_allowed_saved = device->pwrctrl.nap_allowed;
	device->pwrctrl.nap_allowed = false;
	policy_saved = device->pwrscale.policy;
	device->pwrscale.policy = NULL;
	kgsl_pwrctrl_request_state(device, KGSL_STATE_SUSPEND);
	/* Make sure no user process is waiting for a timestamp *
	 * before supending */
	if (device->active_cnt != 0) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->suspend_gate);
		mutex_lock(&device->mutex);
	}
	switch (device->state) {
		case KGSL_STATE_INIT:
			break;
		case KGSL_STATE_ACTIVE:
			/* Wait for the device to become idle */
			device->ftbl->idle(device, KGSL_TIMEOUT_DEFAULT);
		case KGSL_STATE_NAP:
		case KGSL_STATE_SLEEP:
			/* Get the completion ready to be waited upon. */
			INIT_COMPLETION(device->hwaccess_gate);
			device->ftbl->suspend_context(device);
			kgsl_pwrctrl_stop_work(device);
			device->ftbl->stop(device);
			kgsl_pwrctrl_set_state(device, KGSL_STATE_SUSPEND);
			break;
		case KGSL_STATE_SLUMBER:
			INIT_COMPLETION(device->hwaccess_gate);
			kgsl_pwrctrl_set_state(device, KGSL_STATE_SUSPEND);
			break;
		default:
			KGSL_PWR_ERR(device, "suspend fail, device %d\n",
					device->id);
			goto end;
	}
	kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
	device->pwrctrl.nap_allowed = nap_allowed_saved;
	device->pwrscale.policy = policy_saved;
	status = 0;

end:
	mutex_unlock(&device->mutex);
	KGSL_PWR_WARN(device, "suspend end\n");
	return status;
}

static int kgsl_resume_device(struct kgsl_device *device)
{
	int status = -EINVAL;

	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "resume start\n");
	mutex_lock(&device->mutex);
	if (device->state == KGSL_STATE_SUSPEND) {
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		status = 0;
		complete_all(&device->hwaccess_gate);
	}
	kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);

	mutex_unlock(&device->mutex);
	KGSL_PWR_WARN(device, "resume end\n");
	return status;
}

static int kgsl_suspend(struct device *dev)
{

	pm_message_t arg = {0};
	struct kgsl_device *device = dev_get_drvdata(dev);
	return kgsl_suspend_device(device, arg);
}

static int kgsl_resume(struct device *dev)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	return kgsl_resume_device(device);
}

static int kgsl_runtime_suspend(struct device *dev)
{
	return 0;
}

static int kgsl_runtime_resume(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops kgsl_pm_ops = {
	.suspend = kgsl_suspend,
	.resume = kgsl_resume,
	.runtime_suspend = kgsl_runtime_suspend,
	.runtime_resume = kgsl_runtime_resume,
};
EXPORT_SYMBOL(kgsl_pm_ops);

void kgsl_early_suspend_driver(struct early_suspend *h)
{
	struct kgsl_device *device = container_of(h,
					struct kgsl_device, display_off);
	KGSL_PWR_WARN(device, "early suspend start\n");
	mutex_lock(&device->mutex);
	kgsl_pwrctrl_stop_work(device);
	kgsl_pwrctrl_request_state(device, KGSL_STATE_SLUMBER);
	kgsl_pwrctrl_sleep(device);
	mutex_unlock(&device->mutex);
	KGSL_PWR_WARN(device, "early suspend end\n");
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
EXPORT_SYMBOL(kgsl_early_suspend_driver);

<<<<<<< HEAD
static void kgsl_do_standby_timer(unsigned long data)
{
	if (kgsl_yamato_is_idle(&kgsl_driver.yamato_device))
		kgsl_hw_disable();
	else
		mod_timer(&kgsl_driver.standby_timer,
			  jiffies + msecs_to_jiffies(10));
=======
int kgsl_suspend_driver(struct platform_device *pdev,
					pm_message_t state)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);
	return kgsl_suspend_device(device, state);
}
EXPORT_SYMBOL(kgsl_suspend_driver);

int kgsl_resume_driver(struct platform_device *pdev)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);
	return kgsl_resume_device(device);
}
EXPORT_SYMBOL(kgsl_resume_driver);

void kgsl_late_resume_driver(struct early_suspend *h)
{
	struct kgsl_device *device = container_of(h,
					struct kgsl_device, display_off);
	KGSL_PWR_WARN(device, "late resume start\n");
	mutex_lock(&device->mutex);
	kgsl_pwrctrl_wake(device);
	device->pwrctrl.restore_slumber = 0;
	kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_TURBO);
	mutex_unlock(&device->mutex);
	kgsl_check_idle(device);
	KGSL_PWR_WARN(device, "late resume end\n");
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
EXPORT_SYMBOL(kgsl_late_resume_driver);

/* file operations */
static int kgsl_first_open_locked(void)
{
	int result = 0;

<<<<<<< HEAD
	BUG_ON(kgsl_driver.active);
	BUG_ON(kgsl_driver.active_cnt);
=======
	/* no existing process private found for this dev_priv, create one */
	private = kzalloc(sizeof(struct kgsl_process_private), GFP_KERNEL);
	if (private == NULL) {
		KGSL_DRV_ERR(cur_dev_priv->device, "kzalloc(%d) failed\n",
			sizeof(struct kgsl_process_private));
		goto out;
	}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	kgsl_clk_enable();

	/* init memory apertures */
	result = kgsl_sharedmem_init(&kgsl_driver.shmem);
	if (result != 0)
		goto done;

<<<<<<< HEAD
	/* init devices */
	result = kgsl_yamato_init(&kgsl_driver.yamato_device,
					&kgsl_driver.yamato_config);
	if (result != 0)
		goto done;

	result = kgsl_yamato_start(&kgsl_driver.yamato_device, 0);
	if (result != 0)
		goto done;

done:
	kgsl_clk_disable();
	return result;
=======
	if (kgsl_mmu_enabled())
	{
		unsigned long pt_name;

		pt_name = task_tgid_nr(current);
		private->pagetable = kgsl_mmu_getpagetable(pt_name);
		if (private->pagetable == NULL) {
			kfree(private);
			private = NULL;
			goto out;
		}
	}

	list_add(&private->list, &kgsl_driver.process_list);

	kgsl_process_init_sysfs(private);

out:
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}

static int kgsl_last_release_locked(void)
{
	BUG_ON(kgsl_driver.active_cnt);

<<<<<<< HEAD
	disable_irq(kgsl_driver.interrupt_num);
=======
	if (!private)
		return;

	mutex_lock(&kgsl_driver.process_mutex);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	kgsl_yamato_stop(&kgsl_driver.yamato_device);

<<<<<<< HEAD
	/* close devices */
	kgsl_yamato_close(&kgsl_driver.yamato_device);

	/* shutdown memory apertures */
	kgsl_sharedmem_close(&kgsl_driver.shmem);

	kgsl_clk_disable();
	kgsl_driver.active = false;
	wake_unlock(&kgsl_driver.wake_lock);

	return 0;
=======
	kgsl_process_uninit_sysfs(private);

	list_del(&private->list);

	list_for_each_entry_safe(entry, entry_tmp, &private->mem_list, list) {
		list_del(&entry->list);
		kgsl_mem_entry_put(entry);
	}

	kgsl_mmu_putpagetable(private->pagetable);
	kfree(private);
unlock:
	mutex_unlock(&kgsl_driver.process_mutex);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}

static int kgsl_release(struct inode *inodep, struct file *filep)
{
	int result = 0;
<<<<<<< HEAD
	unsigned int i;
	struct kgsl_mem_entry *entry, *entry_tmp;
	struct kgsl_file_private *private = NULL;

	mutex_lock(&kgsl_driver.mutex);

	private = filep->private_data;
	BUG_ON(private == NULL);
=======
	struct kgsl_device_private *dev_priv = filep->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	int next = 0;

>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	filep->private_data = NULL;
	list_del(&private->list);

<<<<<<< HEAD
	kgsl_hw_get_locked();

	for (i = 0; i < KGSL_CONTEXT_MAX; i++)
		if (private->ctxt_id_mask & (1 << i))
			kgsl_drawctxt_destroy(&kgsl_driver.yamato_device, i);
=======
	mutex_lock(&device->mutex);
	kgsl_check_suspended(device);

	while (1) {
		context = idr_get_next(&device->context_idr, &next);
		if (context == NULL)
			break;

		if (context->dev_priv == dev_priv) {
			device->ftbl->drawctxt_destroy(device, context);
			kgsl_destroy_context(dev_priv, context);
		}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	list_for_each_entry_safe(entry, entry_tmp, &private->mem_list, list)
		kgsl_remove_mem_entry(entry);

<<<<<<< HEAD
	if (private->pagetable != NULL) {
		kgsl_yamato_cleanup_pt(&kgsl_driver.yamato_device,
					private->pagetable);
		kgsl_mmu_destroypagetableobject(private->pagetable);
		private->pagetable = NULL;
	}
=======
	device->open_count--;
	if (device->open_count == 0) {
		kgsl_pwrctrl_stop_work(device);
		result = device->ftbl->stop(device);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_INIT);
	}
	/* clean up any to-be-freed entries that belong to this
	 * process and this device
	 */
	kgsl_cancel_events(device, dev_priv);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	kfree(private);

	if (atomic_dec_return(&kgsl_driver.open_count) == 0) {
		KGSL_DRV_VDBG("last_release\n");
		kgsl_hw_put_locked(false);
		result = kgsl_last_release_locked();
	} else
		kgsl_hw_put_locked(true);

<<<<<<< HEAD
	mutex_unlock(&kgsl_driver.mutex);

=======
	pm_runtime_put(device->parentdev);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	return result;
}

static int kgsl_open(struct inode *inodep, struct file *filep)
{
<<<<<<< HEAD
	int result = 0;
	struct kgsl_file_private *private = NULL;

	KGSL_DRV_DBG("file %p pid %d\n", filep, task_pid_nr(current));


	if (filep->f_flags & O_EXCL) {
		KGSL_DRV_ERR("O_EXCL not allowed\n");
		return -EBUSY;
	}

	private = kzalloc(sizeof(*private), GFP_KERNEL);
	if (private == NULL) {
		KGSL_DRV_ERR("cannot allocate file private data\n");
		return -ENOMEM;
=======
	int result;
	struct kgsl_device_private *dev_priv;
	struct kgsl_device *device;
	unsigned int minor = iminor(inodep);

	device = kgsl_get_minor(minor);
	BUG_ON(device == NULL);

	if (filep->f_flags & O_EXCL) {
		KGSL_DRV_ERR(device, "O_EXCL not allowed\n");
		return -EBUSY;
	}

	result = pm_runtime_get_sync(device->parentdev);
	if (result < 0) {
		KGSL_DRV_ERR(device,
			"Runtime PM: Unable to wake up the device, rc = %d\n",
			result);
		return result;
	}
	result = 0;

	dev_priv = kzalloc(sizeof(struct kgsl_device_private), GFP_KERNEL);
	if (dev_priv == NULL) {
		KGSL_DRV_ERR(device, "kzalloc failed(%d)\n",
			sizeof(struct kgsl_device_private));
		result = -ENOMEM;
		goto err_pmruntime;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}

	mutex_lock(&kgsl_driver.mutex);

<<<<<<< HEAD
	private->ctxt_id_mask = 0;
	INIT_LIST_HEAD(&private->mem_list);

	filep->private_data = private;
=======
	/* Get file (per process) private struct */
	dev_priv->process_priv = kgsl_get_process_private(dev_priv);
	if (dev_priv->process_priv ==  NULL) {
		result = -ENOMEM;
		goto err_freedevpriv;
	}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	list_add(&private->list, &kgsl_driver.client_list);

<<<<<<< HEAD
	if (atomic_inc_return(&kgsl_driver.open_count) == 1) {
		result = kgsl_first_open_locked();
		if (result != 0)
			goto done;
	}

	kgsl_hw_get_locked();

	/*NOTE: this must happen after first_open */
	private->pagetable =
		kgsl_mmu_createpagetableobject(&kgsl_driver.yamato_device.mmu);
	if (private->pagetable == NULL) {
		result = -ENOMEM;
		goto done;
	}
	result = kgsl_yamato_setup_pt(&kgsl_driver.yamato_device,
					private->pagetable);
	if (result) {
		kgsl_mmu_destroypagetableobject(private->pagetable);
		private->pagetable = NULL;
		goto done;
	}
	private->vmalloc_size = 0;
done:
	kgsl_hw_put_locked(true);
	mutex_unlock(&kgsl_driver.mutex);
	if (result != 0)
		kgsl_release(inodep, filep);
=======
	if (device->open_count == 0) {
		result = device->ftbl->start(device, true);

		if (result) {
			mutex_unlock(&device->mutex);
			goto err_putprocess;
		}
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
	}
	device->open_count++;
	mutex_unlock(&device->mutex);

	KGSL_DRV_INFO(device, "Initialized %s: mmu=%s pagetable_count=%d\n",
		device->name, kgsl_mmu_enabled() ? "on" : "off",
		kgsl_pagetable_count);

	return result;

err_putprocess:
	kgsl_put_process_private(device, dev_priv->process_priv);
err_freedevpriv:
	filep->private_data = NULL;
	kfree(dev_priv);
err_pmruntime:
	pm_runtime_put(device->parentdev);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	return result;
}


/*call with driver locked */
static struct kgsl_mem_entry *
kgsl_sharedmem_find(struct kgsl_file_private *private, unsigned int gpuaddr)
{
	struct kgsl_mem_entry *entry = NULL, *result = NULL;

	BUG_ON(private == NULL);

	gpuaddr &= PAGE_MASK;

	list_for_each_entry(entry, &private->mem_list, list) {
		if (entry->memdesc.gpuaddr == gpuaddr) {
			result = entry;
			break;
		}
	}
	return result;
}

/*call with driver locked */
struct kgsl_mem_entry *
kgsl_sharedmem_find_region(struct kgsl_file_private *private,
				unsigned int gpuaddr,
				size_t size)
{
	struct kgsl_mem_entry *entry = NULL, *result = NULL;

	BUG_ON(private == NULL);

	list_for_each_entry(entry, &private->mem_list, list) {
		if (kgsl_gpuaddr_in_memdesc(&entry->memdesc, gpuaddr, size)) {
			result = entry;
			break;
		}
	}

	return result;
}
EXPORT_SYMBOL(kgsl_sharedmem_find_region);

/*call all ioctl sub functions with driver locked*/
<<<<<<< HEAD

static long kgsl_ioctl_device_getproperty(struct kgsl_file_private *private,
					 void __user *arg)
{
	int result = 0;
	struct kgsl_device_getproperty param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}
	result = kgsl_yamato_getproperty(&kgsl_driver.yamato_device,
					 param.type,
					 param.value, param.sizebytes);
done:
	return result;
}

static long kgsl_ioctl_device_regread(struct kgsl_file_private *private,
				     void __user *arg)
{
	int result = 0;
	struct kgsl_device_regread param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}
	result = kgsl_yamato_regread(&kgsl_driver.yamato_device,
				     param.offsetwords, &param.value);
	if (result != 0)
		goto done;
=======
static long kgsl_ioctl_device_getproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_device_getproperty *param = data;

	switch (param->type) {
	case KGSL_PROP_VERSION:
	{
		struct kgsl_version version;
		if (param->sizebytes != sizeof(version)) {
			result = -EINVAL;
			break;
		}

		version.drv_major = KGSL_VERSION_MAJOR;
		version.drv_minor = KGSL_VERSION_MINOR;
		version.dev_major = dev_priv->device->ver_major;
		version.dev_minor = dev_priv->device->ver_minor;

		if (copy_to_user(param->value, &version, sizeof(version)))
			result = -EFAULT;

		break;
	}
	default:
		result = dev_priv->device->ftbl->getproperty(
					dev_priv->device, param->type,
					param->value, param->sizebytes);
	}


	return result;
}

static long kgsl_ioctl_device_waittimestamp(struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
{
	int result = 0;
	struct kgsl_device_waittimestamp *param = data;

	/* Set the active count so that suspend doesn't do the
	   wrong thing */

	dev_priv->device->active_cnt++;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	trace_kgsl_waittimestamp_entry(dev_priv->device, param);

	result = dev_priv->device->ftbl->waittimestamp(dev_priv->device,
					param->timestamp,
					param->timeout);

<<<<<<< HEAD
static long kgsl_ioctl_device_waittimestamp(struct kgsl_file_private *private,
				     void __user *arg)
{
	int result = 0;
	struct kgsl_device_waittimestamp param;
=======
	trace_kgsl_waittimestamp_exit(dev_priv->device, result);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	/* Fire off any pending suspend operations that are in flight */

<<<<<<< HEAD
	/* Don't wait forever, set a max value for now */
	if (param.timeout == -1)
		param.timeout = 10 * MSEC_PER_SEC;
	result = kgsl_yamato_waittimestamp(&kgsl_driver.yamato_device,
				     param.timestamp,
				     param.timeout);

	kgsl_yamato_runpending(&kgsl_driver.yamato_device);
done:
	return result;
}

static long kgsl_ioctl_rb_issueibcmds(struct kgsl_file_private *private,
				     void __user *arg)
{
	int result = 0;
	struct kgsl_ringbuffer_issueibcmds param;
=======
	INIT_COMPLETION(dev_priv->device->suspend_gate);
	dev_priv->device->active_cnt--;
	complete(&dev_priv->device->suspend_gate);

	return result;
}
static bool check_ibdesc(struct kgsl_device_private *dev_priv,
			 struct kgsl_ibdesc *ibdesc, unsigned int numibs,
			 bool parse)
{
	bool result = true;
	unsigned int i;
	for (i = 0; i < numibs; i++) {
		struct kgsl_mem_entry *entry;
		spin_lock(&dev_priv->process_priv->mem_lock);
		entry = kgsl_sharedmem_find_region(dev_priv->process_priv,
			ibdesc[i].gpuaddr, ibdesc[i].sizedwords * sizeof(uint));
		spin_unlock(&dev_priv->process_priv->mem_lock);
		if (entry == NULL) {
			KGSL_DRV_ERR(dev_priv->device,
				"invalid cmd buffer gpuaddr %08x " \
				"sizedwords %d\n", ibdesc[i].gpuaddr,
				ibdesc[i].sizedwords);
			result = false;
			break;
		}

		if (parse && !kgsl_cffdump_parse_ibs(dev_priv, &entry->memdesc,
			ibdesc[i].gpuaddr, ibdesc[i].sizedwords, true)) {
			KGSL_DRV_ERR(dev_priv->device,
				"invalid cmd buffer gpuaddr %08x " \
				"sizedwords %d numibs %d/%d\n",
				ibdesc[i].gpuaddr,
				ibdesc[i].sizedwords, i+1, numibs);
			result = false;
			break;
		}
	}
	return result;
}

static long kgsl_ioctl_rb_issueibcmds(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_ringbuffer_issueibcmds *param = data;
	struct kgsl_ibdesc *ibdesc;
	struct kgsl_context *context;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#ifdef CONFIG_MSM_KGSL_DRM
	kgsl_gpu_mem_flush(DRM_KGSL_GEM_CACHE_OP_TO_DEV);
#endif

<<<<<<< HEAD
	if (param.drawctxt_id >= KGSL_CONTEXT_MAX
		|| (private->ctxt_id_mask & 1 << param.drawctxt_id) == 0) {
		result = -EINVAL;
		KGSL_DRV_ERR("invalid drawctxt drawctxt_id %d\n",
			      param.drawctxt_id);
		result = -EINVAL;
		goto done;
	}

	if (kgsl_sharedmem_find_region(private, param.ibaddr,
				param.sizedwords*sizeof(uint32_t)) == NULL) {
		KGSL_DRV_ERR("invalid cmd buffer ibaddr %08x sizedwords %d\n",
			      param.ibaddr, param.sizedwords);
=======
	context = kgsl_find_context(dev_priv, param->drawctxt_id);
	if (context == NULL) {
		result = -EINVAL;
		KGSL_DRV_ERR(dev_priv->device,
			"invalid drawctxt drawctxt_id %d\n",
			param->drawctxt_id);
		goto done;
	}

	if (param->flags & KGSL_CONTEXT_SUBMIT_IB_LIST) {
		KGSL_DRV_INFO(dev_priv->device,
			"Using IB list mode for ib submission, numibs: %d\n",
			param->numibs);
		if (!param->numibs) {
			KGSL_DRV_ERR(dev_priv->device,
				"Invalid numibs as parameter: %d\n",
				 param->numibs);
			result = -EINVAL;
			goto done;
		}

		ibdesc = kzalloc(sizeof(struct kgsl_ibdesc) * param->numibs,
					GFP_KERNEL);
		if (!ibdesc) {
			KGSL_MEM_ERR(dev_priv->device,
				"kzalloc(%d) failed\n",
				sizeof(struct kgsl_ibdesc) * param->numibs);
			result = -ENOMEM;
			goto done;
		}

		if (copy_from_user(ibdesc, (void *)param->ibdesc_addr,
				sizeof(struct kgsl_ibdesc) * param->numibs)) {
			result = -EFAULT;
			KGSL_DRV_ERR(dev_priv->device,
				"copy_from_user failed\n");
			goto free_ibdesc;
		}
	} else {
		KGSL_DRV_INFO(dev_priv->device,
			"Using single IB submission mode for ib submission\n");
		/* If user space driver is still using the old mode of
		 * submitting single ib then we need to support that as well */
		ibdesc = kzalloc(sizeof(struct kgsl_ibdesc), GFP_KERNEL);
		if (!ibdesc) {
			KGSL_MEM_ERR(dev_priv->device,
				"kzalloc(%d) failed\n",
				sizeof(struct kgsl_ibdesc));
			result = -ENOMEM;
			goto done;
		}
		ibdesc[0].gpuaddr = param->ibdesc_addr;
		ibdesc[0].sizedwords = param->numibs;
		param->numibs = 1;
	}

	if (!check_ibdesc(dev_priv, ibdesc, param->numibs, true)) {
		KGSL_DRV_ERR(dev_priv->device, "bad ibdesc");
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
		result = -EINVAL;
		goto done;

	}

<<<<<<< HEAD
	result = kgsl_ringbuffer_issueibcmds(&kgsl_driver.yamato_device,
					     param.drawctxt_id,
					     param.ibaddr,
					     param.sizedwords,
					     &param.timestamp,
					     param.flags);
	if (result != 0)
		goto done;

	if (copy_to_user(arg, &param, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}
=======
	result = dev_priv->device->ftbl->issueibcmds(dev_priv,
					     context,
					     ibdesc,
					     param->numibs,
					     &param->timestamp,
					     param->flags);

	trace_kgsl_issueibcmds(dev_priv->device, param, result);

	if (result != 0)
		goto free_ibdesc;

	/* this is a check to try to detect if a command buffer was freed
	 * during issueibcmds().
	 */
	if (!check_ibdesc(dev_priv, ibdesc, param->numibs, false)) {
		KGSL_DRV_ERR(dev_priv->device, "bad ibdesc AFTER issue");
		result = -EINVAL;
		goto free_ibdesc;
	}

free_ibdesc:
	kfree(ibdesc);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
done:

#ifdef CONFIG_MSM_KGSL_DRM
	kgsl_gpu_mem_flush(DRM_KGSL_GEM_CACHE_OP_FROM_DEV);
#endif

	return result;
}

<<<<<<< HEAD
static long kgsl_ioctl_cmdstream_readtimestamp(struct kgsl_file_private
						*private, void __user *arg)
=======
static long kgsl_ioctl_cmdstream_readtimestamp(struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	struct kgsl_cmdstream_readtimestamp *param = data;

	param->timestamp =
		dev_priv->device->ftbl->readtimestamp(dev_priv->device,
		param->type);

<<<<<<< HEAD
	param.timestamp =
		kgsl_cmdstream_readtimestamp(&kgsl_driver.yamato_device,
							param.type);
	if (result != 0)
		goto done;
=======
	trace_kgsl_readtimestamp(dev_priv->device, param);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	return 0;
}

static void kgsl_freemem_event_cb(struct kgsl_device *device,
	void *priv, u32 timestamp)
{
	struct kgsl_mem_entry *entry = priv;
	spin_lock(&entry->priv->mem_lock);
	list_del(&entry->list);
	spin_unlock(&entry->priv->mem_lock);
	kgsl_mem_entry_put(entry);
}

<<<<<<< HEAD
static long kgsl_ioctl_cmdstream_freememontimestamp(struct kgsl_file_private
						*private, void __user *arg)
=======
static long kgsl_ioctl_cmdstream_freememontimestamp(struct kgsl_device_private
						    *dev_priv, unsigned int cmd,
						    void *data)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	int result = 0;
	struct kgsl_cmdstream_freememontimestamp *param = data;
	struct kgsl_mem_entry *entry = NULL;

<<<<<<< HEAD
	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}

	entry = kgsl_sharedmem_find(private, param.gpuaddr);
	if (entry == NULL) {
		KGSL_DRV_ERR("invalid gpuaddr %08x\n", param.gpuaddr);
=======
	spin_lock(&dev_priv->process_priv->mem_lock);
	entry = kgsl_sharedmem_find(dev_priv->process_priv, param->gpuaddr);
	spin_unlock(&dev_priv->process_priv->mem_lock);

	if (entry) {
		result = kgsl_add_event(dev_priv->device, param->timestamp,
					kgsl_freemem_event_cb, entry, dev_priv);
	} else {
		KGSL_DRV_ERR(dev_priv->device,
			"invalid gpuaddr %08x\n", param->gpuaddr);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
		result = -EINVAL;
		goto done;
	}

<<<<<<< HEAD
	if (entry->memdesc.priv & KGSL_MEMFLAGS_VMALLOC_MEM)
		entry->memdesc.priv &= ~KGSL_MEMFLAGS_MEM_REQUIRES_FLUSH;

	result = kgsl_cmdstream_freememontimestamp(&kgsl_driver.yamato_device,
							entry,
							param.timestamp,
							param.type);

	kgsl_yamato_runpending(&kgsl_driver.yamato_device);

done:
	return result;
}

static long kgsl_ioctl_drawctxt_create(struct kgsl_file_private *private,
				      void __user *arg)
{
	int result = 0;
	struct kgsl_drawctxt_create param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}

	result = kgsl_drawctxt_create(&kgsl_driver.yamato_device,
					private->pagetable,
					param.flags,
					&param.drawctxt_id);
	if (result != 0)
		goto done;

	if (copy_to_user(arg, &param, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}
=======
	return result;
}

static long kgsl_ioctl_drawctxt_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_create *param = data;
	struct kgsl_context *context = NULL;

	context = kgsl_create_context(dev_priv);

	if (context == NULL) {
		result = -ENOMEM;
		goto done;
	}

	if (dev_priv->device->ftbl->drawctxt_create)
		result = dev_priv->device->ftbl->drawctxt_create(
			dev_priv->device, dev_priv->process_priv->pagetable,
			context, param->flags);

	param->drawctxt_id = context->id;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	private->ctxt_id_mask |= 1 << param.drawctxt_id;

done:
	return result;
}

<<<<<<< HEAD
static long kgsl_ioctl_drawctxt_destroy(struct kgsl_file_private *private,
				       void __user *arg)
{
	int result = 0;
	struct kgsl_drawctxt_destroy param;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}

	if (param.drawctxt_id >= KGSL_CONTEXT_MAX
		|| (private->ctxt_id_mask & 1 << param.drawctxt_id) == 0) {
=======
static long kgsl_ioctl_drawctxt_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_destroy *param = data;
	struct kgsl_context *context;

	context = kgsl_find_context(dev_priv, param->drawctxt_id);

	if (context == NULL) {
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
		result = -EINVAL;
		goto done;
	}

<<<<<<< HEAD
	result = kgsl_drawctxt_destroy(&kgsl_driver.yamato_device,
					param.drawctxt_id);
	if (result == 0)
		private->ctxt_id_mask &= ~(1 << param.drawctxt_id);
=======
	if (dev_priv->device->ftbl->drawctxt_destroy)
		dev_priv->device->ftbl->drawctxt_destroy(dev_priv->device,
			context);

	kgsl_destroy_context(dev_priv, context);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

done:
	return result;
}

<<<<<<< HEAD
void kgsl_remove_mem_entry(struct kgsl_mem_entry *entry)
{
	kgsl_mmu_unmap(entry->memdesc.pagetable,
		       entry->memdesc.gpuaddr & KGSL_PAGEMASK,
		       entry->memdesc.size);
	if (KGSL_MEMFLAGS_VMALLOC_MEM & entry->memdesc.priv) {
		vfree((void *)entry->memdesc.physaddr);
		entry->priv->vmalloc_size -= entry->memdesc.size;
	} else
		kgsl_put_phys_file(entry->pmem_file);
	list_del(&entry->list);

	if (entry->free_list.prev)
		list_del(&entry->free_list);

	kfree(entry);

}

static long kgsl_ioctl_sharedmem_free(struct kgsl_file_private *private,
				     void __user *arg)
=======
static long kgsl_ioctl_sharedmem_free(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
{
	int result = 0;
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;

<<<<<<< HEAD
	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto done;
	}

	entry = kgsl_sharedmem_find(private, param.gpuaddr);
	if (entry == NULL) {
		KGSL_DRV_ERR("invalid gpuaddr %08x\n", param.gpuaddr);
=======
	spin_lock(&private->mem_lock);
	entry = kgsl_sharedmem_find(private, param->gpuaddr);
	if (entry)
		list_del(&entry->list);
	spin_unlock(&private->mem_lock);

	if (entry) {
		kgsl_mem_entry_put(entry);
	} else {
		KGSL_CORE_ERR("invalid gpuaddr %08x\n", param->gpuaddr);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
		result = -EINVAL;
		goto done;
	}

<<<<<<< HEAD
	kgsl_remove_mem_entry(entry);
done:
	return result;
}

#ifdef CONFIG_MSM_KGSL_MMU
static int kgsl_ioctl_sharedmem_from_vmalloc(struct kgsl_file_private *private,
					     void __user *arg)
{
	int result = 0, len;
	struct kgsl_sharedmem_from_vmalloc param;
=======
	return result;
}

static struct vm_area_struct *kgsl_get_vma_from_start_addr(unsigned int addr)
{
	struct vm_area_struct *vma;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, addr);
	up_read(&current->mm->mmap_sem);
	if (!vma)
		KGSL_CORE_ERR("find_vma(%x) failed\n", addr);

	return vma;
}

static long
kgsl_ioctl_sharedmem_from_vmalloc(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data)
{
	int result = 0, len = 0;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_sharedmem_from_vmalloc *param = data;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	struct kgsl_mem_entry *entry = NULL;
	struct vm_area_struct *vma;

	if (!kgsl_mmu_enabled())
		return -ENODEV;

	if (!param->hostptr) {
		KGSL_CORE_ERR("invalid hostptr %x\n", param->hostptr);
		result = -EINVAL;
		goto error;
	}

<<<<<<< HEAD
	vma = find_vma(current->mm, param.hostptr);
=======
	vma = kgsl_get_vma_from_start_addr(param->hostptr);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	if (!vma) {
		KGSL_MEM_ERR("Could not find vma for address %x\n",
			     param.hostptr);
		result = -EINVAL;
		goto error;
	}
<<<<<<< HEAD
	len = vma->vm_end - vma->vm_start;
	if (vma->vm_pgoff || !IS_ALIGNED(len, PAGE_SIZE)
	    || !IS_ALIGNED(vma->vm_start, PAGE_SIZE)) {
		KGSL_MEM_ERR
		("kgsl vmalloc mapping must be at offset 0 and page aligned\n");
		result = -EINVAL;
		goto error;
	}
	if (vma->vm_start != param.hostptr) {
		KGSL_MEM_ERR
		    ("vma start address is not equal to mmap address\n");
		result = -EINVAL;
		goto error;
	}

	if ((private->vmalloc_size + len) > KGSL_GRAPHICS_MEMORY_LOW_WATERMARK
	    && !param.force_no_low_watermark) {
		result = -ENOMEM;
		goto error;
	}

	entry = kzalloc(sizeof(struct kgsl_mem_entry), GFP_KERNEL);
	if (entry == NULL) {
		result = -ENOMEM;
=======

	/*
	 * If the user specified a length, use it, otherwise try to
	 * infer the length if the vma region
	 */
	if (param->gpuaddr != 0) {
		len = param->gpuaddr;
	} else {
		/*
		 * For this to work, we have to assume the VMA region is only
		 * for this single allocation.  If it isn't, then bail out
		 */
		if (vma->vm_pgoff || (param->hostptr != vma->vm_start)) {
			KGSL_CORE_ERR("VMA region does not match hostaddr\n");
			result = -EINVAL;
			goto error;
		}

		len = vma->vm_end - vma->vm_start;
	}

	/* Make sure it fits */
	if (len == 0 || param->hostptr + len > vma->vm_end) {
		KGSL_CORE_ERR("Invalid memory allocation length %d\n", len);
		result = -EINVAL;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
		goto error;
	}

	entry = kgsl_mem_entry_create();
	if (entry == NULL) {
		result = -ENOMEM;
		goto error;
	}
<<<<<<< HEAD
	if (!kgsl_cache_enable) {
		/* If we are going to map non-cached, make sure to flush the
		 * cache to ensure that previously cached data does not
		 * overwrite this memory */
		dmac_flush_range(vmalloc_area, vmalloc_area + len);
		KGSL_MEM_INFO("Caching for memory allocation turned off\n");
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	} else {
		KGSL_MEM_INFO("Caching for memory allocation turned on\n");
	}
=======

	result = kgsl_sharedmem_vmalloc_user(&entry->memdesc,
					     private->pagetable, len,
					     param->flags);
	if (result != 0)
		goto error_free_entry;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	result = remap_vmalloc_range(vma, (void *) entry->memdesc.hostptr, 0);
	if (result) {
<<<<<<< HEAD
		KGSL_MEM_ERR("remap_vmalloc_range returned %d\n", result);
		goto error_free_vmalloc;
	}

	result =
	    kgsl_mmu_map(private->pagetable, (unsigned long)vmalloc_area, len,
			 GSL_PT_PAGE_RV | GSL_PT_PAGE_WV,
			 &entry->memdesc.gpuaddr, KGSL_MEMFLAGS_ALIGN4K);

	if (result != 0)
		goto error_free_vmalloc;

	entry->memdesc.pagetable = private->pagetable;
	entry->memdesc.size = len;
	entry->memdesc.hostptr = (void *)param.hostptr;
	entry->memdesc.priv = KGSL_MEMFLAGS_VMALLOC_MEM |
	    KGSL_MEMFLAGS_MEM_REQUIRES_FLUSH;
	entry->memdesc.physaddr = (unsigned long)vmalloc_area;
	entry->priv = private;
=======
		KGSL_CORE_ERR("remap_vmalloc_range failed: %d\n", result);
		goto error_free_vmalloc;
	}

	param->gpuaddr = entry->memdesc.gpuaddr;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	entry->memtype = KGSL_MEM_ENTRY_KERNEL;

<<<<<<< HEAD
	if (copy_to_user(arg, &param, sizeof(param))) {
		result = -EFAULT;
		goto error_unmap_entry;
	}
	private->vmalloc_size += len;
	list_add(&entry->list, &private->mem_list);
=======
	kgsl_mem_entry_attach_process(entry, private);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	/* Process specific statistics */
	kgsl_process_add_stats(private, entry->memtype, len);

	kgsl_check_idle(dev_priv->device);
	return 0;

error_free_vmalloc:
	kgsl_sharedmem_free(&entry->memdesc);

error_free_entry:
	kfree(entry);

error:
	kgsl_check_idle(dev_priv->device);
	return result;
}
<<<<<<< HEAD
#else
static inline int kgsl_ioctl_sharedmem_from_vmalloc(
			struct kgsl_file_private *private, void __user *arg)
{
	return -ENOSYS;
}
#endif
=======

static inline int _check_region(unsigned long start, unsigned long size,
				uint64_t len)
{
	uint64_t end = ((uint64_t) start) + size;
	return (end > len);
}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

static int kgsl_get_phys_file(int fd, unsigned long *start, unsigned long *len,
			      unsigned long *vstart, struct file **filep)
{
	struct file *fbfile;
	int ret = 0;
	dev_t rdev;
	struct fb_info *info;

	*filep = NULL;
#ifdef CONFIG_ANDROID_PMEM
	if (!get_pmem_file(fd, start, vstart, len, filep))
		return 0;
#endif

	fbfile = fget(fd);
	if (fbfile == NULL) {
		KGSL_CORE_ERR("fget_light failed\n");
		return -1;
	}

	rdev = fbfile->f_dentry->d_inode->i_rdev;
	info = MAJOR(rdev) == FB_MAJOR ? registered_fb[MINOR(rdev)] : NULL;
	if (info) {
		*start = info->fix.smem_start;
		*len = info->fix.smem_len;
		*vstart = (unsigned long)__va(info->fix.smem_start);
		ret = 0;
	} else {
		KGSL_CORE_ERR("framebuffer minor %d not found\n",
			      MINOR(rdev));
		ret = -1;
	}

	fput(fbfile);

	return ret;
}

static int kgsl_setup_phys_file(struct kgsl_mem_entry *entry,
				struct kgsl_pagetable *pagetable,
				unsigned int fd, unsigned int offset,
				size_t size)
{
	int ret;
	unsigned long phys, virt, len;
	struct file *filep;

<<<<<<< HEAD
static int kgsl_ioctl_sharedmem_from_pmem(struct kgsl_file_private *private,
						void __user *arg)
{
	int result = 0;
	struct kgsl_sharedmem_from_pmem param;
	struct kgsl_mem_entry *entry = NULL;
	unsigned long start = 0, len = 0;
	struct file *pmem_file = NULL;

	if (copy_from_user(&param, arg, sizeof(param))) {
		result = -EFAULT;
		goto error;
	}

	if (kgsl_get_phys_file(param.pmem_fd, &start, &len, &pmem_file)) {
		result = -EINVAL;
		goto error;
	} else if (param.offset + param.len > len) {
		KGSL_DRV_ERR("%s: region too large 0x%x + 0x%x >= 0x%lx\n",
			     __func__, param.offset, param.len, len);
		result = -EINVAL;
		goto error_put_pmem;
	}

	KGSL_MEM_INFO("get phys file %p start 0x%lx len 0x%lx\n",
		      pmem_file, start, len);
	KGSL_DRV_DBG("locked phys file %p\n", pmem_file);

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL) {
		result = -ENOMEM;
		goto error_put_pmem;
	}

	entry->pmem_file = pmem_file;
=======
	ret = kgsl_get_phys_file(fd, &phys, &len, &virt, &filep);
	if (ret)
		return ret;

	if (phys == 0) {
		ret = -EINVAL;
		goto err;
	}

	if (offset >= len) {
		ret = -EINVAL;
		goto err;
	}

	if (size == 0)
		size = len;

	/* Adjust the size of the region to account for the offset */
	size += offset & ~PAGE_MASK;

	size = ALIGN(size, PAGE_SIZE);

	if (_check_region(offset & PAGE_MASK, size, len)) {
		KGSL_CORE_ERR("Offset (%ld) + size (%d) is larger"
			      "than pmem region length %ld\n",
			      offset & PAGE_MASK, size, len);
		ret = -EINVAL;
		goto err;

	}

	entry->priv_data = filep;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = size;
	entry->memdesc.physaddr = phys + (offset & PAGE_MASK);
	entry->memdesc.hostptr = (void *) (virt + (offset & PAGE_MASK));

<<<<<<< HEAD
	/* Any MMU mapped memory must have a length in multiple of PAGESIZE */
	entry->memdesc.size = ALIGN(param.len, PAGE_SIZE);

	/*we shouldn't need to write here from kernel mode */
	entry->memdesc.hostptr = NULL;

	/* ensure that MMU mappings are at page boundary */
	entry->memdesc.physaddr = start + (param.offset & KGSL_PAGEMASK);
	result = kgsl_mmu_map(private->pagetable, entry->memdesc.physaddr,
			entry->memdesc.size, GSL_PT_PAGE_RV | GSL_PT_PAGE_WV,
			&entry->memdesc.gpuaddr,
			KGSL_MEMFLAGS_ALIGN4K | KGSL_MEMFLAGS_CONPHYS);
	if (result)
		goto error_free_entry;

	/* If the offset is not at 4K boundary then add the correct offset
	 * value to gpuaddr */
	entry->memdesc.gpuaddr += (param.offset & ~KGSL_PAGEMASK);
	param.gpuaddr = entry->memdesc.gpuaddr;

	if (copy_to_user(arg, &param, sizeof(param))) {
		result = -EFAULT;
		goto error_unmap_entry;
	}
	list_add(&entry->list, &private->mem_list);
	return result;

error_unmap_entry:
	kgsl_mmu_unmap(entry->memdesc.pagetable,
		       entry->memdesc.gpuaddr & KGSL_PAGEMASK,
		       entry->memdesc.size);
error_free_entry:
	kfree(entry);

error_put_pmem:
	kgsl_put_phys_file(pmem_file);
=======
	ret = memdesc_sg_phys(&entry->memdesc,
		phys + (offset & PAGE_MASK), size);
	if (ret)
		goto err;

	return 0;
err:
#ifdef CONFIG_ANDROID_PMEM
	put_pmem_file(filep);
#endif
	return ret;
}

static int memdesc_sg_virt(struct kgsl_memdesc *memdesc,
	void *addr, int size)
{
	int i;
	int sglen = PAGE_ALIGN(size) / PAGE_SIZE;
	unsigned long paddr = (unsigned long) addr;

	memdesc->sg = vmalloc(sglen * sizeof(struct scatterlist));
	if (memdesc->sg == NULL)
		return -ENOMEM;

	memdesc->sglen = sglen;
	sg_init_table(memdesc->sg, sglen);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	spin_lock(&current->mm->page_table_lock);

<<<<<<< HEAD
#ifdef CONFIG_MSM_KGSL_MMU
/*This function flushes a graphics memory allocation from CPU cache
 *when caching is enabled with MMU*/
static int kgsl_ioctl_sharedmem_flush_cache(struct kgsl_file_private *private,
				       void __user *arg)
{
	int result = 0;
	struct kgsl_mem_entry *entry;
	struct kgsl_sharedmem_free param;
=======
	for (i = 0; i < sglen; i++, paddr += PAGE_SIZE) {
		struct page *page;
		pmd_t *ppmd;
		pte_t *ppte;
		pgd_t *ppgd = pgd_offset(current->mm, paddr);
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

		if (pgd_none(*ppgd) || pgd_bad(*ppgd))
			goto err;

<<<<<<< HEAD
	entry = kgsl_sharedmem_find(private, param.gpuaddr);
	if (!entry) {
		KGSL_DRV_ERR("invalid gpuaddr %08x\n", param.gpuaddr);
		result = -EINVAL;
		goto done;
	}
	result = flush_l1_cache_range((unsigned long)entry->memdesc.hostptr,
				      entry->memdesc.size);
	/* Mark memory as being flushed so we don't flush it again */
	entry->memdesc.priv &= ~KGSL_MEMFLAGS_MEM_REQUIRES_FLUSH;
done:
	return result;
}
#else
static int kgsl_ioctl_sharedmem_flush_cache(struct kgsl_file_private *private,
					    void __user *arg)
{
	return -ENOSYS;
}
#endif

=======
		ppmd = pmd_offset(ppgd, paddr);
		if (pmd_none(*ppmd) || pmd_bad(*ppmd))
			goto err;

		ppte = pte_offset_map(ppmd, paddr);
		if (ppte == NULL)
			goto err;

		page = pfn_to_page(pte_pfn(*ppte));
		if (!page)
			goto err;

		sg_set_page(&memdesc->sg[i], page, PAGE_SIZE, 0);
		pte_unmap(ppte);
	}

	spin_unlock(&current->mm->page_table_lock);

	return 0;

err:
	spin_unlock(&current->mm->page_table_lock);
	vfree(memdesc->sg);
	memdesc->sg = NULL;

	return -EINVAL;
}

static int kgsl_setup_hostptr(struct kgsl_mem_entry *entry,
			      struct kgsl_pagetable *pagetable,
			      void *hostptr, unsigned int offset,
			      size_t size)
{
	struct vm_area_struct *vma;
	unsigned int len;

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, (unsigned int) hostptr);
	up_read(&current->mm->mmap_sem);

	if (!vma) {
		KGSL_CORE_ERR("find_vma(%p) failed\n", hostptr);
		return -EINVAL;
	}

	/* We don't necessarily start at vma->vm_start */
	len = vma->vm_end - (unsigned long) hostptr;

	if (offset >= len)
		return -EINVAL;

	if (!KGSL_IS_PAGE_ALIGNED((unsigned long) hostptr) ||
	    !KGSL_IS_PAGE_ALIGNED(len)) {
		KGSL_CORE_ERR("user address len(%u)"
			      "and start(%p) must be page"
			      "aligned\n", len, hostptr);
		return -EINVAL;
	}

	if (size == 0)
		size = len;

	/* Adjust the size of the region to account for the offset */
	size += offset & ~PAGE_MASK;

	size = ALIGN(size, PAGE_SIZE);

	if (_check_region(offset & PAGE_MASK, size, len)) {
		KGSL_CORE_ERR("Offset (%ld) + size (%d) is larger"
			      "than region length %d\n",
			      offset & PAGE_MASK, size, len);
		return -EINVAL;
	}

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = size;
	entry->memdesc.hostptr = hostptr + (offset & PAGE_MASK);

	return memdesc_sg_virt(&entry->memdesc,
		hostptr + (offset & PAGE_MASK), size);
}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

#ifdef CONFIG_ASHMEM
static int kgsl_setup_ashmem(struct kgsl_mem_entry *entry,
			     struct kgsl_pagetable *pagetable,
			     int fd, void *hostptr, size_t size)
{
<<<<<<< HEAD
	int result = 0;
	struct kgsl_file_private *private = filep->private_data;
	struct kgsl_drawctxt_set_bin_base_offset binbase;

	BUG_ON(private == NULL);
=======
	int ret;
	struct vm_area_struct *vma;
	struct file *filep, *vmfile;
	unsigned long len;
	unsigned int hostaddr = (unsigned int) hostptr;

	vma = kgsl_get_vma_from_start_addr(hostaddr);
	if (vma == NULL)
		return -EINVAL;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	if (vma->vm_pgoff || vma->vm_start != hostaddr) {
		KGSL_CORE_ERR("Invalid vma region\n");
		return -EINVAL;
	}

<<<<<<< HEAD
	mutex_lock(&kgsl_driver.mutex);

	kgsl_hw_get_locked();
=======
	len = vma->vm_end - vma->vm_start;

	if (size == 0)
		size = len;

	if (size != len) {
		KGSL_CORE_ERR("Invalid size %d for vma region %p\n",
			      size, hostptr);
		return -EINVAL;
	}
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock

	ret = get_ashmem_file(fd, &filep, &vmfile, &len);

<<<<<<< HEAD
	case IOCTL_KGSL_DEVICE_GETPROPERTY:
		result =
		    kgsl_ioctl_device_getproperty(private, (void __user *)arg);
		break;

	case IOCTL_KGSL_DEVICE_REGREAD:
		result = kgsl_ioctl_device_regread(private, (void __user *)arg);
		break;

	case IOCTL_KGSL_DEVICE_WAITTIMESTAMP:
		result = kgsl_ioctl_device_waittimestamp(private,
							(void __user *)arg);
		break;

	case IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS:
		if (kgsl_cache_enable)
			flush_l1_cache_all(private);
		result = kgsl_ioctl_rb_issueibcmds(private, (void __user *)arg);
		break;

	case IOCTL_KGSL_CMDSTREAM_READTIMESTAMP:
		result =
		    kgsl_ioctl_cmdstream_readtimestamp(private,
							(void __user *)arg);
		break;

	case IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP:
		result =
		    kgsl_ioctl_cmdstream_freememontimestamp(private,
						    (void __user *)arg);
		break;

	case IOCTL_KGSL_DRAWCTXT_CREATE:
		result = kgsl_ioctl_drawctxt_create(private,
							(void __user *)arg);
		break;

	case IOCTL_KGSL_DRAWCTXT_DESTROY:
		result =
		    kgsl_ioctl_drawctxt_destroy(private, (void __user *)arg);
		break;

	case IOCTL_KGSL_SHAREDMEM_FREE:
		result = kgsl_ioctl_sharedmem_free(private, (void __user *)arg);
		break;

	case IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC:
		kgsl_yamato_runpending(&kgsl_driver.yamato_device);
		result = kgsl_ioctl_sharedmem_from_vmalloc(private,
							   (void __user *)arg);
		break;

	case IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE:
		if (kgsl_cache_enable)
			result = kgsl_ioctl_sharedmem_flush_cache(private,
						       (void __user *)arg);
		break;
	case IOCTL_KGSL_SHAREDMEM_FROM_PMEM:
		kgsl_yamato_runpending(&kgsl_driver.yamato_device);
		result = kgsl_ioctl_sharedmem_from_pmem(private,
							(void __user *)arg);
		break;

	case IOCTL_KGSL_DRAWCTXT_SET_BIN_BASE_OFFSET:
		if (copy_from_user(&binbase, (void __user *)arg,
				   sizeof(binbase))) {
			result = -EFAULT;
			break;
		}

		if (private->ctxt_id_mask & (1 << binbase.drawctxt_id)) {
			result = kgsl_drawctxt_set_bin_base_offset(
					&kgsl_driver.yamato_device,
					binbase.drawctxt_id,
					binbase.offset);
		} else {
			result = -EINVAL;
			KGSL_DRV_ERR("invalid drawctxt drawctxt_id %d\n",
				     binbase.drawctxt_id);
		}
		break;

	default:
		KGSL_DRV_ERR("invalid ioctl code %08x\n", cmd);
		result = -EINVAL;
		break;
	}

	kgsl_hw_put_locked(true);
	mutex_unlock(&kgsl_driver.mutex);
	KGSL_DRV_VDBG("result %d\n", result);
	return result;
}

static struct file_operations kgsl_fops = {
=======
	if (ret) {
		KGSL_CORE_ERR("get_ashmem_file failed\n");
		return ret;
	}

	if (vmfile != vma->vm_file) {
		KGSL_CORE_ERR("ashmem shmem file does not match vma\n");
		ret = -EINVAL;
		goto err;
	}

	entry->priv_data = filep;
	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = ALIGN(size, PAGE_SIZE);
	entry->memdesc.hostptr = hostptr;

	ret = memdesc_sg_virt(&entry->memdesc, hostptr, size);
	if (ret)
		goto err;

	return 0;

err:
	put_ashmem_file(filep);
	return ret;
}
#else
static int kgsl_setup_ashmem(struct kgsl_mem_entry *entry,
			     struct kgsl_pagetable *pagetable,
			     int fd, void *hostptr, size_t size)
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_ION_MSM
static int kgsl_setup_ion(struct kgsl_mem_entry *entry,
		struct kgsl_pagetable *pagetable, int fd)
{
	struct ion_handle *handle;
	struct scatterlist *s;
	unsigned long flags;

	if (kgsl_ion_client == NULL) {
		kgsl_ion_client = msm_ion_client_create(UINT_MAX, KGSL_NAME);
		if (kgsl_ion_client == NULL)
			return -ENODEV;
	}

	handle = ion_import_fd(kgsl_ion_client, fd);
	if (IS_ERR_OR_NULL(handle))
		return PTR_ERR(handle);

	entry->memtype = KGSL_MEM_ENTRY_ION;
	entry->priv_data = handle;
	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = 0;

	if (ion_handle_get_flags(kgsl_ion_client, handle, &flags))
		goto err;

	entry->memdesc.sg = ion_map_dma(kgsl_ion_client, handle, flags);

	if (IS_ERR_OR_NULL(entry->memdesc.sg))
		goto err;

	/* Calculate the size of the memdesc from the sglist */

	entry->memdesc.sglen = 0;

	for (s = entry->memdesc.sg; s != NULL; s = sg_next(s)) {
		entry->memdesc.size += s->length;
		entry->memdesc.sglen++;
	}

	return 0;
err:
	ion_free(kgsl_ion_client, handle);
	return -ENOMEM;
}
#endif

static long kgsl_ioctl_map_user_mem(struct kgsl_device_private *dev_priv,
				     unsigned int cmd, void *data)
{
	int result = -EINVAL;
	struct kgsl_map_user_mem *param = data;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_process_private *private = dev_priv->process_priv;
	enum kgsl_user_mem_type memtype;

	entry = kgsl_mem_entry_create();

	if (entry == NULL)
		return -ENOMEM;

	if (_IOC_SIZE(cmd) == sizeof(struct kgsl_sharedmem_from_pmem))
		memtype = KGSL_USER_MEM_TYPE_PMEM;
	else
		memtype = param->memtype;

	switch (memtype) {
	case KGSL_USER_MEM_TYPE_PMEM:
		if (param->fd == 0 || param->len == 0)
			break;

		result = kgsl_setup_phys_file(entry, private->pagetable,
					      param->fd, param->offset,
					      param->len);
		entry->memtype = KGSL_MEM_ENTRY_PMEM;
		break;

	case KGSL_USER_MEM_TYPE_ADDR:
		if (!kgsl_mmu_enabled()) {
			KGSL_DRV_ERR(dev_priv->device,
				"Cannot map paged memory with the "
				"MMU disabled\n");
			break;
		}

		if (param->hostptr == 0)
			break;

		result = kgsl_setup_hostptr(entry, private->pagetable,
					    (void *) param->hostptr,
					    param->offset, param->len);
		entry->memtype = KGSL_MEM_ENTRY_USER;
		break;

	case KGSL_USER_MEM_TYPE_ASHMEM:
		if (!kgsl_mmu_enabled()) {
			KGSL_DRV_ERR(dev_priv->device,
				"Cannot map paged memory with the "
				"MMU disabled\n");
			break;
		}

		if (param->hostptr == 0)
			break;

		result = kgsl_setup_ashmem(entry, private->pagetable,
					   param->fd, (void *) param->hostptr,
					   param->len);

		entry->memtype = KGSL_MEM_ENTRY_ASHMEM;
		break;
#ifdef CONFIG_ION_MSM
	case KGSL_USER_MEM_TYPE_ION:
		result = kgsl_setup_ion(entry, private->pagetable,
			param->fd);
		break;
#endif
	default:
		KGSL_CORE_ERR("Invalid memory type: %x\n", memtype);
		break;
	}

	if (result)
		goto error;

	result = kgsl_mmu_map(private->pagetable,
			      &entry->memdesc,
			      GSL_PT_PAGE_RV | GSL_PT_PAGE_WV);

	if (result)
		goto error_put_file_ptr;

	/* Adjust the returned value for a non 4k aligned offset */
	param->gpuaddr = entry->memdesc.gpuaddr + (param->offset & ~PAGE_MASK);

	KGSL_STATS_ADD(param->len, kgsl_driver.stats.mapped,
		kgsl_driver.stats.mapped_max);

	kgsl_process_add_stats(private, entry->memtype, param->len);

	kgsl_mem_entry_attach_process(entry, private);

	kgsl_check_idle(dev_priv->device);
	return result;

 error_put_file_ptr:
	if (entry->priv_data)
		fput(entry->priv_data);

error:
	kfree(entry);
	kgsl_check_idle(dev_priv->device);
	return result;
}

/*This function flushes a graphics memory allocation from CPU cache
 *when caching is enabled with MMU*/
static long
kgsl_ioctl_sharedmem_flush_cache(struct kgsl_device_private *dev_priv,
				 unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_mem_entry *entry;
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;

	spin_lock(&private->mem_lock);
	entry = kgsl_sharedmem_find(private, param->gpuaddr);
	if (!entry) {
		KGSL_CORE_ERR("invalid gpuaddr %08x\n", param->gpuaddr);
		result = -EINVAL;
		goto done;
	}
	if (!entry->memdesc.hostptr) {
		KGSL_CORE_ERR("invalid hostptr with gpuaddr %08x\n",
			param->gpuaddr);
			goto done;
	}

	kgsl_cache_range_op(&entry->memdesc, KGSL_CACHE_OP_CLEAN);
done:
	spin_unlock(&private->mem_lock);
	return result;
}

static long
kgsl_ioctl_gpumem_alloc(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpumem_alloc *param = data;
	struct kgsl_mem_entry *entry;
	int result;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	result = kgsl_allocate_user(&entry->memdesc, private->pagetable,
		param->size, param->flags);

	if (result == 0) {
		entry->memtype = KGSL_MEM_ENTRY_KERNEL;
		kgsl_mem_entry_attach_process(entry, private);
		param->gpuaddr = entry->memdesc.gpuaddr;

		kgsl_process_add_stats(private, entry->memtype, param->size);
	} else
		kfree(entry);

	kgsl_check_idle(dev_priv->device);
	return result;
}
static long kgsl_ioctl_cff_syncmem(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_cff_syncmem *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;

	spin_lock(&private->mem_lock);
	entry = kgsl_sharedmem_find_region(private, param->gpuaddr, param->len);
	if (entry)
		kgsl_cffdump_syncmem(dev_priv, &entry->memdesc, param->gpuaddr,
				     param->len, true);
	else
		result = -EINVAL;
	spin_unlock(&private->mem_lock);
	return result;
}

static long kgsl_ioctl_cff_user_event(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_cff_user_event *param = data;

	kgsl_cffdump_user_event(param->cff_opcode, param->op1, param->op2,
			param->op3, param->op4, param->op5);

	return result;
}

#ifdef CONFIG_GENLOCK
struct kgsl_genlock_event_priv {
	struct genlock_handle *handle;
	struct genlock *lock;
};

/**
 * kgsl_genlock_event_cb - Event callback for a genlock timestamp event
 * @device - The KGSL device that expired the timestamp
 * @priv - private data for the event
 * @timestamp - the timestamp that triggered the event
 *
 * Release a genlock lock following the expiration of a timestamp
 */

static void kgsl_genlock_event_cb(struct kgsl_device *device,
	void *priv, u32 timestamp)
{
	struct kgsl_genlock_event_priv *ev = priv;
	int ret;

	ret = genlock_lock(ev->handle, GENLOCK_UNLOCK, 0, 0);
	if (ret)
		KGSL_CORE_ERR("Error while unlocking genlock: %d\n", ret);

	genlock_put_handle(ev->handle);

	kfree(ev);
}

/**
 * kgsl_add_genlock-event - Create a new genlock event
 * @device - KGSL device to create the event on
 * @timestamp - Timestamp to trigger the event
 * @data - User space buffer containing struct kgsl_genlock_event_priv
 * @len - length of the userspace buffer
 * @owner - driver instance that owns this event
 * @returns 0 on success or error code on error
 *
 * Attack to a genlock handle and register an event to release the
 * genlock lock when the timestamp expires
 */

static int kgsl_add_genlock_event(struct kgsl_device *device,
	u32 timestamp, void __user *data, int len,
	struct kgsl_device_private *owner)
{
	struct kgsl_genlock_event_priv *event;
	struct kgsl_timestamp_event_genlock priv;
	int ret;

	if (len !=  sizeof(priv))
		return -EINVAL;

	if (copy_from_user(&priv, data, sizeof(priv)))
		return -EFAULT;

	event = kzalloc(sizeof(*event), GFP_KERNEL);

	if (event == NULL)
		return -ENOMEM;

	event->handle = genlock_get_handle_fd(priv.handle);

	if (IS_ERR(event->handle)) {
		int ret = PTR_ERR(event->handle);
		kfree(event);
		return ret;
	}

	ret = kgsl_add_event(device, timestamp, kgsl_genlock_event_cb, event,
			     owner);
	if (ret)
		kfree(event);

	return ret;
}
#else
static long kgsl_add_genlock_event(struct kgsl_device *device,
	u32 timestamp, void __user *data, int len,
	struct kgsl_device_private *owner)
{
	return -EINVAL;
}
#endif

/**
 * kgsl_ioctl_timestamp_event - Register a new timestamp event from userspace
 * @dev_priv - pointer to the private device structure
 * @cmd - the ioctl cmd passed from kgsl_ioctl
 * @data - the user data buffer from kgsl_ioctl
 * @returns 0 on success or error code on failure
 */

static long kgsl_ioctl_timestamp_event(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_timestamp_event *param = data;
	int ret;

	switch (param->type) {
	case KGSL_TIMESTAMP_EVENT_GENLOCK:
		ret = kgsl_add_genlock_event(dev_priv->device,
			param->timestamp, param->priv, param->len,
			dev_priv);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

typedef long (*kgsl_ioctl_func_t)(struct kgsl_device_private *,
	unsigned int, void *);

#define KGSL_IOCTL_FUNC(_cmd, _func, _lock) \
	[_IOC_NR(_cmd)] = { .cmd = _cmd, .func = _func, .lock = _lock }

static const struct {
	unsigned int cmd;
	kgsl_ioctl_func_t func;
	int lock;
} kgsl_ioctl_funcs[] = {
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_GETPROPERTY,
			kgsl_ioctl_device_getproperty, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DEVICE_WAITTIMESTAMP,
			kgsl_ioctl_device_waittimestamp, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS,
			kgsl_ioctl_rb_issueibcmds, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_READTIMESTAMP,
			kgsl_ioctl_cmdstream_readtimestamp, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP,
			kgsl_ioctl_cmdstream_freememontimestamp, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_CREATE,
			kgsl_ioctl_drawctxt_create, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_DRAWCTXT_DESTROY,
			kgsl_ioctl_drawctxt_destroy, 1),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_MAP_USER_MEM,
			kgsl_ioctl_map_user_mem, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FROM_PMEM,
			kgsl_ioctl_map_user_mem, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FREE,
			kgsl_ioctl_sharedmem_free, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FROM_VMALLOC,
			kgsl_ioctl_sharedmem_from_vmalloc, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_SHAREDMEM_FLUSH_CACHE,
			kgsl_ioctl_sharedmem_flush_cache, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_GPUMEM_ALLOC,
			kgsl_ioctl_gpumem_alloc, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CFF_SYNCMEM,
			kgsl_ioctl_cff_syncmem, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_CFF_USER_EVENT,
			kgsl_ioctl_cff_user_event, 0),
	KGSL_IOCTL_FUNC(IOCTL_KGSL_TIMESTAMP_EVENT,
			kgsl_ioctl_timestamp_event, 1),
};

static long kgsl_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	unsigned int nr = _IOC_NR(cmd);
	kgsl_ioctl_func_t func;
	int lock, ret;
	char ustack[64];
	void *uptr = NULL;

	BUG_ON(dev_priv == NULL);

	/* Workaround for an previously incorrectly defined ioctl code.
	   This helps ensure binary compatability */

	if (cmd == IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP_OLD)
		cmd = IOCTL_KGSL_CMDSTREAM_FREEMEMONTIMESTAMP;
	else if (cmd == IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_OLD)
		cmd = IOCTL_KGSL_CMDSTREAM_READTIMESTAMP;

	if (cmd & (IOC_IN | IOC_OUT)) {
		if (_IOC_SIZE(cmd) < sizeof(ustack))
			uptr = ustack;
		else {
			uptr = kzalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (uptr == NULL) {
				KGSL_MEM_ERR(dev_priv->device,
					"kzalloc(%d) failed\n", _IOC_SIZE(cmd));
				ret = -ENOMEM;
				goto done;
			}
		}

		if (cmd & IOC_IN) {
			if (copy_from_user(uptr, (void __user *) arg,
				_IOC_SIZE(cmd))) {
				ret = -EFAULT;
				goto done;
			}
		} else
			memset(uptr, 0, _IOC_SIZE(cmd));
	}

	if (nr < ARRAY_SIZE(kgsl_ioctl_funcs) &&
	    kgsl_ioctl_funcs[nr].func != NULL) {
		func = kgsl_ioctl_funcs[nr].func;
		lock = kgsl_ioctl_funcs[nr].lock;
	} else {
		func = dev_priv->device->ftbl->ioctl;
		if (!func) {
			KGSL_DRV_INFO(dev_priv->device,
				      "invalid ioctl code %08x\n", cmd);
			ret = -EINVAL;
			goto done;
		}
		lock = 1;
	}

	if (lock) {
		mutex_lock(&dev_priv->device->mutex);
		kgsl_check_suspended(dev_priv->device);
	}

	ret = func(dev_priv, cmd, uptr);

	if (lock) {
		kgsl_check_idle_locked(dev_priv->device);
		mutex_unlock(&dev_priv->device->mutex);
	}

	if (ret == 0 && (cmd & IOC_OUT)) {
		if (copy_to_user((void __user *) arg, uptr, _IOC_SIZE(cmd)))
			ret = -EFAULT;
	}

done:
	if (_IOC_SIZE(cmd) >= sizeof(ustack))
		kfree(uptr);

	return ret;
}

static int
kgsl_mmap_memstore(struct kgsl_device *device, struct vm_area_struct *vma)
{
	struct kgsl_memdesc *memdesc = &device->memstore;
	int result;
	unsigned int vma_size = vma->vm_end - vma->vm_start;

	/* The memstore can only be mapped as read only */

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	if (memdesc->size  !=  vma_size) {
		KGSL_MEM_ERR(device, "memstore bad size: %d should be %d\n",
			     vma_size, memdesc->size);
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	result = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vma_size, vma->vm_page_prot);
	if (result != 0)
		KGSL_MEM_ERR(device, "remap_pfn_range failed: %d\n",
			     result);

	return result;
}

/*
 * kgsl_gpumem_vm_open is called whenever a vma region is copied or split.
 * Increase the refcount to make sure that the accounting stays correct
 */

static void kgsl_gpumem_vm_open(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;
	kgsl_mem_entry_get(entry);
}

static int
kgsl_gpumem_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;

	if (!entry->memdesc.ops || !entry->memdesc.ops->vmfault)
		return VM_FAULT_SIGBUS;

	return entry->memdesc.ops->vmfault(&entry->memdesc, vma, vmf);
}

static void
kgsl_gpumem_vm_close(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry  = vma->vm_private_data;
	kgsl_mem_entry_put(entry);
}

static struct vm_operations_struct kgsl_gpumem_vm_ops = {
	.open  = kgsl_gpumem_vm_open,
	.fault = kgsl_gpumem_vm_fault,
	.close = kgsl_gpumem_vm_close,
};

static int kgsl_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vma_offset = vma->vm_pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *tmp, *entry = NULL;
	struct kgsl_device *device = dev_priv->device;

	/* Handle leagacy behavior for memstore */

	if (vma_offset == device->memstore.physaddr)
		return kgsl_mmap_memstore(device, vma);

	/* Find a chunk of GPU memory */

	spin_lock(&private->mem_lock);
	list_for_each_entry(tmp, &private->mem_list, list) {
		if (vma_offset == tmp->memdesc.gpuaddr) {
			kgsl_mem_entry_get(tmp);
			entry = tmp;
			break;
		}
	}
	spin_unlock(&private->mem_lock);

	if (entry == NULL)
		return -EINVAL;

	if (!entry->memdesc.ops ||
		!entry->memdesc.ops->vmflags ||
		!entry->memdesc.ops->vmfault)
		return -EINVAL;

	vma->vm_flags |= entry->memdesc.ops->vmflags(&entry->memdesc);

	vma->vm_private_data = entry;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &kgsl_gpumem_vm_ops;
	vma->vm_file = file;

	return 0;
}

static const struct file_operations kgsl_fops = {
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	.owner = THIS_MODULE,
	.release = kgsl_release,
	.open = kgsl_open,
	.unlocked_ioctl = kgsl_ioctl,
};

<<<<<<< HEAD

struct kgsl_driver kgsl_driver = {
	.misc = {
		 .name = DRIVER_NAME,
		 .minor = MISC_DYNAMIC_MINOR,
		 .fops = &kgsl_fops,
	 },
	.open_count = ATOMIC_INIT(0),
	.mutex = __MUTEX_INITIALIZER(kgsl_driver.mutex),
};

static void kgsl_driver_cleanup(void)
{

	wake_lock_destroy(&kgsl_driver.wake_lock);

	if (kgsl_driver.interrupt_num > 0) {
		if (kgsl_driver.have_irq) {
			free_irq(kgsl_driver.interrupt_num, NULL);
			kgsl_driver.have_irq = 0;
		}
		kgsl_driver.interrupt_num = 0;
	}

	if (kgsl_driver.grp_clk) {
		clk_put(kgsl_driver.grp_clk);
		kgsl_driver.grp_clk = NULL;
	}

	if (kgsl_driver.imem_clk != NULL) {
		clk_put(kgsl_driver.imem_clk);
		kgsl_driver.imem_clk = NULL;
	}

	if (kgsl_driver.ebi1_clk != NULL) {
		clk_put(kgsl_driver.ebi1_clk);
		kgsl_driver.ebi1_clk = NULL;
	}

	kgsl_driver.pdev = NULL;

=======
struct kgsl_driver kgsl_driver  = {
	.process_mutex = __MUTEX_INITIALIZER(kgsl_driver.process_mutex),
	.ptlock = __SPIN_LOCK_UNLOCKED(kgsl_driver.ptlock),
	.devlock = __MUTEX_INITIALIZER(kgsl_driver.devlock),
};
EXPORT_SYMBOL(kgsl_driver);

void kgsl_unregister_device(struct kgsl_device *device)
{
	int minor;

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < KGSL_DEVICE_MAX; minor++) {
		if (device == kgsl_driver.devp[minor])
			break;
	}

	mutex_unlock(&kgsl_driver.devlock);

	if (minor == KGSL_DEVICE_MAX)
		return;

	kgsl_device_snapshot_close(device);

	kgsl_cffdump_close(device->id);
	kgsl_pwrctrl_uninit_sysfs(device);

	if (cpu_is_msm8x60())
		wake_lock_destroy(&device->idle_wakelock);

	idr_destroy(&device->context_idr);

	if (device->memstore.hostptr)
		kgsl_sharedmem_free(&device->memstore);

	kgsl_mmu_close(device);

	if (device->work_queue) {
		destroy_workqueue(device->work_queue);
		device->work_queue = NULL;
	}

	device_destroy(kgsl_driver.class,
		       MKDEV(MAJOR(kgsl_driver.major), minor));

	mutex_lock(&kgsl_driver.devlock);
	kgsl_driver.devp[minor] = NULL;
	mutex_unlock(&kgsl_driver.devlock);
}
EXPORT_SYMBOL(kgsl_unregister_device);

int
kgsl_register_device(struct kgsl_device *device)
{
	int minor, ret;
	dev_t dev;

	/* Find a minor for the device */

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < KGSL_DEVICE_MAX; minor++) {
		if (kgsl_driver.devp[minor] == NULL) {
			kgsl_driver.devp[minor] = device;
			break;
		}
	}

	mutex_unlock(&kgsl_driver.devlock);

	if (minor == KGSL_DEVICE_MAX) {
		KGSL_CORE_ERR("minor devices exhausted\n");
		return -ENODEV;
	}

	/* Create the device */
	dev = MKDEV(MAJOR(kgsl_driver.major), minor);
	device->dev = device_create(kgsl_driver.class,
				    device->parentdev,
				    dev, device,
				    device->name);

	if (IS_ERR(device->dev)) {
		ret = PTR_ERR(device->dev);
		KGSL_CORE_ERR("device_create(%s): %d\n", device->name, ret);
		goto err_devlist;
	}

	dev_set_drvdata(device->parentdev, device);

	/* Generic device initialization */
	init_waitqueue_head(&device->wait_queue);

	kgsl_cffdump_open(device->id);

	init_completion(&device->hwaccess_gate);
	init_completion(&device->suspend_gate);

	ATOMIC_INIT_NOTIFIER_HEAD(&device->ts_notifier_list);

	setup_timer(&device->idle_timer, kgsl_timer, (unsigned long) device);
	ret = kgsl_create_device_workqueue(device);
	if (ret)
		goto err_devlist;

	INIT_WORK(&device->idle_check_ws, kgsl_idle_check);
	INIT_WORK(&device->ts_expired_ws, kgsl_timestamp_expired);

	INIT_LIST_HEAD(&device->events);

	ret = kgsl_mmu_init(device);
	if (ret != 0)
		goto err_dest_work_q;

	ret = kgsl_allocate_contiguous(&device->memstore,
		sizeof(struct kgsl_devmemstore));

	if (ret != 0)
		goto err_close_mmu;

	if (cpu_is_msm8x60())
		wake_lock_init(&device->idle_wakelock,
					   WAKE_LOCK_IDLE, device->name);

	idr_init(&device->context_idr);

	/* Initalize the snapshot engine */
	kgsl_device_snapshot_init(device);

	/* sysfs and debugfs initalization - failure here is non fatal */

	/* Initialize logging */
	kgsl_device_debugfs_init(device);

	/* Initialize common sysfs entries */
	kgsl_pwrctrl_init_sysfs(device);

	return 0;

err_close_mmu:
	kgsl_mmu_close(device);
err_dest_work_q:
	destroy_workqueue(device->work_queue);
	device->work_queue = NULL;
err_devlist:
	mutex_lock(&kgsl_driver.devlock);
	kgsl_driver.devp[minor] = NULL;
	mutex_unlock(&kgsl_driver.devlock);

	return ret;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
}
EXPORT_SYMBOL(kgsl_register_device);

<<<<<<< HEAD

static int __devinit kgsl_platform_probe(struct platform_device *pdev)
{
	int result = 0;
	struct clk *clk;
	struct resource *res = NULL;

	kgsl_debug_init();

	INIT_LIST_HEAD(&kgsl_driver.client_list);

	/*acquire clocks */
	BUG_ON(kgsl_driver.grp_clk != NULL);
	BUG_ON(kgsl_driver.imem_clk != NULL);
	BUG_ON(kgsl_driver.ebi1_clk != NULL);

	kgsl_driver.pdev = pdev;

	setup_timer(&kgsl_driver.standby_timer, kgsl_do_standby_timer, 0);
	wake_lock_init(&kgsl_driver.wake_lock, WAKE_LOCK_SUSPEND, "kgsl");

	clk = clk_get(&pdev->dev, "grp_clk");
	if (IS_ERR(clk)) {
		result = PTR_ERR(clk);
		KGSL_DRV_ERR("clk_get(grp_clk) returned %d\n", result);
		goto done;
	}
	kgsl_driver.grp_clk = clk;

	clk = clk_get(&pdev->dev, "grp_pclk");
	if (IS_ERR(clk)) {
		KGSL_DRV_ERR("no grp_pclk, continuing\n");
		clk = NULL;
	}
	kgsl_driver.grp_pclk = clk;

	clk = clk_get(&pdev->dev, "imem_clk");
	if (IS_ERR(clk)) {
		result = PTR_ERR(clk);
		KGSL_DRV_ERR("clk_get(imem_clk) returned %d\n", result);
		goto done;
	}
	kgsl_driver.imem_clk = clk;

	clk = clk_get(&pdev->dev, "ebi1_clk");
	if (IS_ERR(clk)) {
		result = PTR_ERR(clk);
		KGSL_DRV_ERR("clk_get(ebi1_clk) returned %d\n", result);
		goto done;
=======
int kgsl_device_platform_probe(struct kgsl_device *device,
			       irqreturn_t (*dev_isr) (int, void*))
{
	int status = -EINVAL;
	struct kgsl_memregion *regspace = NULL;
	struct resource *res;
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);

	pm_runtime_enable(device->parentdev);

	status = kgsl_pwrctrl_init(device);
	if (status)
		goto error;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   device->iomemname);
	if (res == NULL) {
		KGSL_DRV_ERR(device, "platform_get_resource_byname failed\n");
		status = -EINVAL;
		goto error_pwrctrl_close;
	}
	if (res->start == 0 || resource_size(res) == 0) {
		KGSL_DRV_ERR(device, "dev %d invalid regspace\n", device->id);
		status = -EINVAL;
		goto error_pwrctrl_close;
	}

	regspace = &device->regspace;
	regspace->mmio_phys_base = res->start;
	regspace->sizebytes = resource_size(res);

	if (!request_mem_region(regspace->mmio_phys_base,
				regspace->sizebytes, device->name)) {
		KGSL_DRV_ERR(device, "request_mem_region failed\n");
		status = -ENODEV;
		goto error_pwrctrl_close;
	}

	regspace->mmio_virt_base = ioremap(regspace->mmio_phys_base,
					   regspace->sizebytes);

	if (regspace->mmio_virt_base == NULL) {
		KGSL_DRV_ERR(device, "ioremap failed\n");
		status = -ENODEV;
		goto error_release_mem;
	}

	status = request_irq(device->pwrctrl.interrupt_num, dev_isr,
			     IRQF_TRIGGER_HIGH, device->name, device);
	if (status) {
		KGSL_DRV_ERR(device, "request_irq(%d) failed: %d\n",
			      device->pwrctrl.interrupt_num, status);
		goto error_iounmap;
	}
	device->pwrctrl.have_irq = 1;
	disable_irq(device->pwrctrl.interrupt_num);

	KGSL_DRV_INFO(device,
		"dev_id %d regs phys 0x%08x size 0x%08x virt %p\n",
		device->id, regspace->mmio_phys_base,
		regspace->sizebytes, regspace->mmio_virt_base);


	status = kgsl_register_device(device);
	if (!status)
		return status;

	free_irq(device->pwrctrl.interrupt_num, NULL);
	device->pwrctrl.have_irq = 0;
error_iounmap:
	iounmap(regspace->mmio_virt_base);
	regspace->mmio_virt_base = NULL;
error_release_mem:
	release_mem_region(regspace->mmio_phys_base, regspace->sizebytes);
error_pwrctrl_close:
	kgsl_pwrctrl_close(device);
error:
	return status;
}
EXPORT_SYMBOL(kgsl_device_platform_probe);

void kgsl_device_platform_remove(struct kgsl_device *device)
{
	struct kgsl_memregion *regspace = &device->regspace;

	kgsl_unregister_device(device);

	if (regspace->mmio_virt_base != NULL) {
		iounmap(regspace->mmio_virt_base);
		regspace->mmio_virt_base = NULL;
		release_mem_region(regspace->mmio_phys_base,
					regspace->sizebytes);
	}
	kgsl_pwrctrl_close(device);

	pm_runtime_disable(device->parentdev);
}
EXPORT_SYMBOL(kgsl_device_platform_remove);

static int __devinit
kgsl_ptdata_init(void)
{
	kgsl_driver.ptpool = kgsl_mmu_ptpool_init(KGSL_PAGETABLE_SIZE,
						kgsl_pagetable_count);
	if (!kgsl_driver.ptpool)
		return -ENOMEM;
	return 0;
}

static void kgsl_core_exit(void)
{
	unregister_chrdev_region(kgsl_driver.major, KGSL_DEVICE_MAX);

	kgsl_mmu_ptpool_destroy(&kgsl_driver.ptpool);
	kgsl_driver.ptpool = NULL;

	device_unregister(&kgsl_driver.virtdev);

	if (kgsl_driver.class) {
		class_destroy(kgsl_driver.class);
		kgsl_driver.class = NULL;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}
	kgsl_driver.ebi1_clk = clk;

<<<<<<< HEAD
	/*acquire interrupt */
	kgsl_driver.interrupt_num = platform_get_irq(pdev, 0);
	if (kgsl_driver.interrupt_num <= 0) {
		KGSL_DRV_ERR("platform_get_irq() returned %d\n",
			       kgsl_driver.interrupt_num);
		result = -EINVAL;
		goto done;
	}

	result = request_irq(kgsl_driver.interrupt_num, kgsl_yamato_isr,
				IRQF_TRIGGER_HIGH, DRIVER_NAME, NULL);
	if (result) {
		KGSL_DRV_ERR("request_irq(%d) returned %d\n",
			      kgsl_driver.interrupt_num, result);
		goto done;
=======
	kgsl_drm_exit();
	kgsl_cffdump_destroy();
	kgsl_core_debugfs_close();
	kgsl_sharedmem_uninit_sysfs();
}

static int __init kgsl_core_init(void)
{
	int result = 0;
	/* alloc major and minor device numbers */
	result = alloc_chrdev_region(&kgsl_driver.major, 0, KGSL_DEVICE_MAX,
				  KGSL_NAME);
	if (result < 0) {
		KGSL_CORE_ERR("alloc_chrdev_region failed err = %d\n", result);
		goto err;
	}

	cdev_init(&kgsl_driver.cdev, &kgsl_fops);
	kgsl_driver.cdev.owner = THIS_MODULE;
	kgsl_driver.cdev.ops = &kgsl_fops;
	result = cdev_add(&kgsl_driver.cdev, MKDEV(MAJOR(kgsl_driver.major), 0),
		       KGSL_DEVICE_MAX);

	if (result) {
		KGSL_CORE_ERR("kgsl: cdev_add() failed, dev_num= %d,"
			     " result= %d\n", kgsl_driver.major, result);
		goto err;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}
	kgsl_driver.have_irq = 1;
	disable_irq(kgsl_driver.interrupt_num);

<<<<<<< HEAD
	result = kgsl_yamato_config(&kgsl_driver.yamato_config, pdev);
	if (result != 0)
		goto done;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "kgsl_phys_memory");
	if (res == NULL) {
		result = -EINVAL;
		goto done;
	}

	kgsl_driver.shmem.physbase = res->start;
	kgsl_driver.shmem.size = resource_size(res);

done:
	if (result)
		kgsl_driver_cleanup();
	else
		result = misc_register(&kgsl_driver.misc);

	return result;
}

static int kgsl_platform_remove(struct platform_device *pdev)
{

	kgsl_driver_cleanup();
	misc_deregister(&kgsl_driver.misc);

	return 0;
}

static int kgsl_platform_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	mutex_lock(&kgsl_driver.mutex);
	if (atomic_read(&kgsl_driver.open_count) > 0) {
		if (kgsl_driver.active)
			pr_err("%s: Suspending while active???\n", __func__);
	}
	mutex_unlock(&kgsl_driver.mutex);
	return 0;
}

static struct platform_driver kgsl_platform_driver = {
	.probe = kgsl_platform_probe,
	.remove = __devexit_p(kgsl_platform_remove),
	.suspend = kgsl_platform_suspend,
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME
=======
	kgsl_driver.class = class_create(THIS_MODULE, KGSL_NAME);

	if (IS_ERR(kgsl_driver.class)) {
		result = PTR_ERR(kgsl_driver.class);
		KGSL_CORE_ERR("failed to create class %s", KGSL_NAME);
		goto err;
	}

	/* Make a virtual device for managing core related things
	   in sysfs */
	kgsl_driver.virtdev.class = kgsl_driver.class;
	dev_set_name(&kgsl_driver.virtdev, "kgsl");
	result = device_register(&kgsl_driver.virtdev);
	if (result) {
		KGSL_CORE_ERR("driver_register failed\n");
		goto err;
	}

	/* Make kobjects in the virtual device for storing statistics */

	kgsl_driver.ptkobj =
	  kobject_create_and_add("pagetables",
				 &kgsl_driver.virtdev.kobj);

	kgsl_driver.prockobj =
		kobject_create_and_add("proc",
				       &kgsl_driver.virtdev.kobj);

	kgsl_core_debugfs_init();

	kgsl_sharedmem_init_sysfs();
	kgsl_cffdump_init();

	INIT_LIST_HEAD(&kgsl_driver.process_list);

	INIT_LIST_HEAD(&kgsl_driver.pagetable_list);

	kgsl_mmu_set_mmutype(ksgl_mmu_type);

	if (KGSL_MMU_TYPE_GPU == kgsl_mmu_get_mmutype()) {
		result = kgsl_ptdata_init();
		if (result)
			goto err;
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
	}

	result = kgsl_drm_init(NULL);

	if (result)
		goto err;

	return 0;

err:
	kgsl_core_exit();
	return result;
}

<<<<<<< HEAD
module_init(kgsl_mod_init);
module_exit(kgsl_mod_exit);

MODULE_AUTHOR("QUALCOMM");
MODULE_DESCRIPTION("3D graphics driver for QSD8x50 and MSM7x27");
MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:kgsl");
=======
module_init(kgsl_core_init);
module_exit(kgsl_core_exit);

MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("MSM GPU driver");
MODULE_LICENSE("GPL");
>>>>>>> 5f54204... Update KGSL and add genlock, kgsl and genlock
