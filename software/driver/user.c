#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "tcam_lib.h"

//#define DEBUG

#define	TCAM_DEVICE	"/dev/tcam/0"

//#define	NMAX		(4)
#define BOARD_MAX	1
#define	NMAX		(32)
#define TCAM_MAX_PKT_BURST 32 // 48

int main(int argc,char **argv)
{
	struct tcam_board_conf *board = &tcam_boards[0];
	int fd, rc, i, j, dma_mode, driver_mode;
	int dma_channel, dma_queue_max;
	struct tc_buff160 tbuf[10];
	unsigned long long cmdp_start;
	unsigned long long resp_start;
	char buf[64];

	driver_mode = 0;
	dma_mode = 1;

	if (driver_mode) {
		if ((fd=open(TCAM_DEVICE,O_RDONLY)) <0) {
			fprintf(stderr,"cannot open %s\n",TCAM_DEVICE);
			return -1;
		}

/*
		cmdp_start = 0x10f00000;
		if ( ioctl( fd, TCAM_IOCTL_SET_CMDP_ADDR, &cmdp_start) < 0 ) {
			fprintf(stderr,"cannot IOCTL\n");
			return -2;
		}
*/

		if ( ioctl( fd, TCAM_IOCTL_GET_CMDP_ADDR, &cmdp_start) < 0 ) {
			fprintf(stderr,"cannot IOCTL\n");
			return -2;
		}
		printf("CMDP_START:%llX\n", cmdp_start);

		if ( ioctl( fd, TCAM_IOCTL_GET_RESP_ADDR, &resp_start) < 0 ) {
			fprintf(stderr,"cannot IOCTL\n");
			return -2;
		}
		printf("RESP_START:%llX\n", resp_start);
	
		if ( ioctl( fd, TCAM_IOCTL_SET_DMA_MODE, &dma_mode) < 0 ) {
			fprintf(stderr,"cannot IOCTL\n");
			return -2;
		}
		if ( ioctl( fd, TCAM_IOCTL_GET_DMA_MODE, &dma_mode) < 0 ) {
			fprintf(stderr,"cannot IOCTL\n");
			return -2;
		}
		printf("DMA_MODE:%X\n", dma_mode);
	} else {
		for (j=0; j<BOARD_MAX; ++j) {
			printf("Initialize board#=%d\n", j);
			if ( tcam_init( j, 0x10800000 + (0x100000 * j), ((TCAM_MAX_PKT_BURST+2)/3) * 0x40, 0x10000000 + (0x100000 * j), ((TCAM_MAX_PKT_BURST+2)/3) * 0x20 , &tcam_boards[j] ) < 0 ) {
				fprintf(stderr,"tcam_init error\n");
				return -3;
			}
			printf("DMA queue max:%d\n", tcam_get_dma_queue_max(&tcam_boards[j]));
			printf("\n");
		}
	}

	// initialize cmd descriptor
	for (j=0; j<BOARD_MAX; ++j) {
		tbuf[j].buf_count  = (TCAM_MAX_PKT_BURST+2)/3*3;
		tbuf[j].count      = NMAX;
		tbuf[j].enable_cam = 0;	// 0:CAM#0 1:CAM#1 2:CAM#0 and #1
		tbuf[j].fd         = 0;


		for (i=0; i < (NMAX+2)/3*3; ++i) {
			if (i<NMAX) {
				//tbuf[j].data[i][0] = 0x01010000 | ((i << 8) & 0xff);
				tbuf[j].data[i][0] =  (i << 8);
				tbuf[j].data[i][1] = 0x03030404;
				tbuf[j].data[i][2] = 0x05050606;
				tbuf[j].data[i][3] = 0x07070808;
				tbuf[j].data[i][4] = 0x09090a0a;
			} else {
				tbuf[j].data[i][0] = 0x0;
				tbuf[j].data[i][1] = 0x0;
				tbuf[j].data[i][2] = 0x0;
				tbuf[j].data[i][3] = 0x0;
				tbuf[j].data[i][4] = 0x0;
			}
		}
		tbuf[j].data[0][0] = 0xc0a80001;
	}

	if (driver_mode) {
		if ( ioctl( fd, TCAM_IOCTL_SET_KWS160, &tbuf) < 0 ) {
			fprintf(stderr,"cannot IOCTL\n");
			return -3;
		}
		if (dma_mode) {
			int i;
			if ( ioctl( fd, TCAM_IOCTL_GET_RESULT, buf) < 0 ) {
				fprintf(stderr,"cannot IOCTL\n");
				return -4;
			}
			for (i = 0; i<sizeof(buf); ++i)
				printf("%02x ", buf[i]);
		} else {
			if ( ioctl( fd, TCAM_IOCTL_GET_RESULT, &tbuf[0]) < 0 ) {
				fprintf(stderr,"cannot IOCTL\n");
				return -4;
			}
			for (i=0; i < NMAX; ++i) {
				printf("result#%d: %X\n", i, tbuf[0].result[i]);
			}
		}
	} else {
		for (j=0; j<BOARD_MAX; ++j) {
			unsigned char *p = (unsigned char *)&tcam_boards[j].bar_p[4];
			printf("board#=%d kws160_dma\n", j);
printf("user:%p,%d,%p\n", &tcam_boards[j], 0, &tbuf[j]);
			if (tcam_set_dma_queue_max(&tcam_boards[j], 16) < 0) {
				fprintf(stderr,"invalid parameter in tcam_set_dma_queue_max()\n");
				return -1;
			}
			for (dma_channel=0; dma_channel<tcam_get_dma_queue_max(&tcam_boards[j]); ++dma_channel) {
				printf("1回目:kws160_dma(dma channel=%d)\n", dma_channel);
				if ( tcam_kws160_dma( &tcam_boards[j], dma_channel, 0, &tbuf[j] ) < 0 ) {
					fprintf(stderr,"invalid parameter in tcam_kws160_dma()\n");
					return -1;
				}
				if ( tcam_kws160_dma_result( &tcam_boards[j], dma_channel, 0, &tbuf[j] ) < 0 ) {
					fprintf(stderr,"invalid parameter in tcam_kws160_dma_result()\n");
					return -1;
				}
				for (i=0; i < NMAX; ++i) {
					printf("#%02d: key=%08X, result= %X\n", i, tbuf[j].data[i][0], tbuf[0].result[i]);
				}
				for (i = 0; i<sizeof(buf); ++i)
					printf(" %02x", *(p+i));

//				sleep(5);

//				printf("\n2回目:kws160_dma(dma channel=%d)\n", dma_channel);
//				tcam_kws160_dma( &tcam_boards[j], dma_channel, 0, &tbuf[1] );
//				for (i = 0; i<sizeof(buf); ++i)
//					printf(" %02x", *(p+i));
				printf("\n\n");
			}
		}
	}

	close(fd);

	return 0;
}
