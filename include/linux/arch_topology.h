/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ARCH_TOPOLOGY_H
#define _LINUX_ARCH_TOPOLOGY_H

#include <linux/cpumask.h>

/*
 * CPU topology related definitions. Architectures can implement
 * their own versions of these functions in order to provide
 * CPU topology information to the scheduler.
 */

struct cpumask;
struct sched_domain;

struct cpu_topology {
	int thread_id;
	int core_id;
	int package_id;
	int llc_id;
	int numa_id;
	int die_id;
};

DECLARE_PER_CPU(int, cpu_number);
DECLARE_PER_CPU(unsigned long, max_freq_scale);
DECLARE_PER_CPU(unsigned long, thermal_pressure);

struct cpu_topology *cpu_topology;
cpumask_var_t cpu_active_mask;
cpumask_var_t cpu_present_mask;
cpumask_var_t cpu_online_mask;
cpumask_var_t cpu_possible_mask;
cpumask_var_t cpu_isolated_map;
cpumask_var_t cpu_active_mask;

extern int topology_physical_package_id(int cpu);
extern int topology_core_id(int cpu);
extern int topology_core_cpumask(int cpu, struct cpumask *mask);
extern int topology_thread_cpumask(int cpu, struct cpumask *mask);
extern int topology_die_cpumask(int cpu, struct cpumask *mask);
extern int topology_cluster_cpumask(int cpu, struct cpumask *mask);
extern int topology_sibling_cpumask(int cpu, struct cpumask *mask);
extern int topology_l3_cpumask(int cpu, struct cpumask *mask);
extern int topology_package_cpumask(int cpu, struct cpumask *mask);
extern int topology_cluster_cpumask(int cpu, struct cpumask *mask);

static inline int topology_get_max_freq_scale(struct sched_domain *sd, int cpu)
{
	return per_cpu(max_freq_scale, cpu);
}

static inline unsigned long topology_get_thermal_pressure(int cpu)
{
	return per_cpu(thermal_pressure, cpu);
}

void arch_set_thermal_pressure(struct cpumask *cpus, unsigned long th_pressure);

static inline unsigned long topology_get_max_freq_scale(struct sched_domain *sd, int cpu)
{
	return per_cpu(max_freq_scale, cpu);
}

static inline unsigned long topology_get_thermal_pressure(int cpu)
{
	return per_cpu(thermal_pressure, cpu);
}

void arch_set_thermal_pressure(struct cpumask *cpus, unsigned long th_pressure);

static inline int topology_physical_package_id(int cpu)
{
	return 0;
}

static inline int topology_core_id(int cpu)
{
	return cpu;
}

static inline int topology_sibling_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_sibling_mask(cpu));
	return 0;
}

static inline int topology_core_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_core_mask(cpu));
	return 0;
}

static inline int topology_llc_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_llc_mask(cpu));
	return 0;
}

static inline int topology_package_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_package_mask(cpu));
	return 0;
}

static inline int topology_cluster_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_cluster_mask(cpu));
	return 0;
}

static inline int topology_die_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_die_mask(cpu));
	return 0;
}

static inline int topology_thread_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_thread_mask(cpu));
	return 0;
}

static inline int topology_cluster_cpumask(int cpu, struct cpumask *mask)
{
	cpumask_copy(mask, cpu_cluster_mask(cpu));
	return 0;
}

#endif /* _LINUX_ARCH_TOPOLOGY_H */
