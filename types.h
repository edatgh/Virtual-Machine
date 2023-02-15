
#ifndef __TYPES_H__
#define __TYPES_H__

#define WORD_SIZE sizeof(word_t)

typedef unsigned char byte_t;
typedef unsigned int  word_t;

typedef union _mem_word_t
{
	word_t w;
	byte_t bytes[WORD_SIZE];
} mem_word_t;

typedef enum
{
	MODE_REGISTER,
	MODE_MEMORY,
	MODE_IMMEDIATE,
	MODE_REGISTER_MEMORY,
	MODE_REGISTER_REGISTER,
	MODE_MEMORY_REGISTER,
	MODE_IMMEDIATE_MEMORY,
	MODE_IMMEDIATE_REGISTER
} address_mode_t;

#endif /* __TYPES_H__ */
