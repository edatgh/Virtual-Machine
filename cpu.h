
#ifndef __CPU_H__
#define __CPU_H__

/*
 *   Includes
 */
#include "types.h"
#include "mem.h"
#include "io.h"

/*
 *   Constants
 */
#define NR_COMMANDS 10

/*
 *   Types
 */
typedef struct _cpu_t cpu_t;
typedef struct _cmd_t cmd_t;

/*
 *   Prototypes (CPU interface)
 */
cpu_t* cpu_init        (mem_t *mem, io_t *io);
int    cpu_free        (cpu_t *cpu);
int    cpu_poweron     (cpu_t *cpu);
int    cpu_load_code   (cpu_t *cpu, word_t addr, byte_t *code, word_t size);
int    cpu_run         (cpu_t *cpu);
int    cpu_next_command(cpu_t *cpu);
int    cpu_get_ip      (cpu_t *cpu, word_t *ip);
int    cpu_dump        (cpu_t *cpu);

#endif /* __CPU_H__ */
