#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

//#define DEBUG

#define	PCIEHID_DEVICE	"/dev/pciehid/0"

//#define	NMAX		(4)

int main(int argc,char **argv)
{
	int fd, rc, i, j;

	if ((fd=open(PCIEHID_DEVICE,O_RDONLY)) <0) {
		fprintf(stderr,"cannot open %s\n",PCIEHID_DEVICE);
		return -1;
	}

	close(fd);


	return 0;
}
