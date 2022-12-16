#define PROT_READ	0x1		/* page can be read */
#define PROT_WRITE	0x2		/* page can be written */

#define MAP_PRIVATE	    0x02
#define MAP_ANONYMOUS	0x20		/* don't use a file */

#include <sys/types.h>


void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
int munmap(void *addr, size_t length);