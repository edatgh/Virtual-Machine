
#include <stdio.h>
#include "asm.h"

int main(int argc, char *argv)
{
	byte_t *code;
	word_t size;
	word_t i;


	asm_assemble("code.text", &code, &size);
	printf("size == %lu\n", size);

	printf("---------- Code dump ------------\n");
	for (i = 0; i < size; i++)
	{
		printf("0x%02x\n", code[i]);
	}

	return 0;
}
