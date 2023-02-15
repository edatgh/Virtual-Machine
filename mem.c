
/*
 *   Includes
 */
#include <stdlib.h>
#include <stdio.h>
#include "types.h"
#include "mem.h"

/*
 *   Types
 */
struct _mem_t
{
	word_t *words;
};

/*
 *   Implementation
 */


mem_t* mem_init(void)
{
	mem_t *mem;


	mem = (mem_t *)malloc(sizeof(*mem));
	if (mem == NULL)
	{
		return NULL;
	}

	mem->words = (word_t *)malloc(sizeof(word_t) * MEM_SIZE);
	if (mem->words == NULL)
	{
		free(mem);
		return NULL;
	}

	return mem;
}

int mem_free(mem_t *mem)
{
	if (mem == NULL)
	{
		return -1;
	}

	/*
	 *   Free memory state structure items
	 */
	free(mem->words);

	/*
	 *   Free memory state structure itself
	 */
	free(mem);

	return 0;
}

int mem_read(mem_t *mem, word_t addr, word_t *w)
{
	word_t     w_addr;


	if (mem == NULL || w == NULL)
	{
		return -1;
	}

	w_addr = addr / WORD_SIZE;
	*w     = mem->words[w_addr];

	return 0;
}

int mem_write(mem_t *mem, word_t addr, word_t w)
{
	word_t     w_addr;


	if (mem == NULL)
	{
		return -1;
	}

	w_addr             = addr / WORD_SIZE;
	mem->words[w_addr] = w;

	return 0;
}

int mem_dump(mem_t *mem, word_t addr, word_t size)
{
	word_t     i;
	word_t     j;
	mem_word_t word;


	if (mem == NULL)
	{
		return -1;
	}

	if (addr % WORD_SIZE != 0)
	{
		return -1;
	}

	printf("---------------- Memory ----------------\n");
	for (i = addr; i < size; i += 4)
	{
		word.w = mem->words[i / 4];

		printf("[0x%08x]: ", i);
		for (j = 0; j < WORD_SIZE; j++)
		{
			printf("%02x ", word.bytes[j]);
		}
		printf("\n");
	}

	return 0;
}
