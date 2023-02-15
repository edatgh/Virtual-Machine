
/*
 *   Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asm.h"
#include "cpu.h"
#include "mem.h"
#include "io.h"

/*
 *   Program entry point
 */
int main(int argc, char **argv)
{
	cpu_t  *cpu;
	mem_t  *mem;
	io_t   *io;
	int    ret;
	char   cmd[32];
	word_t addr;
	word_t size;
	word_t buf;
	word_t ip;
	byte_t *code;


	asm_assemble("code.text", &code, &size);

	mem = mem_init();
	if (mem == NULL)
	{
		printf("Unable to initialize memory\n");
		return -1;
	}

	io = io_init();
	if (io == NULL)
	{
		printf("Unable to initialize IO\n");
		mem_free(mem);
		return -1;
	}

	cpu = cpu_init(mem, io);
	if (cpu == NULL)
	{
		printf("Unable to initialize CPU\n");
		io_free(io);
		mem_free(mem);
		return -1;
	}

	printf("Power ON...");
	ret = cpu_poweron(cpu);
	if (ret == -1)
	{
		printf("FAILED\n");
		return -1;
	}
	else
	{
		printf("OK\n\n\n");
	}

	cpu_load_code(cpu, 0, code, size);

	free(code);

	/*
	 *   Simple shell
	 */
	printf("+-------------------------------------+\n");
	printf("| Welcome to the first Ed's VM shell! |\n");
	printf("+-------------------------------------+\n");

	while (1)
	{
		/*
		 *   Read user's command from shell
		 */
		printf("Enter command: ");
		scanf("%s", cmd);

		/*
		 *   Parse the command
		 */
		if (strcmp(cmd, "dump") == 0)
		{
			printf("Enter address (dec): ");
			scanf("%d", &addr);

			printf("Enter size (dec): ");
			scanf("%d", &size);

			mem_dump(mem, addr, size);
			cpu_dump(cpu);
		}
		else if (strcmp(cmd, "read") == 0)
		{
			printf("Enter address (dec): ");
			scanf("%d", &addr);

			mem_read(mem, addr, &buf);
			printf("Memory value at 0x%08x: 0x%08x\n", addr, buf);
		}
		else if (strcmp(cmd, "write") == 0)
		{
			printf("Enter address (dec): ");
			scanf("%d", &addr);

			printf("Enter value (hex): ");
			scanf("%x", &buf);

			mem_write(mem, addr, buf);

			printf("0x%08x ---> [%#x]\n", buf, addr);
		}
		else if (strcmp(cmd, "next") == 0)
		{
			cpu_get_ip(cpu, &ip);
			printf("Executing CPU command at [0x%08x]...", ip);
			ret = cpu_next_command(cpu);
			if (ret == -1)
			{
				printf("ERROR\n");
			}
			else
			{
				printf("OK\n");
			}
			mem_dump(mem, 0, 40);
			cpu_dump(cpu);
		}
		else if (strcmp(cmd, "run") == 0)
		{
			cpu_get_ip(cpu, &ip);
			printf("Running CPU at [0x%08x]...\n", ip);
			ret = cpu_run(cpu);
			if (ret == -1)
			{
				printf("ERROR: Unrecognized opcode at: [0x%08x]\n", ip);
			}
			else
			{
				printf("DONE\n");
				cpu_get_ip(cpu, &ip);
				printf("IP: [0x%08x]\n", ip);
			}
		}
		else if (strcmp(cmd, "quit") == 0)
		{
			printf("Bye.\n");
			break;
		}
		else if (strcmp(cmd, "help") == 0)
		{
			printf("Available commands:\n");
			printf("\tdump  - Make a dump of memory\n");
			printf("\tread  - Read some portion of memory\n");
			printf("\twrite - Write some value to memory\n");
			printf("\tnext  - Execute next CPU instruction\n");
			printf("\trun   - Execute program in memory\n");
			printf("\tquit  - Quit the shell\n");
			printf("\thelp  - This menu\n");
		}
		else
		{
			printf("Invalid command, use help\n");
		}
	}

	io_free(io);
	mem_free(mem);
	cpu_free(cpu);

	return 0;
}
