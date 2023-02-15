
#ifndef __MEM_H__
#define __MEM_H__

/*
 *   Includes
 */
#include "types.h"

/*
 *   Constants
 */
#define MEM_SIZE 1024 /* in words */

/*
 *   Types
 */
typedef struct _mem_t mem_t;

/*
 *   Prototypes
 */
mem_t* mem_init (void);
int    mem_free (mem_t *mem);
int    mem_read (mem_t *mem, word_t addr, word_t *w);
int    mem_write(mem_t *mem, word_t addr, word_t w);
int    mem_dump (mem_t *mem, word_t addr, word_t size);

#endif /* __MEM_H__ */
