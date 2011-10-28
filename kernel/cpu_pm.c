/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Author:
<<<<<<< HEAD
 *	Colin Cross <ccross <at> android.com>
=======
 *	Colin Cross <ccross@android.com>
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
<<<<<<< HEAD
=======
#include <linux/syscore_ops.h>
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm

static DEFINE_RWLOCK(cpu_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(cpu_pm_notifier_chain);

<<<<<<< HEAD
=======
static int cpu_pm_notify(enum cpu_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&cpu_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

/**
 * cpu_pm_register_notifier - register a driver with cpu_pm
 * @nb: notifier block to register
 *
 * Add a driver to a list of drivers that are notified about
 * CPU and CPU cluster low power entry and exit.
 *
 * This function may sleep, and has the same return conditions as
 * raw_notifier_chain_register.
 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
int cpu_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&cpu_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&cpu_pm_notifier_chain, nb);
	write_unlock_irqrestore(&cpu_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_register_notifier);

<<<<<<< HEAD
=======
/**
 * cpu_pm_unregister_notifier - unregister a driver with cpu_pm
 * @nb: notifier block to be unregistered
 *
 * Remove a driver from the CPU PM notifier list.
 *
 * This function may sleep, and has the same return conditions as
 * raw_notifier_chain_unregister.
 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
int cpu_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&cpu_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&cpu_pm_notifier_chain, nb);
	write_unlock_irqrestore(&cpu_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_unregister_notifier);

<<<<<<< HEAD
static int cpu_pm_notify(enum cpu_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&cpu_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

=======
/**
 * cpm_pm_enter - CPU low power entry notifier
 *
 * Notifies listeners that a single CPU is entering a low power state that may
 * cause some blocks in the same power domain as the cpu to reset.
 *
 * Must be called on the affected CPU with interrupts disabled.  Platform is
 * responsible for ensuring that cpu_pm_enter is not called twice on the same
 * CPU before cpu_pm_exit is called. Notified drivers can include VFP
 * co-processor, interrupt controller and it's PM extensions, local CPU
 * timers context save/restore which shouldn't be interrupted. Hence it
 * must be called with interrupts disabled.
 *
 * Return conditions are same as __raw_notifier_call_chain.
 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
int cpu_pm_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_PM_ENTER, -1, &nr_calls);
	if (ret)
<<<<<<< HEAD
=======
		/*
		 * Inform listeners (nr_calls - 1) about failure of CPU PM
		 * PM entry who are notified earlier to prepare for it.
		 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
		cpu_pm_notify(CPU_PM_ENTER_FAILED, nr_calls - 1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_enter);

<<<<<<< HEAD
=======
/**
 * cpm_pm_exit - CPU low power exit notifier
 *
 * Notifies listeners that a single CPU is exiting a low power state that may
 * have caused some blocks in the same power domain as the cpu to reset.
 *
 * Notified drivers can include VFP co-processor, interrupt controller
 * and it's PM extensions, local CPU timers context save/restore which
 * shouldn't be interrupted. Hence it must be called with interrupts disabled.
 *
 * Return conditions are same as __raw_notifier_call_chain.
 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
int cpu_pm_exit(void)
{
	int ret;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_PM_EXIT, -1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_exit);

<<<<<<< HEAD
=======
/**
 * cpm_cluster_pm_enter - CPU cluster low power entry notifier
 *
 * Notifies listeners that all cpus in a power domain are entering a low power
 * state that may cause some blocks in the same power domain to reset.
 *
 * Must be called after cpu_pm_enter has been called on all cpus in the power
 * domain, and before cpu_pm_exit has been called on any cpu in the power
 * domain. Notified drivers can include VFP co-processor, interrupt controller
 * and it's PM extensions, local CPU timers context save/restore which
 * shouldn't be interrupted. Hence it must be called with interrupts disabled.
 *
 * Must be called with interrupts disabled.
 *
 * Return conditions are same as __raw_notifier_call_chain.
 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
int cpu_cluster_pm_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_CLUSTER_PM_ENTER, -1, &nr_calls);
	if (ret)
<<<<<<< HEAD
=======
		/*
		 * Inform listeners (nr_calls - 1) about failure of CPU cluster
		 * PM entry who are notified earlier to prepare for it.
		 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
		cpu_pm_notify(CPU_CLUSTER_PM_ENTER_FAILED, nr_calls - 1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_cluster_pm_enter);

<<<<<<< HEAD
=======
/**
 * cpm_cluster_pm_exit - CPU cluster low power exit notifier
 *
 * Notifies listeners that all cpus in a power domain are exiting form a
 * low power state that may have caused some blocks in the same power domain
 * to reset.
 *
 * Must be called after cpu_pm_exit has been called on all cpus in the power
 * domain, and before cpu_pm_exit has been called on any cpu in the power
 * domain. Notified drivers can include VFP co-processor, interrupt controller
 * and it's PM extensions, local CPU timers context save/restore which
 * shouldn't be interrupted. Hence it must be called with interrupts disabled.
 *
 * Return conditions are same as __raw_notifier_call_chain.
 */
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
int cpu_cluster_pm_exit(void)
{
	int ret;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_CLUSTER_PM_EXIT, -1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_cluster_pm_exit);
<<<<<<< HEAD
=======

#ifdef CONFIG_PM
static int cpu_pm_suspend(void)
{
	int ret;

	ret = cpu_pm_enter();
	if (ret)
		return ret;

	ret = cpu_cluster_pm_enter();
	return ret;
}

static void cpu_pm_resume(void)
{
	cpu_cluster_pm_exit();
	cpu_pm_exit();
}

static struct syscore_ops cpu_pm_syscore_ops = {
	.suspend = cpu_pm_suspend,
	.resume = cpu_pm_resume,
};

static int cpu_pm_init(void)
{
	register_syscore_ops(&cpu_pm_syscore_ops);
	return 0;
}
core_initcall(cpu_pm_init);
#endif
>>>>>>> 1fdb24e... Merge branch 'devel-stable' of http://ftp.arm.linux.org.uk/pub/linux/arm/kernel/git-cur/linux-2.6-arm
