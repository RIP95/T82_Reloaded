#undef DEBUG

/*
 * ARM performance counter support.
 *
 * Copyright (C) 2009 picoChip Designs, Ltd., Jamie Iles
 * Copyright (C) 2010 ARM Ltd., Will Deacon <will.deacon@arm.com>
 *
 * This code is based on the sparc64 perf event code, which is in turn based
 * on the x86 code. Callchain code is based on the ARM OProfile backtrace
 * code.
 */
#define pr_fmt(fmt) "hw perfevents: " fmt

#include <linux/bitmap.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/irq.h>

#include <asm/cputype.h>
#include <asm/irq.h>
#include <asm/irq_regs.h>
#include <asm/pmu.h>
#include <asm/stacktrace.h>
#include <linux/cpu_pm.h>

/*
 * ARMv6 supports a maximum of 3 events, starting from index 0. If we add
 * another platform that supports more, we need to increase this to be the
 * largest of all platforms.
 *
 * ARMv7 supports up to 32 events:
 *  cycle counter CCNT + 31 events counters CNT0..30.
 *  Cortex-A8 has 1+4 counters, Cortex-A9 has 1+6 counters.
 */
#define ARMPMU_MAX_HWEVENTS		32

static DEFINE_PER_CPU(struct perf_event * [ARMPMU_MAX_HWEVENTS], hw_events);
static DEFINE_PER_CPU(unsigned long [BITS_TO_LONGS(ARMPMU_MAX_HWEVENTS)], used_mask);
static DEFINE_PER_CPU(struct pmu_hw_events, cpu_hw_events);

<<<<<<< HEAD
	/*
	 * A 1 bit for an index indicates that the counter is actively being
	 * used.
	 */
	unsigned long		active_mask[BITS_TO_LONGS(ARMPMU_MAX_HWEVENTS)];
};
static DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events);

struct arm_pmu {
	enum arm_perf_pmu_ids id;
	const char	*name;
	irqreturn_t	(*handle_irq)(int irq_num, void *dev);
	void		(*enable)(struct hw_perf_event *evt, int idx);
	void		(*disable)(struct hw_perf_event *evt, int idx);
	int		(*get_event_idx)(struct cpu_hw_events *cpuc,
					 struct hw_perf_event *hwc);
	u32		(*read_counter)(int idx);
	void		(*write_counter)(int idx, u32 val);
	int             (*set_event_filter) (struct hw_perf_event *evt,
			struct perf_event_attr *attr);
	void		(*start)(void);
	void		(*stop)(void);
	void		(*reset)(void *);
	unsigned	(*cache_map)[PERF_COUNT_HW_CACHE_MAX]
				    [PERF_COUNT_HW_CACHE_OP_MAX]
				    [PERF_COUNT_HW_CACHE_RESULT_MAX];
	const unsigned	(*event_map)[PERF_COUNT_HW_MAX];
	int	(*request_pmu_irq)(int irq, irq_handler_t *irq_h);
	void	(*free_pmu_irq)(int irq);
	u32		raw_event_mask;
	int		num_events;
	u64		max_period;
};
=======
#define to_arm_pmu(p) (container_of(p, struct arm_pmu, pmu))
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm

/* Set at runtime when we know what CPU type we are. */
static struct arm_pmu *cpu_pmu;

enum arm_perf_pmu_ids
armpmu_get_pmu_id(void)
{
	int id = -ENODEV;

	if (cpu_pmu != NULL)
		id = cpu_pmu->id;

	return id;
}
EXPORT_SYMBOL_GPL(armpmu_get_pmu_id);

int
armpmu_get_max_events(void)
{
	int max_events = 0;

	if (cpu_pmu != NULL)
		max_events = cpu_pmu->num_events;

	return max_events;
}
EXPORT_SYMBOL_GPL(armpmu_get_max_events);

int perf_num_counters(void)
{
	return armpmu_get_max_events();
}
EXPORT_SYMBOL_GPL(perf_num_counters);

#define HW_OP_UNSUPPORTED		0xFFFF

#define C(_x) \
	PERF_COUNT_HW_CACHE_##_x

#define CACHE_OP_UNSUPPORTED		0xFFFF

static int
armpmu_map_cache_event(const unsigned (*cache_map)
				      [PERF_COUNT_HW_CACHE_MAX]
				      [PERF_COUNT_HW_CACHE_OP_MAX]
				      [PERF_COUNT_HW_CACHE_RESULT_MAX],
		       u64 config)
{
	unsigned int cache_type, cache_op, cache_result, ret;

	cache_type = (config >>  0) & 0xff;
	if (cache_type >= PERF_COUNT_HW_CACHE_MAX)
		return -EINVAL;

	cache_op = (config >>  8) & 0xff;
	if (cache_op >= PERF_COUNT_HW_CACHE_OP_MAX)
		return -EINVAL;

	cache_result = (config >> 16) & 0xff;
	if (cache_result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return -EINVAL;

	ret = (int)(*cache_map)[cache_type][cache_op][cache_result];

	if (ret == CACHE_OP_UNSUPPORTED)
		return -ENOENT;

	return ret;
}

static int
armpmu_map_event(const unsigned (*event_map)[PERF_COUNT_HW_MAX], u64 config)
{
	int mapping = (*event_map)[config];
	return mapping == HW_OP_UNSUPPORTED ? -ENOENT : mapping;
}

static int
armpmu_map_raw_event(u32 raw_event_mask, u64 config)
{
	return (int)(config & raw_event_mask);
}

static int map_cpu_event(struct perf_event *event,
			 const unsigned (*event_map)[PERF_COUNT_HW_MAX],
			 const unsigned (*cache_map)
					[PERF_COUNT_HW_CACHE_MAX]
					[PERF_COUNT_HW_CACHE_OP_MAX]
					[PERF_COUNT_HW_CACHE_RESULT_MAX],
			 u32 raw_event_mask)
{
	u64 config = event->attr.config;

	switch (event->attr.type) {
	case PERF_TYPE_HARDWARE:
		return armpmu_map_event(event_map, config);
	case PERF_TYPE_HW_CACHE:
		return armpmu_map_cache_event(cache_map, config);
	case PERF_TYPE_RAW:
		return armpmu_map_raw_event(raw_event_mask, config);
	}

	return -ENOENT;
}

int
armpmu_event_set_period(struct perf_event *event,
			struct hw_perf_event *hwc,
			int idx)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	s64 left = local64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int ret = 0;

	if (unlikely(left <= -period)) {
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (left > (s64)armpmu->max_period)
		left = armpmu->max_period;

	local64_set(&hwc->prev_count, (u64)-left);

	armpmu->write_counter(idx, (u64)(-left) & 0xffffffff);

	perf_event_update_userpage(event);

	return ret;
}

u64
armpmu_event_update(struct perf_event *event,
		    struct hw_perf_event *hwc,
		    int idx, int overflow)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	u64 delta, prev_raw_count, new_raw_count;

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = armpmu->read_counter(idx);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
			     new_raw_count) != prev_raw_count)
		goto again;

	new_raw_count &= armpmu->max_period;
	prev_raw_count &= armpmu->max_period;

	if (overflow)
		delta = armpmu->max_period - prev_raw_count + new_raw_count + 1;
	else
		delta = new_raw_count - prev_raw_count;

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);

	return new_raw_count;
}

static void
armpmu_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	/* Don't read disabled counters! */
	if (hwc->idx < 0)
		return;
	armpmu_event_update(event, hwc, hwc->idx, 0);
}

static void
armpmu_stop(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	/*
	 * ARM pmu always has to update the counter, so ignore
	 * PERF_EF_UPDATE, see comments in armpmu_start().
	 */
	if (!(hwc->state & PERF_HES_STOPPED)) {
		armpmu->disable(hwc, hwc->idx);
		barrier(); /* why? */
		armpmu_event_update(event, hwc, hwc->idx, 0);
		hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	}
}

static void
armpmu_start(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;

	/*
	 * ARM pmu always has to reprogram the period, so ignore
	 * PERF_EF_RELOAD, see the comment below.
	 */
	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;
	/*
	 * Set the period again. Some counters can't be stopped, so when we
	 * were stopped we simply disabled the IRQ source and the counter
	 * may have been left counting. If we don't do this step then we may
	 * get an interrupt too soon or *way* too late if the overflow has
	 * happened since disabling.
	 */
	armpmu_event_set_period(event, hwc, hwc->idx);
	armpmu->enable(hwc, hwc->idx);
}

static void
armpmu_del(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *hw_events = armpmu->get_hw_events();
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	WARN_ON(idx < 0);

	armpmu_stop(event, PERF_EF_UPDATE);
	hw_events->events[idx] = NULL;
	clear_bit(idx, hw_events->used_mask);

	perf_event_update_userpage(event);
}

static int
armpmu_add(struct perf_event *event, int flags)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *hw_events = armpmu->get_hw_events();
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	int err = 0;

	perf_pmu_disable(event->pmu);

	/* If we don't have a space for the counter then finish early. */
	idx = armpmu->get_event_idx(hw_events, hwc);
	if (idx < 0) {
		err = idx;
		goto out;
	}

	/*
	 * If there is an event in the counter we are going to use then make
	 * sure it is disabled.
	 */
	event->hw.idx = idx;
	armpmu->disable(hwc, idx);
	hw_events->events[idx] = event;

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	if (flags & PERF_EF_START)
		armpmu_start(event, PERF_EF_RELOAD);

	/* Propagate our changes to the userspace mapping. */
	perf_event_update_userpage(event);

out:
	perf_pmu_enable(event->pmu);
	return err;
}

static int
validate_event(struct pmu_hw_events *hw_events,
	       struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event fake_event = event->hw;
	struct pmu *leader_pmu = event->group_leader->pmu;

	if (event->pmu != leader_pmu || event->state <= PERF_EVENT_STATE_OFF)
		return 1;

	return armpmu->get_event_idx(hw_events, &fake_event) >= 0;
}

static int
validate_group(struct perf_event *event)
{
	struct perf_event *sibling, *leader = event->group_leader;
	struct pmu_hw_events fake_pmu;

	memset(&fake_pmu, 0, sizeof(fake_pmu));

	if (!validate_event(&fake_pmu, leader))
		return -ENOSPC;

	list_for_each_entry(sibling, &leader->sibling_list, group_entry) {
		if (!validate_event(&fake_pmu, sibling))
			return -ENOSPC;
	}

	if (!validate_event(&fake_pmu, event))
		return -ENOSPC;

	return 0;
}

static irqreturn_t armpmu_platform_irq(int irq, void *dev)
{
	struct arm_pmu *armpmu = (struct arm_pmu *) dev;
	struct platform_device *plat_device = armpmu->plat_device;
	struct arm_pmu_platdata *plat = dev_get_platdata(&plat_device->dev);

	return plat->handle_irq(irq, dev, armpmu->handle_irq);
}

static void
armpmu_release_hardware(struct arm_pmu *armpmu)
{
	int i, irq, irqs;
	struct platform_device *pmu_device = armpmu->plat_device;

	irqs = min(pmu_device->num_resources, num_possible_cpus());

	for (i = 0; i < irqs; ++i) {
		if (!cpumask_test_and_clear_cpu(i, &armpmu->active_irqs))
			continue;
		irq = platform_get_irq(pmu_device, i);
		if (irq >= 0)
			free_irq(irq, armpmu);
	}

	release_pmu(armpmu->type);
}

static int
<<<<<<< HEAD
armpmu_generic_request_irq(int irq, irq_handler_t *handle_irq)
{
	return request_irq(irq, *handle_irq,
			IRQF_DISABLED | IRQF_NOBALANCING,
			"armpmu", NULL);
}

static void
armpmu_generic_free_irq(int irq)
{
	if (irq >= 0)
		free_irq(irq, NULL);
}

static int
armpmu_reserve_hardware(void)
=======
armpmu_reserve_hardware(struct arm_pmu *armpmu)
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
{
	struct arm_pmu_platdata *plat;
	irq_handler_t handle_irq;
	int i, err, irq, irqs;
	struct platform_device *pmu_device = armpmu->plat_device;

	err = reserve_pmu(armpmu->type);
	if (err) {
		pr_warning("unable to reserve pmu\n");
		return err;
	}

	plat = dev_get_platdata(&pmu_device->dev);
	if (plat && plat->handle_irq)
		handle_irq = armpmu_platform_irq;
	else
		handle_irq = armpmu->handle_irq;

	irqs = min(pmu_device->num_resources, num_possible_cpus());
	if (irqs < 1) {
		pr_err("no irqs for PMUs defined\n");
		return -ENODEV;
	}

	for (i = 0; i < irqs; ++i) {
		err = 0;
		irq = platform_get_irq(pmu_device, i);
		if (irq < 0)
			continue;

<<<<<<< HEAD
		err = armpmu->request_pmu_irq(irq, &handle_irq);

=======
		/*
		 * If we have a single PMU interrupt that we can't shift,
		 * assume that we're running on a uniprocessor machine and
		 * continue. Otherwise, continue without this interrupt.
		 */
		if (irq_set_affinity(irq, cpumask_of(i)) && irqs > 1) {
			pr_warning("unable to set irq affinity (irq=%d, cpu=%u)\n",
				    irq, i);
			continue;
		}

		err = request_irq(irq, handle_irq,
				  IRQF_DISABLED | IRQF_NOBALANCING,
				  "arm-pmu", armpmu);
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
		if (err) {
			pr_err("unable to request IRQ%d for ARM PMU counters\n",
				irq);
			armpmu_release_hardware(armpmu);
			return err;
		}

<<<<<<< HEAD
	if (err) {
		for (i = i - 1; i >= 0; --i) {
			irq = platform_get_irq(pmu_device, i);

			armpmu->free_pmu_irq(irq);
		}
		release_pmu(pmu_device);
		pmu_device = NULL;
=======
		cpumask_set_cpu(i, &armpmu->active_irqs);
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
	}

	return 0;
}

static void
hw_perf_event_destroy(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	atomic_t *active_events	 = &armpmu->active_events;
	struct mutex *pmu_reserve_mutex = &armpmu->reserve_mutex;

<<<<<<< HEAD
	for (i = pmu_device->num_resources - 1; i >= 0; --i) {
		irq = platform_get_irq(pmu_device, i);
		armpmu->free_pmu_irq(irq);
	}
	armpmu->stop();

	release_pmu(pmu_device);
	pmu_device = NULL;
=======
	if (atomic_dec_and_mutex_lock(active_events, pmu_reserve_mutex)) {
		armpmu_release_hardware(armpmu);
		mutex_unlock(pmu_reserve_mutex);
	}
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
}

static int
event_requires_mode_exclusion(struct perf_event_attr *attr)
<<<<<<< HEAD
{
	return attr->exclude_idle || attr->exclude_user ||
		attr->exclude_kernel || attr->exclude_hv;
}

static atomic_t active_events = ATOMIC_INIT(0);
static DEFINE_MUTEX(pmu_reserve_mutex);

static void
hw_perf_event_destroy(struct perf_event *event)
=======
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
{
	return attr->exclude_idle || attr->exclude_user ||
	       attr->exclude_kernel || attr->exclude_hv;
}

static int
__hw_perf_event_init(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int mapping, err;

	mapping = armpmu->map_event(event);

	if (mapping < 0) {
		pr_debug("event %x:%llx not supported\n", event->attr.type,
			 event->attr.config);
		return mapping;
	}

<<<<<<< HEAD
=======
	/*
	 * We don't assign an index until we actually place the event onto
	 * hardware. Use -1 to signify that we haven't decided where to put it
	 * yet. For SMP systems, each core has it's own PMU so we can't do any
	 * clever allocation or constraints checking at this point.
	 */
	hwc->idx		= -1;
	hwc->config_base	= 0;
	hwc->config		= 0;
	hwc->event_base		= 0;

	/*
	 * Check whether we need to exclude the counter from certain modes.
	 */
	if ((!armpmu->set_event_filter ||
	     armpmu->set_event_filter(hwc, &event->attr)) &&
	     event_requires_mode_exclusion(&event->attr)) {
		pr_debug("ARM performance counters do not support "
			 "mode exclusion\n");
		return -EPERM;
	}
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm

	/*
	 * Store the event encoding into the config_base field.
	 */
<<<<<<< HEAD
	hwc->idx = -1;

	/*
	 * Store the event encoding into the config_base field. config and
	 * event_base are unused as the only 2 things we need to know are
	 * the event mapping and the counter to use. The counter to use is
	 * also the indx and the config_base is the event type.
	 */
	hwc->config_base = 0;
	hwc->config = 0;
	hwc->event_base = 0;

	if ((!armpmu->set_event_filter ||
		armpmu->set_event_filter(hwc, &event->attr)) &&
		event_requires_mode_exclusion(&event->attr)) {
		pr_debug("ARM performance counters do not support "
				"mode exclusion\n");
		return -EPERM;
	}

	hwc->config_base |= (unsigned long)mapping;
=======
	hwc->config_base	    |= (unsigned long)mapping;
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm

	if (!hwc->sample_period) {
		hwc->sample_period  = armpmu->max_period;
		hwc->last_period = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	err = 0;
	if (event->group_leader != event) {
		err = validate_group(event);
		if (err)
			return -EINVAL;
	}

	return err;
}

static int armpmu_event_init(struct perf_event *event)
{
	struct arm_pmu *armpmu = to_arm_pmu(event->pmu);
	int err = 0;
	atomic_t *active_events = &armpmu->active_events;

	if (armpmu->map_event(event) == -ENOENT)
		return -ENOENT;

	event->destroy = hw_perf_event_destroy;

	if (!atomic_inc_not_zero(active_events)) {
		mutex_lock(&armpmu->reserve_mutex);
		if (atomic_read(active_events) == 0)
			err = armpmu_reserve_hardware(armpmu);

		if (!err)
			atomic_inc(active_events);
		mutex_unlock(&armpmu->reserve_mutex);
	}

	if (err)
		return err;

	err = __hw_perf_event_init(event);
	if (err)
		hw_perf_event_destroy(event);

	return err;
}

static void armpmu_enable(struct pmu *pmu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);
	struct pmu_hw_events *hw_events = armpmu->get_hw_events();
	int enabled = bitmap_weight(hw_events->used_mask, armpmu->num_events);

	if (enabled)
		armpmu->start();
}

static void armpmu_disable(struct pmu *pmu)
{
	struct arm_pmu *armpmu = to_arm_pmu(pmu);
	armpmu->stop();
}

<<<<<<< HEAD
static void armpmu_update_counters(void)
{
	int idx;
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!armpmu)
		return;

	for (idx = 0; idx <= armpmu->num_events; ++idx) {
		struct perf_event *event = cpuc->events[idx];

		if (!event)
			continue;

		armpmu_read(event);
	}
}

static struct pmu pmu = {
	.pmu_enable	= armpmu_enable,
	.pmu_disable	= armpmu_disable,
	.event_init	= armpmu_event_init,
	.add		= armpmu_add,
	.del		= armpmu_del,
	.start		= armpmu_start,
	.stop		= armpmu_stop,
	.read		= armpmu_read,
};
=======
static void __init armpmu_init(struct arm_pmu *armpmu)
{
	atomic_set(&armpmu->active_events, 0);
	mutex_init(&armpmu->reserve_mutex);

	armpmu->pmu = (struct pmu) {
		.pmu_enable	= armpmu_enable,
		.pmu_disable	= armpmu_disable,
		.event_init	= armpmu_event_init,
		.add		= armpmu_add,
		.del		= armpmu_del,
		.start		= armpmu_start,
		.stop		= armpmu_stop,
		.read		= armpmu_read,
	};
}

int __init armpmu_register(struct arm_pmu *armpmu, char *name, int type)
{
	armpmu_init(armpmu);
	return perf_pmu_register(&armpmu->pmu, name, type);
}
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm

/* Include the PMU-specific implementations. */
#include "perf_event_xscale.c"
#include "perf_event_v6.c"
#include "perf_event_v7.c"
#include "perf_event_msm.c"
#include "perf_event_msm_l2.c"
#include "perf_event_msm_krait.c"
#include "perf_event_msm_krait_l2.c"

static int perf_cpu_pm_notifier(struct notifier_block *self, unsigned long cmd,
		void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		armpmu_update_counters();
		perf_pmu_disable(&pmu);
		break;

	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		if (armpmu && armpmu->reset)
			armpmu->reset(NULL);
		perf_pmu_enable(&pmu);

		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block perf_cpu_pm_notifier_block = {
	.notifier_call = perf_cpu_pm_notifier,
};

/*
 * Ensure the PMU has sane values out of reset.
 * This requires SMP to be available, so exists as a separate initcall.
 */
static int __init
cpu_pmu_reset(void)
{
	if (cpu_pmu && cpu_pmu->reset)
		return on_each_cpu(cpu_pmu->reset, NULL, 1);
	return 0;
}
arch_initcall(cpu_pmu_reset);

/*
 * PMU platform driver and devicetree bindings.
 */
static struct of_device_id armpmu_of_device_ids[] = {
	{.compatible = "arm,cortex-a9-pmu"},
	{.compatible = "arm,cortex-a8-pmu"},
	{.compatible = "arm,arm1136-pmu"},
	{.compatible = "arm,arm1176-pmu"},
	{},
};

static struct platform_device_id armpmu_plat_device_ids[] = {
	{.name = "arm-pmu"},
	{},
};

static int __devinit armpmu_device_probe(struct platform_device *pdev)
{
	cpu_pmu->plat_device = pdev;
	return 0;
}

static struct platform_driver armpmu_driver = {
	.driver		= {
		.name	= "arm-pmu",
		.of_match_table = armpmu_of_device_ids,
	},
	.probe		= armpmu_device_probe,
	.id_table	= armpmu_plat_device_ids,
};

static int __init register_pmu_driver(void)
{
	return platform_driver_register(&armpmu_driver);
}
device_initcall(register_pmu_driver);

static struct pmu_hw_events *armpmu_get_cpu_events(void)
{
	return &__get_cpu_var(cpu_hw_events);
}

static void __init cpu_pmu_init(struct arm_pmu *armpmu)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		struct pmu_hw_events *events = &per_cpu(cpu_hw_events, cpu);
		events->events = per_cpu(hw_events, cpu);
		events->used_mask = per_cpu(used_mask, cpu);
		raw_spin_lock_init(&events->pmu_lock);
	}
	armpmu->get_hw_events = armpmu_get_cpu_events;
	armpmu->type = ARM_PMU_DEVICE_CPU;
}

/*
 * CPU PMU identification and registration.
 */
static int __init
init_hw_perf_events(void)
{
	unsigned long cpuid = read_cpuid_id();
	unsigned long implementor = (cpuid & 0xFF000000) >> 24;
	unsigned long part_number = (cpuid & 0xFFF0);

	/* ARM Ltd CPUs. */
	if (0x41 == implementor) {
		switch (part_number) {
		case 0xB360:	/* ARM1136 */
		case 0xB560:	/* ARM1156 */
		case 0xB760:	/* ARM1176 */
			cpu_pmu = armv6pmu_init();
			break;
		case 0xB020:	/* ARM11mpcore */
			cpu_pmu = armv6mpcore_pmu_init();
			break;
		case 0xC080:	/* Cortex-A8 */
			cpu_pmu = armv7_a8_pmu_init();
			break;
		case 0xC090:	/* Cortex-A9 */
			cpu_pmu = armv7_a9_pmu_init();
			break;
		case 0xC050:	/* Cortex-A5 */
			cpu_pmu = armv7_a5_pmu_init();
			break;
		case 0xC0F0:	/* Cortex-A15 */
			cpu_pmu = armv7_a15_pmu_init();
			break;
		}
	/* Intel CPUs [xscale]. */
	} else if (0x69 == implementor) {
		part_number = (cpuid >> 13) & 0x7;
		switch (part_number) {
		case 1:
			cpu_pmu = xscale1pmu_init();
			break;
		case 2:
			cpu_pmu = xscale2pmu_init();
			break;
		}
	/* Qualcomm CPUs */
	} else if (0x51 == implementor) {
		switch (part_number) {
		case 0x00F0:    /* 8x50 & 7x30*/
			armpmu = armv7_scorpion_pmu_init();
			break;
		case 0x02D0:    /* 8x60 */
			armpmu = armv7_scorpionmp_pmu_init();
			scorpionmp_l2_pmu_init();
			break;
		case 0x0490:    /* 8960 sim */
		case 0x04D0:    /* 8960 */
			armpmu = armv7_krait_pmu_init();
			krait_l2_pmu_init();
			break;
		}
	}

	if (cpu_pmu) {
		pr_info("enabled with %s PMU driver, %d counters available\n",
			cpu_pmu->name, cpu_pmu->num_events);
		cpu_pmu_init(cpu_pmu);
		armpmu_register(cpu_pmu, "cpu", PERF_TYPE_RAW);
	} else {
		pr_info("no hardware support available\n");
	}

<<<<<<< HEAD
	perf_pmu_register(&pmu, "cpu", PERF_TYPE_RAW);

	cpu_pm_register_notifier(&perf_cpu_pm_notifier_block);

=======
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
	return 0;
}
early_initcall(init_hw_perf_events);

/*
 * Callchain handling code.
 */

/*
 * The registers we're interested in are at the end of the variable
 * length saved register structure. The fp points at the end of this
 * structure so the address of this struct is:
 * (struct frame_tail *)(xxx->fp)-1
 *
 * This code has been adapted from the ARM OProfile support.
 */
struct frame_tail {
	struct frame_tail __user *fp;
	unsigned long sp;
	unsigned long lr;
} __attribute__((packed));

/*
 * Get the return address for a single stackframe and return a pointer to the
 * next frame tail.
 */
static struct frame_tail __user *
user_backtrace(struct frame_tail __user *tail,
	       struct perf_callchain_entry *entry)
{
	struct frame_tail buftail;

	/* Also check accessibility of one struct frame_tail beyond */
	if (!access_ok(VERIFY_READ, tail, sizeof(buftail)))
		return NULL;
	if (__copy_from_user_inatomic(&buftail, tail, sizeof(buftail)))
		return NULL;

	perf_callchain_store(entry, buftail.lr);

	/*
	 * Frame pointers should strictly progress back up the stack
	 * (towards higher addresses).
	 */
	if (tail + 1 >= buftail.fp)
		return NULL;

	return buftail.fp - 1;
}

void
perf_callchain_user(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	struct frame_tail __user *tail;


	tail = (struct frame_tail __user *)regs->ARM_fp - 1;

	while ((entry->nr < PERF_MAX_STACK_DEPTH) &&
	       tail && !((unsigned long)tail & 0x3))
		tail = user_backtrace(tail, entry);
}

/*
 * Gets called by walk_stackframe() for every stackframe. This will be called
 * whist unwinding the stackframe and is like a subroutine return so we use
 * the PC.
 */
static int
callchain_trace(struct stackframe *fr,
		void *data)
{
	struct perf_callchain_entry *entry = data;
	perf_callchain_store(entry, fr->pc);
	return 0;
}

void
perf_callchain_kernel(struct perf_callchain_entry *entry, struct pt_regs *regs)
{
	struct stackframe fr;

	fr.fp = regs->ARM_fp;
	fr.sp = regs->ARM_sp;
	fr.lr = regs->ARM_lr;
	fr.pc = regs->ARM_pc;
	walk_stackframe(&fr, callchain_trace, entry);
}
