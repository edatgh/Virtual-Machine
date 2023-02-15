
/*
 *   Includes
 */
#include <stdio.h>
#include <stdlib.h>
#include "io.h"

/*
 *   Types
 */
struct _io_t
{
	word_t a;
};

/*
 *   Implementation
 */


io_t* io_init(void)
{
	io_t *io;


	io = (io_t *)malloc(sizeof(*io));
	if (io == NULL)
	{
		return NULL;
	}

	/*
	 *   Initialize IO state structure
	 */
	/* TODO */

	return io;
}

int io_free(io_t *io)
{
	if (io == NULL)
	{
		return -1;
	}

	/*
	 *   Free io state structure items
	 */
	/* TODO */

	free(io);

	return 0;
}

int io_read(io_t *io, void *buf, word_t size)
{
	if (io == NULL || buf == NULL)
	{
		return -1;
	}

	if (size == 0)
	{
		return -1;
	}

	/*
	 *   Do read...
	 */

	return 0;
}

int io_write(io_t *io, void *buf, word_t size)
{
	if (io == NULL || buf == NULL)
	{
		return -1;
	}

	if (size == 0)
	{
		return -1;
	}

	/*
	 *   Do write...
	 */

	return 0;
}
