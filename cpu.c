
/*
 *   Includes
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "mem.h"
#include "io.h"
#include "cpu.h"

/*
 *   Types
 */
typedef void (*executor_t)(cpu_t *cpu);
struct _cmd_t
{
	byte_t     opcode;
	executor_t exec;
};

typedef struct _cpu_flags_t
{
	byte_t halt;      /* Halt flag. If set, CPU must not fetch further commands from memory. */
	byte_t error;     /* Error flag. Some error appeared while executing command.            */
	byte_t equ;       /* Equivalence flag. Set depending on the last compare operation.      */
	byte_t greater;   /* Greater flag. Set after compare.                                    */
} cpu_flags_t;

typedef struct _cpu_register_t
{
	word_t data;
	word_t code;
} cpu_register_t;

typedef struct _cpu_registers_t
{
	cpu_register_t ip; /* Instruction pointer */
	cpu_register_t sp; /* Stack pointer */
	cpu_register_t g[16]; /* General purpose registers g0 - g15 */
} cpu_registers_t;

struct _cpu_t
{
	mem_t           *mem;      /* Memory resource available for the CPU        */
	io_t            *io;       /* Input/Output facility for the CPU            */
	cpu_flags_t     flags;     /* CPU state flags                              */
	cpu_registers_t registers; /* Set of CPU registers                         */
	cmd_t           *cmd_tbl;  /* Table of commands (pairs: opcode - executor) */
	word_t          nr_cmds;   /* Number of entries in the table of commands   */
};

static int cpu_mem_read_word(cpu_t *cpu, word_t addr, word_t *word)
{
	int        ret;
	word_t     byte;
	mem_word_t w1;
	mem_word_t w2;
	mem_word_t w3;
	int        i;
	int        j;


	if (cpu == NULL || word == 0)
	{
		return -1;
	}

	/*
	 *   If address is not word-aligned, we need more
	 *   sophisticated reading algorithm
	 */
	if (addr % WORD_SIZE != 0)
	{
		byte = addr % WORD_SIZE;
		addr = addr / WORD_SIZE;
		addr = addr * WORD_SIZE;

		/*
		 *   Read two words from memory
		 */
		ret  = mem_read(cpu->mem, addr, &w1.w);
		ret += mem_read(cpu->mem, addr + WORD_SIZE, &w2.w);
		if (ret < 0)
		{
			return -1;
		}

		/*
		 *   Fetch the requested word from the
		 *   two words
		 */
		for (i = 0, j = byte; i < WORD_SIZE && j < WORD_SIZE; i++, j++)
		{
			w3.bytes[i] = w1.bytes[j];
		}

		for (i = i, j = 0; i < WORD_SIZE && j < WORD_SIZE; i++, j++)
		{
			w3.bytes[i] = w2.bytes[j];
		}

		*word = w3.w;
	}
	else
	{
		ret = mem_read(cpu->mem, addr, word);
		if (ret == -1)
		{
			return -1;
		}
	}

	return 0;
}

static int cpu_mem_write_word(cpu_t *cpu, word_t addr, word_t word)
{
	int        ret;
	word_t     byte;
	mem_word_t w1;
	mem_word_t w2;
	mem_word_t w3;
	int        i;
	int        j;


	if (cpu == NULL)
	{
		return -1;
	}

	if (addr % WORD_SIZE != 0)
	{
		byte = addr % WORD_SIZE;
		addr = addr / WORD_SIZE;
		addr = addr * WORD_SIZE;

		ret  = mem_read(cpu->mem, addr, &w1.w);
		ret += mem_read(cpu->mem, addr + WORD_SIZE, &w2.w);
		if (ret < 0)
		{
			return -1;
		}

		w3.w = word;

		for (i = byte, j = 0; i < WORD_SIZE && j < WORD_SIZE; i++, j++)
		{
			w1.bytes[i] = w3.bytes[j];
		}

		for (i = 0, j = j; i < WORD_SIZE && j < WORD_SIZE; i++, j++)
		{
			w2.bytes[i] = w3.bytes[j];
		}

		ret  = mem_write(cpu->mem, addr, w1.w);
		ret += mem_write(cpu->mem, addr + WORD_SIZE, w2.w);
		if (ret < 0)
		{
			return -1;
		}
	}
	else
	{
		ret = mem_write(cpu->mem, addr, word);
		if (ret == -1)
		{
			return -1;
		}
	}

	return 0;
}

static int cpu_mem_read_byte(cpu_t *cpu, word_t addr, byte_t *byte)
{
	int        ret;
	mem_word_t word;


	if (cpu == NULL || byte == NULL)
	{
		return -1;
	}

	ret = cpu_mem_read_word(cpu, addr, &word.w);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return -1;
	}

	*byte = word.bytes[0];

	return 0;
}

static int cpu_mem_write_byte(cpu_t *cpu, word_t addr, byte_t byte)
{
	mem_word_t word;
	int        ret;


	if (cpu == NULL)
	{
		return -1;
	}

	ret = cpu_mem_read_word(cpu, addr, &word.w);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return -1;
	}

	word.bytes[0] = byte;

	ret = cpu_mem_write_word(cpu, addr, word.w);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return -1;
	}

	return 0;
}

static int cpu_reg_read(cpu_t *cpu, word_t code, word_t *data)
{
	word_t r;


	for (r = 0; r < 0x10; r++)
	{
		if (cpu->registers.g[r].code == code)
		{
			*data = cpu->registers.g[r].data;
		}
	}

	return 0;
}

static int cpu_reg_write(cpu_t *cpu, word_t code, word_t data)
{
	word_t r;


	for (r = 0; r < 0x10; r++)
	{
		if (cpu->registers.g[r].code == code)
		{
			cpu->registers.g[r].data = data;
		}
	}

	return 0;
}

/*
 *   Command executors
 */
static void add(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t op2;
	word_t reg;
	word_t aux;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1 + 4, &op2);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op1, &reg);

		aux = reg + aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_REGISTER_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu_reg_read(cpu, op2, &aux);
		aux = reg + aux;
		cpu_reg_write(cpu, op2, aux);
		break;

	case MODE_MEMORY_REGISTER:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op2, &reg);
		reg = aux + reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	case MODE_IMMEDIATE_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		aux = op1 + aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_IMMEDIATE_REGISTER:
		cpu_reg_read(cpu, op2, &reg);
		reg = op1 + reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */

	cpu->registers.ip.data += (1 + 1 + 4 + 4);
}

static void sub(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t op2;
	word_t reg;
	word_t aux;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1 + 4, &op2);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op1, &reg);

		aux = reg - aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_REGISTER_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu_reg_read(cpu, op2, &aux);
		aux = reg - aux;
		cpu_reg_write(cpu, op2, aux);
		break;

	case MODE_MEMORY_REGISTER:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op2, &reg);
		reg = aux - reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	case MODE_IMMEDIATE_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		aux = op1 - aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_IMMEDIATE_REGISTER:
		cpu_reg_read(cpu, op2, &reg);
		reg = op1 - reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */

	cpu->registers.ip.data += (1 + 1 + 4 + 4);
}

static void jump(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t aux;
	word_t reg;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu->registers.ip.data = reg;
		break;

	case MODE_MEMORY:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}

		cpu->registers.ip.data = aux;
		break;

	case MODE_IMMEDIATE:
		cpu->registers.ip.data = op1;
		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */
}

static void halt(cpu_t *cpu)
{
	if (cpu == NULL)
	{
		return;
	}

	cpu->flags.halt = 1;
}

static void mov(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t op2;
	word_t aux;
	word_t reg;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1 + 4, &op2);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER_MEMORY:
		cpu_reg_read(cpu, op1, &aux);

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_REGISTER_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu_reg_write(cpu, op2, reg);
		break;

	case MODE_MEMORY_REGISTER:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_write(cpu, op2, aux);
		break;

	case MODE_IMMEDIATE_MEMORY:
		aux = op1;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_IMMEDIATE_REGISTER:
		cpu_reg_write(cpu, op2, op1);
		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */

	cpu->registers.ip.data += (1 + 1 + 4 + 4);
}

static void cmp(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t op2;
	word_t aux;
	word_t reg;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1 + 4, &op2);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER_MEMORY:
		cpu_reg_read(cpu, op1, &reg);

		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}

		if (reg == aux)
		{
			cpu->flags.equ     = 1;
			cpu->flags.greater = 0;
		}
		else if (reg < aux)
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 0;
		}
		else
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 1;
		}
		break;

	case MODE_REGISTER_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu_reg_read(cpu, op2, &aux);

		if (reg == aux)
		{
			cpu->flags.equ     = 1;
			cpu->flags.greater = 0;
		}
		else if (reg < aux)
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 0;
		}
		else
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 1;
		}

		break;

	case MODE_MEMORY_REGISTER:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op2, &reg);

		if (reg == aux)
		{
			cpu->flags.equ     = 1;
			cpu->flags.greater = 0;
		}
		else if (aux < reg)
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 0;
		}
		else
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 1;
		}

		break;

	case MODE_IMMEDIATE_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}

		if (op1 == aux)
		{
			cpu->flags.equ     = 1;
			cpu->flags.greater = 0;
		}
		else if (op1 < aux)
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 0;
		}
		else
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 1;
		}

		break;

	case MODE_IMMEDIATE_REGISTER:
		cpu_reg_read(cpu, op2, &reg);

		if (op1 == reg)
		{
			cpu->flags.equ     = 1;
			cpu->flags.greater = 0;
		}
		else if (op1 < reg)
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 0;
		}
		else
		{
			cpu->flags.equ     = 0;
			cpu->flags.greater = 1;
		}

		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */

	cpu->registers.ip.data += (1 + 1 + 4 + 4);
}

static void jg(cpu_t *cpu)
{
	if (cpu == NULL)
	{
		return;
	}

	if (cpu->flags.greater == 0)
	{
		cpu->registers.ip.data += (1 + 1 + 4);
		return;
	}
	else
	{
		jump(cpu);
	}
}

static void je(cpu_t *cpu)
{
	if (cpu == NULL)
	{
		return;
	}

	if (cpu->flags.equ == 0)
	{
		cpu->registers.ip.data += (1 + 1 + 4);
		return;
	}
	else
	{
		jump(cpu);
	}
}

static void my_mul(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t op2;
	word_t reg;
	word_t aux;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1 + 4, &op2);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op1, &reg);

		aux = reg * aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_REGISTER_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu_reg_read(cpu, op2, &aux);
		aux = reg * aux;
		cpu_reg_write(cpu, op2, aux);
		break;

	case MODE_MEMORY_REGISTER:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op2, &reg);
		reg = aux * reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	case MODE_IMMEDIATE_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		aux = op1 * aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_IMMEDIATE_REGISTER:
		cpu_reg_read(cpu, op2, &reg);
		reg = op1 * reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */

	cpu->registers.ip.data += (1 + 1 + 4 + 4);
}

static void my_div(cpu_t *cpu)
{
	int    ret;
	byte_t am;
	word_t op1;
	word_t op2;
	word_t reg;
	word_t aux;


	if (cpu == NULL)
	{
		return;
	}

	ret = cpu_mem_read_byte(cpu, cpu->registers.ip.data + 1, &am);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1, &op1);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data + 1 + 1 + 4, &op2);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return;
	}

	switch (am)
	{
	case MODE_REGISTER_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op1, &reg);

		aux = reg / aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_REGISTER_REGISTER:
		cpu_reg_read(cpu, op1, &reg);
		cpu_reg_read(cpu, op2, &aux);
		aux = reg / aux;
		cpu_reg_write(cpu, op2, aux);
		break;

	case MODE_MEMORY_REGISTER:
		ret = cpu_mem_read_word(cpu, op1, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		cpu_reg_read(cpu, op2, &reg);
		reg = aux / reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	case MODE_IMMEDIATE_MEMORY:
		ret = cpu_mem_read_word(cpu, op2, &aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		aux = op1 / aux;

		ret = cpu_mem_write_word(cpu, op2, aux);
		if (ret == -1)
		{
			cpu->flags.error = 1;
			return;
		}
		break;

	case MODE_IMMEDIATE_REGISTER:
		cpu_reg_read(cpu, op2, &reg);
		reg = op1 / reg;
		cpu_reg_write(cpu, op2, reg);
		break;

	default:
		cpu->flags.error = 1;
		break;
	} /* switch */

	cpu->registers.ip.data += (1 + 1 + 4 + 4);
}

/*
 *   Implementations (CPU)
 */

cpu_t* cpu_init(mem_t *mem, io_t *io)
{
	cpu_t  *cpu;
	word_t rc;


	if (mem == NULL || io == NULL)
	{
		return NULL;
	}

	cpu = (cpu_t *)malloc(sizeof(*cpu));
	if (cpu == NULL)
	{
		return NULL;
	}

	/*
	 *   Initialize the CPU state structure
	 */
	cpu->mem  = mem;
	cpu->io   = io;

	/*
	 *   Initialize flags
	 */
	memset(&cpu->flags, 0, sizeof(cpu->flags));

	/*
	 *   Initialize registers
	 */
	memset(&cpu->registers, 0, sizeof(cpu->registers));

	/*
	 *   Assign register codes
	 */
	cpu->registers.ip.code = 0x00;
	cpu->registers.sp.code = 0x01;
	for (rc = 0x00; rc < 0x10; rc++)
	{
		cpu->registers.g[rc].code = 0x02 + rc;
	}

	cpu->cmd_tbl = (cmd_t *)malloc(sizeof(cmd_t) * NR_COMMANDS);
	if (cpu->cmd_tbl == NULL)
	{
		free(cpu);
		return NULL;
	}

	cpu->nr_cmds = NR_COMMANDS;

	cpu->cmd_tbl[0].opcode = 0x01;
	cpu->cmd_tbl[0].exec   = add;

	cpu->cmd_tbl[1].opcode = 0x02;
	cpu->cmd_tbl[1].exec   = sub;

	cpu->cmd_tbl[2].opcode = 0x03;
	cpu->cmd_tbl[2].exec   = jump;

	cpu->cmd_tbl[3].opcode = 0x04;
	cpu->cmd_tbl[3].exec   = halt;

	cpu->cmd_tbl[4].opcode = 0x05;
	cpu->cmd_tbl[4].exec   = mov;

	cpu->cmd_tbl[5].opcode = 0x06;
	cpu->cmd_tbl[5].exec   = cmp;

	cpu->cmd_tbl[6].opcode = 0x07;
	cpu->cmd_tbl[6].exec   = jg;

	cpu->cmd_tbl[7].opcode = 0x08;
	cpu->cmd_tbl[7].exec   = je;

	cpu->cmd_tbl[8].opcode = 0x09;
	cpu->cmd_tbl[8].exec   = my_mul;

	cpu->cmd_tbl[9].opcode = 0x0a;
	cpu->cmd_tbl[9].exec   = my_div;

	return cpu;
}

int cpu_free(cpu_t *cpu)
{
	if (cpu == NULL)
	{
		return -1;
	}

	/*
	 *   Free CPU state structure items
	 */
	/* TODO */

	free(cpu);

	return 0;
}

int cpu_poweron(cpu_t *cpu)
{
	if (cpu == NULL)
	{
		return -1;
	}

	return 0;
}

int cpu_load_code(cpu_t *cpu, word_t addr, byte_t *code, word_t size)
{
	word_t i;


	for (i = 0; i < size; i++)
	{
		cpu_mem_write_byte(cpu, addr + i, code[i]);
	}

	return 0;
}

int cpu_run(cpu_t *cpu)
{
	int ret;


	if (cpu == NULL)
	{
		return -1;
	}

	while (!cpu->flags.halt)
	{
		ret = cpu_next_command(cpu);
		if (ret == -1)
		{
			return -1;
		}
	}

	cpu->flags.halt = 1;

	return 0;
}

int cpu_next_command(cpu_t *cpu)
{
	int        i;
	mem_word_t word;
	byte_t     opcode;
	int        ret;


	if (cpu == NULL)
	{
		return -1;
	}

	/*
	 *   Fetch a command (opcode) from memory
	 */
	ret = cpu_mem_read_word(cpu, cpu->registers.ip.data, &word.w);
	if (ret == -1)
	{
		cpu->flags.error = 1;
		return -1;
	}

	opcode = word.bytes[0];

	/*
	 *   Find opcode's executer
	 */
	for (i = 0; i < cpu->nr_cmds; i++)
	{
		if (cpu->cmd_tbl[i].opcode == opcode)
		{
			break;
		}
	}

	/*
	 *   Command with the opcode was not found...
	 */
	if (i == cpu->nr_cmds)
	{
		cpu->flags.error = 1;
		return -1;
	}

	/*
	 *   Execute the command
	 */
	cpu->cmd_tbl[i].exec(cpu);

	return 0;
}

int cpu_get_ip(cpu_t *cpu, word_t *ip)
{
	if (cpu == NULL)
	{
		return -1;
	}

	*ip = cpu->registers.ip.data;

	return 0;
}

int cpu_dump(cpu_t *cpu)
{
	word_t r;


	if (cpu == NULL)
	{
		return -1;
	}

	printf("----------------  CPU   ----------------\n");
	printf("Flags:\n");
	printf("\tHALT   : 0x%02x\n", cpu->flags.halt);
	printf("\tERROR  : 0x%02x\n", cpu->flags.error);
	printf("\tEQU    : 0x%02x\n", cpu->flags.equ);
	printf("\tGREATER: 0x%02x\n", cpu->flags.greater);
	printf("Registers:\n");
	printf("\tIP: 0x%08x\n", cpu->registers.ip.data);
	for (r = 0x0; r < 0x10; r++)
	{
		printf("\tg%d: 0x%08x\n", r, cpu->registers.g[r].data);
	}

	return 0;
}
