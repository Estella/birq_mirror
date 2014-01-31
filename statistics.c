/* stat_parse.c
 * Parse statistics files.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "statistics.h"
#include "cpu.h"
#include "irq.h"

#define STR(str) ( str ? str : "" )

void parse_proc_stat(lub_list_t *cpus, lub_list_t *irqs)
{
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	int cpunr, rc, cpucount;
	unsigned long long l_user;
	unsigned long long l_nice;
	unsigned long long l_system;
	unsigned long long l_idle;
	unsigned long long l_iowait;
	unsigned long long l_irq;
	unsigned long long l_softirq;
	unsigned long long l_steal;
	unsigned long long l_guest;
	unsigned long long l_guest_nice;
	unsigned long long load_irq, load_all;
	char *intr_str;
	char *saveptr;
	unsigned int inum = 0;

	file = fopen("/proc/stat", "r");
	if (!file) {
		fprintf(stderr, "Warning: Can't open /proc/stat. Balacing is broken.\n");
		return;
	}

	/* first line is the header we don't need; nuke it */
	if (getline(&line, &size, file) == 0) {
		free(line);
		fprintf(stderr, "Warning: Can't read /proc/stat. Balancing is broken.\n");
		fclose(file);
		return;
	}

	cpucount = 0;
	while (!feof(file)) {
		cpu_t *cpu;
		if (getline(&line, &size, file)==0)
			break;
		if (!strstr(line, "cpu"))
			break;
		cpunr = strtoul(&line[3], NULL, 10);

		cpu = cpu_list_search(cpus, cpunr);
		if (!cpu)
			continue;

		rc = sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			&l_user, &l_nice, &l_system, &l_idle, &l_iowait,
			&l_irq, &l_softirq, &l_steal, &l_guest, &l_guest_nice);
		if (rc < 2)
			break;
		cpucount++;

		load_all = l_user + l_nice + l_system + l_idle + l_iowait +
			l_irq + l_softirq + l_steal + l_guest + l_guest_nice;
		load_irq = l_irq + l_softirq;

		if (cpu->old_load_all == 0) {
			/* When old_load_all = 0 - it's first iteration */
			cpu->load = 0;
		} else {
			float d_all = (float)(load_all - cpu->old_load_all);
			float d_irq = (float)(load_irq - cpu->old_load_irq);
			cpu->load = d_irq * 100 / d_all;
		}

		cpu->old_load_all = load_all;
		cpu->old_load_irq = load_irq;

		printf("CPU %u %.2f%%\n", cpunr, cpu->load);
	}

	/* Parse "intr" line. Get number of interrupts. */
	intr_str = strtok_r(line, " ", &saveptr); /* String "intr" */
	intr_str = strtok_r(NULL, " ", &saveptr); /* Total number of interrupts */
	for (intr_str = strtok_r(NULL, " ", &saveptr);
		intr_str; intr_str = strtok_r(NULL, " ", &saveptr)) {
		unsigned long long intr = 0;
		char *endptr;
		irq_t *irq;
		
		irq = irq_list_search(irqs, inum);
		inum++;
		if (!irq)
			continue;
		intr = strtoull(intr_str, &endptr, 10);
		if (endptr == intr_str)
			intr = 0;
		if (irq->old_intr == 0)
			irq->intr = 0;
		else
			irq->intr = intr - irq->old_intr;
		irq->old_intr = intr;
		printf("IRQ %u %llu %s\n", irq->irq, irq->intr, irq->desc);
	}

	fclose(file);
	free(line);
}