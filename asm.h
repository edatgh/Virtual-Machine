
#ifndef __ASM_H__
#define __ASM_H__

/*
 *   Includes
 */
#include "types.h"

/*
 *   Prototypes
 */
int asm_assemble(const char *file_name, byte_t **code, word_t *size);

#endif /* __ASM_H__ */
