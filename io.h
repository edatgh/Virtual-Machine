
#ifndef __IO_H__
#define __IO_H__

/*
 *   Includes
 */
#include "types.h"

/*
 *   Types
 */
typedef struct _io_t io_t;

/*
 *   Prototypes
 */
io_t* io_init (void);
int   io_free (io_t *io);
int   io_read (io_t *io, void *buf, word_t size);
int   io_write(io_t *io, void *buf, word_t size);

#endif /* __IO_H__ */
