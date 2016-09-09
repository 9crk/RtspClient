/*
 * =====================================================================================
 *
 *       Filename:  rtpc.c
 *
 *    Description:  this file is used to get a rtp package
 *
 *        Version:  1.0
 *        Created:  2017-03-15 09:44:10 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:   (), 
 *        Company:  
 *
 * =====================================================================================
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <fcntl.h> 
#include <pthread.h>
#include <sys/ipc.h> 
#include <sys/msg.h>
#include <netinet/if_ether.h>
#include <net/if.h>

#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#define __PACKED__        __attribute__ ((__packed__)) 
typedef enum
{
	RTSP_VIDEO=0,
	RTSP_VIDEOSUB=1,
	RTSP_AUDIO=2,
	RTSP_YUV422=3,
	RTSP_RGB=4,
	RTSP_VIDEOPS=5,
	RTSP_VIDEOSUBPS=6
}enRTSP_MonBlockType;

struct _RTP_FIXED_HEADER
{
    /**//* byte 0 */
    unsigned char csrc_len:4;        /**//* expect 0 */
    unsigned char extension:1;        /**//* expect 1, see RTP_OP below */
    unsigned char padding:1;        /**//* expect 0 */
    unsigned char version:2;        /**//* expect 2 */
    /**//* byte 1 */
    unsigned char payload:7;        /**//* RTP_PAYLOAD_RTSP */
    unsigned char marker:1;        /**//* expect 1 */
    /**//* bytes 2, 3 */
    unsigned short seq_no;            
    /**//* bytes 4-7 */
    unsigned  long timestamp;        
    /**//* bytes 8-11 */
    unsigned long ssrc;            /**//* stream number is used here. */
} __PACKED__;

typedef struct _RTP_FIXED_HEADER RTP_FIXED_HEADER;

struct _NALU_HEADER
{
    //byte 0
	unsigned char TYPE:5;
    unsigned char NRI:2;
	unsigned char F:1;    
	
}__PACKED__; /**//* 1 BYTES */

typedef struct _NALU_HEADER NALU_HEADER;

struct _FU_INDICATOR
{
    	//byte 0
    unsigned char TYPE:5;
	unsigned char NRI:2; 
	unsigned char F:1;    
	
}__PACKED__; /**//* 1 BYTES */

typedef struct _FU_INDICATOR FU_INDICATOR;

struct _FU_HEADER
{
   	 //byte 0
    unsigned char TYPE:5;
	unsigned char R:1;
	unsigned char E:1;
	unsigned char S:1;    
} __PACKED__; /**//* 1 BYTES */
typedef struct _FU_HEADER FU_HEADER;

struct _AU_HEADER
{
    //byte 0, 1
    unsigned short au_len;
    //byte 2,3
    unsigned  short frm_len:13;  
    unsigned char au_index:3;
} __PACKED__; /**//* 1 BYTES */
typedef struct _AU_HEADER AU_HEADER;

#define SLICE_NORMAL	0	// 常规单包
#define SLICE_START 1		// 多包开始
#define SLICE_MID	2		// 多包中包
#define SLICE_END	3		// 多包尾包
typedef struct RTP_PACK
{
	char *rpt_buf;
	// 前包序，如果新来的包头包序比PreSeq大，则属于新包，如果一样则
	// 属于片包，需要Append到buf中，等待包结束，
	// 如果比PreSeq大，说明包来的顺序错了，抛弃该包
	int PreSeq;
	// 当前包到来时判定的buf的偏移地址，既如果是单包，则offset为0，如果是片包，
	// 则为上个包append到rtp_buf的位置，每个包在开头包到来的时候
	// 在rtp_buf头添加h264包头信息
	int offset;
	int size;
	unsigned  long timestamp;	// 每个包的timestamp，没啥用
}RTP_PACK_S;

static int AddH264Head(char *buf)
{
	char soh[12] = {0};
	soh[11] = 0x01;
	memcpy( buf, soh, sizeof( soh ));
	return 0;
	return sizeof( soh );
}

static int  AppendData( char *dst, int offset, char *buf, int size )
{
	memcpy( dst + offset, buf, size );
	return size;
}

static RTP_PACK_S  MyRTP;

int RTP_Init()
{
	memset( &MyRTP, 0, sizeof( RTP_PACK_S ));
	MyRTP.rpt_buf = malloc( 1024*1024 );
}

int RTP_Exit()
{
	if( MyRTP.rpt_buf )
		free(  MyRTP.rpt_buf );
}

typedef void * HANDLE;

int SaveAs()
{
	static int fd = 0;
	if( fd == 0 )
	{
		fd = open( "test0.264", O_RDWR | O_CREAT);
		if( fd < 0 )
		{ 
			fd = 0;
			return;
		}
	}
	write( fd , MyRTP.rpt_buf, MyRTP.offset );
}

int RTP_Send(HANDLE Vde)
{
	return;
	if( MyRTP.rpt_buf && MyRTP.offset > 12 )
		Hi264DecFrame( Vde, MyRTP.rpt_buf, MyRTP.offset );
}


// -1 Error
// 1 keep receive package
// 0 ok to Write
int ParseRtp(  char *Buffer, int BufferSize )
{
	if( BufferSize < sizeof( RTP_FIXED_HEADER ))
	{	
		printf("Package too small!\r\n");
		return -1;
	}
	int seq;
	RTP_FIXED_HEADER    *rtp_hdr;
	NALU_HEADER		*nalu_hdr;
	FU_INDICATOR	*fu_ind;
	FU_HEADER		*fu_hdr;
	char *packBuf;
	RTP_PACK_S *rtp_pack = &MyRTP;
	rtp_pack->size += BufferSize;
	int SliceSta;
	rtp_hdr = (RTP_FIXED_HEADER*)Buffer;
	rtp_pack->timestamp = ntohl(rtp_hdr->timestamp);
	nalu_hdr = (NALU_HEADER*)(Buffer+12);
	fu_ind = (FU_INDICATOR*)(Buffer+12);
	fu_hdr = (FU_HEADER*)(Buffer+13);
	seq = rtp_hdr->seq_no;
	if( fu_hdr->S == 1 )
		SliceSta = SLICE_START;
	else if( fu_hdr->E=1 )
		SliceSta = SLICE_END;
	else if( rtp_hdr->marker = 0 )
		SliceSta = SLICE_MID;
	else
		SliceSta = SLICE_NORMAL;

	if( seq > rtp_pack->PreSeq )
	{
		printf("New Package Comming.\r\n");
		rtp_pack->PreSeq = seq;
	}
	else if( seq == rtp_pack->PreSeq )
	{
		printf("Delay Package Coming\r\n");
	}else{
		printf("Package Comming too late! PreSeq:%d , New Seq:%d \r\n", rtp_pack->PreSeq, seq);
	}
	if( rtp_hdr->payload != 96 )
	{
		printf("It is not a H264 Package!\r\n");
		return -1;
	}
	if( SliceSta == SLICE_NORMAL )
	{
		rtp_pack->offset = AddH264Head(rtp_pack->rpt_buf);
		AppendData(rtp_pack->rpt_buf,rtp_pack->offset, Buffer + 13, BufferSize - 13);
		return 0;
		// ready to send frame directly
	}else if( SliceSta == SLICE_START )	{
		rtp_pack->offset = AddH264Head(rtp_pack->rpt_buf);
		rtp_pack->offset += AppendData(rtp_pack->rpt_buf, rtp_pack->offset, Buffer + 14, BufferSize - 14);
	}else if( SliceSta == SLICE_MID ){
		rtp_pack->offset += AppendData(rtp_pack->rpt_buf, rtp_pack->offset, Buffer + 14, BufferSize - 14);
	}else{
		rtp_pack->offset += AppendData(rtp_pack->rpt_buf, rtp_pack->offset, Buffer + 14, BufferSize - 14);
		return 0;
		// end, we append the data to the buf
		// ready to send to the frame
	}
	printf("Size:%d kb!\r\n", rtp_pack->size/1024 );
	return 1;
}
