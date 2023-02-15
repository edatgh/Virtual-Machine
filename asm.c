
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "types.h"
#include "asm.h"

/*
 *   Types
 */
typedef enum
{ 
	ST_START,
	ST_COMMAND_OR_DEFINITION,
	ST_DEFINITION,
	ST_COMMAND,
	ST_OPERAND
} state_t;

typedef enum
{
	OP_REGISTER,
	OP_MEMORY,
	OP_IMMEDIATE
} operand_type_t;

typedef struct _cmd_t
{
	char   *mnemonic;
	byte_t opcode;
	int    nr_operands;
} cmd_t;

typedef struct _label_table_t
{
	char   *name;
	word_t addr;
} label_table_t;

typedef struct _unresolved_t
{
	char   *symbol;
	word_t offset;
} unresolved_t;

/*
 *   Command table
 */
static cmd_t commands[] =
{
	{ "add",   0x01, 2},
	{ "sub",   0x02, 2},
	{ "jump",  0x03, 1},
	{ "halt",  0x04, 0},
	{ "mov",   0x05, 2},
	{ "cmp",   0x06, 2},
	{ "jg",    0x07, 1},
	{ "je",    0x08, 1},
	{ "mul",   0x09, 2},
	{ "div",   0x0a, 2},
	{ NULL,           },
};

static label_table_t labels[100];
static int           label_cnt;

static unresolved_t  unresolved[100];
static int           unresolved_cnt;

/*
 *   Local utility functions
 */
static char* read_symbol(FILE *file)
{
	char *symbol;


	symbol = (char *)malloc(sizeof(char) * 32);
	if (symbol == NULL)
	{
		return NULL;
	}

	fscanf(file, "%s", symbol);

	return symbol;
}

/*
 *   Implementation
 */

static int is_command(const char *symbol)
{
	int i;


	for (i = 0; commands[i].mnemonic != NULL; i++)
	{
		if (strcmp(commands[i].mnemonic, symbol) == 0)
		{
			return 1;
		}
	}

	return 0;
}

static int is_decimal(const char *symbol)
{
	int i;


	for (i = 0; i < strlen(symbol); i++)
	{
		if (!isdigit(symbol[i]))
		{
			return 0;
		}
	}

	return 1;
}

static int is_hexadecimal(const char *symbol)
{
	int i;


	if (!(symbol[0] == '0' && symbol[1] == 'x'))
	{
		return 0;
	}

	for (i = 2; i < strlen(symbol); i++)
	{
		if (isdigit(symbol[i]) || (symbol[i] == 'a' ||
					   symbol[i] == 'b' ||
					   symbol[i] == 'c' ||
					   symbol[i] == 'd' ||
					   symbol[i] == 'e' ||
					   symbol[i] == 'f'))
		{
			continue;
		}
		else
		{
			break;
		}
	}

	if (i == strlen(symbol))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int is_number(const char *symbol)
{
	int ret;


	ret = (is_decimal(symbol) || is_hexadecimal(symbol));

	return ret;
}

static int is_definition(const char *symbol)
{
	if (strcmp(symbol, "byte") == 0 || strcmp(symbol, "word") == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int is_register(const char *symbol)
{
	word_t aux;


	aux = atoi(symbol + 1);
	if (symbol[0] == 'g' && aux >=0 && aux <= 15)
	{
		return 1;
	}

	return 0;
}

static int is_label(const char *symbol)
{
	
	if (isalpha(*symbol) && !is_register(symbol) && !is_command(symbol))
	{
		return 1;
	}

	return 0;
}

static int is_immlabel(const char *symbol)
{
	if (*symbol == '$' && is_label(symbol + 1))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int is_memory(const char *symbol)
{
	return is_number(symbol);
}

static int is_immediate(const char *symbol)
{
	if (*symbol == '$' && is_number(symbol + 1))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int is_operand(const char *symbol)
{
	return is_register(symbol) || is_memory(symbol) || is_immediate(symbol) || is_label(symbol) || is_immlabel(symbol);
}

static int sym2number(const char *symbol, word_t *word)
{
	int ret;


	ret = -1;

	if (is_decimal(symbol))
	{
		sscanf(symbol, "%d", word);
		ret = 0;
	}
	else if (is_hexadecimal(symbol))
	{
		sscanf(symbol + 2, "%x", word);
		ret = 0;
	}

	return ret;
}

static word_t register_code(const char *symbol)
{
	return 2 + atoi(symbol + 1);
}

static word_t memory_address(const char *symbol)
{
	word_t ret;


	sym2number(symbol, &ret);

	return ret;
}

static word_t immediate_value(const char *symbol)
{
	word_t ret;


	sym2number(symbol + 1, &ret);

	return ret;
}

static int definition_size(const char *symbol)
{
	int ret;


	if (strcmp(symbol, "byte") == 0)
	{
		ret = sizeof(byte_t);
	}
	else if (strcmp(symbol, "word") == 0)
	{
		ret = sizeof(word_t);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

static int label_create(const char *name, word_t addr)
{
	labels[label_cnt].name = strdup(name);
	labels[label_cnt].addr = addr;

	label_cnt++;

	return 0;
}

static int label_find(const char *name, word_t *addr)
{
	int i;


	for (i = 0; i < label_cnt; i++)
	{
		if (strcmp(labels[i].name, name) == 0)
		{
			*addr = labels[i].addr;
			return 0;
		}
	}

	return -1;
}

static int label_destroy(void)
{
	int i;


	for (i = 0; i < label_cnt; i++)
	{
		if (labels[i].name != NULL)
		{
			free(labels[i].name);
		}
	}

	return 0;
}

static int unresolved_add(const char *symbol, word_t offset)
{
	unresolved[unresolved_cnt].symbol = strdup(symbol);
	unresolved[unresolved_cnt].offset = offset;

	unresolved_cnt++;

	return 0;
}

static int unresolved_resolve(byte_t *code)
{
	int    i;
	int    ret;
	word_t addr;


	for (i = 0; i < unresolved_cnt; i++)
	{
		ret = label_find(unresolved[i].symbol, &addr);
		if (ret == -1)
		{
			return -1;
		}

		memcpy(code + unresolved[i].offset, &addr, sizeof(addr));
	}

	return 0;
}

static int unresolved_destroy(void)
{
	int i;


	for (i = 0; i < unresolved_cnt; i++)
	{
		if (unresolved[i].symbol != NULL)
		{
			free(unresolved[i].symbol);
		}
	}

	return 0;
}

static int command_find(const char *command, byte_t *opcode, int *nr_operands)
{
	int i;


	for (i = 0; commands[i].mnemonic != NULL; i++)
	{
		if (strcmp(commands[i].mnemonic, command) == 0)
		{
			*opcode      = commands[i].opcode;
			*nr_operands = commands[i].nr_operands;

			return 0;
		}
	}

	return -1;
}

int asm_assemble(const char *file_name, byte_t **code, word_t *size)
{
	FILE   *file;
	char   *symbol;
	word_t offset;
	byte_t opcode;
	int    nr_operands;
	state_t state;
	int     err;
	int     ret;
	word_t  fst_operand;
	operand_type_t fst_op_type;
	word_t  snd_operand;
	operand_type_t snd_op_type;
	word_t  operand_idx;
	byte_t  a_mode;
	word_t  number;
	word_t  def_size;
	byte_t  *p_code;


	if (code == NULL || size == NULL)
	{
		return -1;
	}

	*code = (byte_t *)malloc(sizeof(byte_t) * 1024);
	if (*code == NULL)
	{
		return -1;
	}

	p_code = *code;

	*size = 0;

	file = fopen(file_name, "r");
	if (file == NULL)
	{
		free(p_code);
		return -1;
	}

	offset = 0;
	ret    = 0;
	err    = 0;
	state  = ST_START;
	while (!feof(file) && !err)
	{
		symbol = read_symbol(file);
		if (symbol == NULL)
		{
			break;
		}

		/*
		 *   Skip empty symbols
		 */
		if (strcmp(symbol, "") == 0)
		{
			continue;
		}

		switch (state)
		{
		case ST_START:
			if (is_command(symbol))
			{
				if (command_find(symbol, &opcode, &nr_operands) == -1)
				{
					err = 1;
					break;
				}

				*p_code = opcode;
				(p_code)++;
				offset++;
				operand_idx = 0;
				if (nr_operands > 0)
				{
					state = ST_OPERAND;
				}
				else
				{
					state = ST_START;
				}
			}
			else if (is_label(symbol))
			{
				label_create(symbol, offset);
				state = ST_COMMAND_OR_DEFINITION;
			}
			else
			{
				err = 1;
			}
			break;

		case ST_COMMAND_OR_DEFINITION:
			if (is_definition(symbol))
			{
				def_size = definition_size(symbol);
				state    = ST_DEFINITION;
			}
			else if (is_command(symbol))
			{
				if (command_find(symbol, &opcode, &nr_operands) == -1)
				{
					err = 1;
					break;
				}

				*p_code = opcode;
				(p_code)++;
				offset++;
				operand_idx = 0;
				if (nr_operands > 0)
				{
					state = ST_OPERAND;
				}
				else
				{
					state = ST_START;
				}
			}
			else
			{
				err = 1;
			}
			break;

		case ST_DEFINITION:
			if (is_number(symbol))
			{
				sym2number(symbol, &number);
				memcpy(p_code, &number, def_size);
				(p_code) += def_size;
				offset   += def_size;
				state     = ST_START;
			}
			else
			{
				printf("Not a number: [%s]\n", symbol);
				err = 1;
			}
			break;

		case ST_COMMAND:
			if (is_command(symbol))
			{
				if (command_find(symbol, &opcode, &nr_operands) == -1)
				{
					err = 1;
					break;
				}

				*p_code = opcode;
				(p_code)++;
				offset++;
				operand_idx = 0;
				if (nr_operands > 0)
				{
					state = ST_OPERAND;
				}
				else
				{
					state = ST_START;
				}
			}
			else
			{
				err = 1;
			}
			break;

		case ST_OPERAND:
			if (is_operand(symbol))
			{
				if (nr_operands > 0)
				{

					switch (operand_idx)
					{
					case 0: /* reg, mem, imm */
						if (is_register(symbol))
						{
							fst_operand = register_code(symbol);
							fst_op_type = OP_REGISTER;
						}
						else if (is_memory(symbol) || is_label(symbol))
						{
							if (is_label(symbol))
							{
								unresolved_add(symbol, offset + 1);
								fst_operand = 0;
							}
							else
							{
								fst_operand = memory_address(symbol);
							}
							fst_op_type = OP_MEMORY;
						}
						else if (is_immediate(symbol) || is_immlabel(symbol))
						{
							if (is_immlabel(symbol))
							{
								unresolved_add(symbol + 1, offset + 1);
								fst_operand = 0;
							}
							else
							{
								fst_operand = immediate_value(symbol);
							}
							fst_op_type = OP_IMMEDIATE;
						}
						else
						{
							err = 1;
						}

						operand_idx++;
						nr_operands--;
						break;
					case 1: /* reg, mem */
						if (is_register(symbol))
						{
							snd_operand = register_code(symbol);
							snd_op_type = OP_REGISTER;
						}
						else if (is_memory(symbol) || is_label(symbol))
						{
							if (is_label(symbol))
							{
								unresolved_add(symbol, offset + 1);
								snd_operand = 0;
							}
							else
							{
								snd_operand = memory_address(symbol);
							}
							snd_op_type = OP_MEMORY;
						}
						else if (is_immlabel(symbol))
						{
							snd_op_type = OP_IMMEDIATE;
							err = 1;
						}
						else
						{
							err = 1;
						}

						operand_idx++;
						nr_operands--;
						break;
					default:
						err = 1;
						break;
					} /* switch */
				}

				if (nr_operands == 0)
				{
					/*
					 *   The command may have only one operand
					 */
					if (operand_idx == 1)
					{
						switch (fst_op_type)
						{
						case OP_REGISTER:
							a_mode = MODE_REGISTER;
							break;

						case OP_MEMORY:
							a_mode = MODE_MEMORY;
							break;

						case OP_IMMEDIATE:
							a_mode = MODE_IMMEDIATE;
							break;
						} /* switch */

						*p_code = a_mode;
						(p_code)++;
						memcpy(p_code, &fst_operand, sizeof(fst_operand));
						(p_code) += sizeof(fst_operand);
						offset += sizeof(fst_operand) + sizeof(a_mode);
					}
					else if (operand_idx == 2)
					{
						/*
						 *   Filter out incompatible operands
						 */
						if (fst_op_type == OP_MEMORY && snd_op_type == OP_MEMORY)
						{
							printf("Incompatible operands: Memory-memory\n");
							err = 1;
							break;
						}
						else if (snd_op_type == OP_IMMEDIATE)
						{
							printf("Second operand can't be immediate value\n");
							err = 1;
							break;
						}

						if (fst_op_type == OP_REGISTER)
						{
							if (snd_op_type == OP_REGISTER)
							{
								a_mode = MODE_REGISTER_REGISTER;
							}
							else if (snd_op_type == OP_MEMORY)
							{
								a_mode = MODE_REGISTER_MEMORY;
							}
						}
						else if (fst_op_type == OP_MEMORY)
						{
							if (snd_op_type == OP_REGISTER)
							{
								a_mode = MODE_MEMORY_REGISTER;
							}
						}
						else if (fst_op_type == OP_IMMEDIATE)
						{
							if (snd_op_type == OP_REGISTER)
							{
								a_mode = MODE_IMMEDIATE_REGISTER;
							}
							else if (snd_op_type == OP_MEMORY)
							{
								a_mode = MODE_IMMEDIATE_MEMORY;
							}
						}

						*p_code = a_mode;
						(p_code)++;
						memcpy(p_code, &fst_operand, sizeof(fst_operand));
						(p_code) += sizeof(fst_operand);
						memcpy(p_code, &snd_operand, sizeof(snd_operand));
						(p_code) += sizeof(snd_operand);
						offset += sizeof(fst_operand) + sizeof(snd_operand) + sizeof(a_mode);
					}
					else
					{
						err = 1;
					}

					state = ST_START;
				}
			}
			else
			{
				err = 1;
			}
			break;
		} /* switch */

		if (err)
		{
			ret = -1;
			printf("Assembly error at: [%s]\n", symbol);
			free(symbol);
			fclose(file);
			label_destroy();
			unresolved_destroy();
			return ret;
		}

		free(symbol);
	}

	fclose(file);

	*size = offset;

	ret = unresolved_resolve(*code);
	if (ret == -1)
	{
		printf("Code contains some unresolved symbols\n");
	}

	label_destroy();
	unresolved_destroy();

	return ret;
}
