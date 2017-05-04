/*
 * =====================================================================================
 *
 *       Filename:  rtpc.c
 *
 *    Description:  this file is used to get a rtp package
 *     stand by url:  http://blog.csdn.net/chen495810242/article/details/39207305
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
// 500K 应该够了
#define MAX_RTP_BUFFER 1024*1000
typedef void * HANDLE;
#define __PACKED__        __attribute__ ((__packed__)) 
// 小端格式代码
typedef unsigned int HI_U32; 

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
    unsigned char version:2;        /**//* expect 2 */
    unsigned char padding:1;        /**//* expect 0 */
    unsigned char extension:1;        /**//* expect 1, see RTP_OP below */
    unsigned char csrc_len:4;        /**//* expect 0 */
    /**//* byte 0 */
    /**//* bytes 2, 3 */
    /**//* byte 1 */
    unsigned char marker:1;        /**//* expect 1 */
    unsigned char payload:7;        /**//* RTP_PAYLOAD_RTSP */
    unsigned short seq_no;            
    /**//* bytes 4-7 */
    HI_U32 timestamp;        
    /**//* bytes 8-11 */
    HI_U32 ssrc;            /**//* stream number is used here. */
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
    // byte 0
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

typedef enum tagNALU_E{
	NALU_SIGNEL,	// 单片包, 标准的NALU包
	NALU_MUTIL,		// 聚合包	// 一个包里有多个分片内容
	NALU_SLICE		// 分片包	// 多个小包拼接成大包
}NALU_E;

#define SLICE_NORMAL	0	// 常规单包
#define SLICE_START 1		// 多包开始
#define SLICE_MID	2		// 多包中包
#define SLICE_END	3		// 多包尾包

#define MAGIC_NUM	0xabcd1234
#define RTP_INVALID( obj ) ( obj->magic != MAGIC_NUM )

typedef struct RTP_PACK
{
	unsigned int magic;
	// 包序，没啥用
	int PreSeq;
	// 存放每次收到的RTP包
	char *buf;
	// 每次收到RTP包的时候都会计入offset偏移值里
	int offset;
	// 可以发送出去的帧buf
	char *frame;
	// 帧的长度，该frame里可能有多个小的帧信息
	int frame_offset;
	int size;
	// 本地包是否已经满了，满了的话在次进入时要重设偏移位置
	int hasFull;
	//这次收到的包类型
	char type;
	HI_U32 timestamp;	// 每个包的timestamp，没啥用
}RTP_PACK_S;

static int AddH264Head(char *buf, char type)
{
	unsigned char soh[10] = {0x00, 0x00, 0x0, 0x01, 0x65};
	soh[4] = type;
	memcpy( buf, soh, sizeof( soh ));
	return 5;
}

static int  AppendData( char *dst, int offset, char *buf, int size )
{
	memcpy( dst + offset, buf, size );
	return size;
}

HANDLE RTP_Init()
{
	RTP_PACK_S *obj = malloc( sizeof( RTP_PACK_S ));
	memset( obj, 0, sizeof( RTP_PACK_S ));
	obj->magic = MAGIC_NUM;
	obj->buf = malloc( MAX_RTP_BUFFER );	// 网络包小于1500
	obj->frame = malloc( MAX_RTP_BUFFER );	// 整包小于500K
	return obj;
}

int RTP_Exit( HANDLE handle)
{
	RTP_PACK_S *obj = (RTP_PACK_S*)handle;
	if( RTP_INVALID( obj ) )
	{
		printf("invalid obj !\r\n");
		return -1;
	}
	free( obj->buf );
	if( obj->frame );
		free( obj->frame );
}

int RTP_Send( HANDLE handle,HANDLE Vde)
{
	RTP_PACK_S *obj = (RTP_PACK_S*)handle;
	if( RTP_INVALID( obj ) )
	{
		printf("invlaid obj!\r\n");
		return -1;
	}
	if( Vde )
	{
		printf("Send Hex:%s \r\n", show_hex( obj->frame, 20 ));
		// you should add handle code here
//		Hi264DecFrame( Vde, obj->frame, obj->frame_offset, 0, NULL , 0 );
	}
	obj->frame_offset = 0;
}

static int Append_frame( RTP_PACK_S *obj,char type )
{
	if( type == 0x61 || type == 0x65 || type == 0x06)
	{
		// I帧或BP帧
		if( (obj->frame_offset + obj->offset ) > MAX_RTP_BUFFER )
		{
			printf("包已经超过了大小，抛弃该包!请设置最大包大小\r\n");
			obj->hasFull = 1;
			return 0;
		}
		memcpy( obj->frame + obj->frame_offset, obj->buf, obj->offset );
		obj->frame_offset += obj->offset;
		obj->hasFull = 1;
		return 1;
	}
	//帧头,不做发送处理，等待后续的数据帧
	memcpy( obj->frame + obj->frame_offset , obj->buf, obj->offset );
	obj->frame_offset += obj->offset;
	return 0;
}

int ParseRtp(  HANDLE handle , char *Buffer, int BufferSize )
{
	if( BufferSize < sizeof( RTP_FIXED_HEADER ))
	{	
		printf("Package too small!\r\n");
		return -1;
	}
	RTP_PACK_S *rtp_pack = (RTP_PACK_S*)handle;
	if( RTP_INVALID( rtp_pack ))
	{
		printf("rtp obj invalid!\r\n");
		return -1;
	}
	if( rtp_pack->hasFull )
	{
		rtp_pack->hasFull = 0;
		rtp_pack->frame_offset = 0;
		rtp_pack->offset = 0;
	}
	RTP_FIXED_HEADER    *rtp_hdr;
	NALU_HEADER		*nalu_hdr;
	FU_INDICATOR	*fu_ind;
	FU_HEADER		*fu_hdr;
	char *packBuf;
	unsigned char type;
	rtp_pack->size += BufferSize;
	int SliceSta;
	int offset = 12;
	rtp_hdr = (RTP_FIXED_HEADER*)Buffer;
	rtp_pack->timestamp = ntohl(rtp_hdr->timestamp);
	rtp_pack->PreSeq = ntohs(rtp_hdr->seq_no);
	NALU_E nalu_type = NALU_SIGNEL;
	nalu_hdr = (NALU_HEADER*)(Buffer+12);
	if( nalu_hdr->TYPE <= 23 )
		nalu_type = NALU_SIGNEL;
	else if( 24 <= nalu_hdr->TYPE && nalu_hdr->TYPE <= 27 )
		nalu_type = NALU_MUTIL;
	else if( 28 == nalu_hdr->TYPE || nalu_hdr->TYPE == 29 )
		nalu_type = NALU_SLICE;
	else
		printf("Unkow NALU packge!\r\n");
	if( nalu_type == NALU_SLICE )
	{
		// 如果是分片包，处理
		fu_ind = (FU_INDICATOR*)(Buffer+12);
		fu_hdr = (FU_HEADER*)(Buffer+13);
		if( fu_hdr->S == 1 )
			SliceSta = SLICE_START;
		else if( fu_hdr->E == 1 )
			SliceSta = SLICE_END;
		else if( rtp_hdr->marker == 0 )
			SliceSta = SLICE_MID;
		offset = 14;
		if( SliceSta == SLICE_START )	{
			type  = (Buffer[12] & 0xe0)  | (Buffer[13] & 0x1f );
			rtp_pack->type = type;
			rtp_pack->offset = 0;
			rtp_pack->offset = AddH264Head(rtp_pack->buf, type);
			rtp_pack->offset += AppendData(rtp_pack->buf, rtp_pack->offset, Buffer + offset, BufferSize - offset);
		}else if( SliceSta == SLICE_MID ){
			rtp_pack->offset += AppendData(rtp_pack->buf, rtp_pack->offset, Buffer + offset, BufferSize - offset);
		}else if( SliceSta == SLICE_END ) {
			rtp_pack->offset += AppendData(rtp_pack->buf, rtp_pack->offset, Buffer + offset, BufferSize - offset);
			// 仅在最后一次完成帧结构的时候去组织该帧
			return Append_frame(rtp_pack, rtp_pack->type );
		}else{
			printf("UnKnow package!\r\n");
		}
	}else if( nalu_type == NALU_MUTIL ) {
		printf("聚合包，暂不支持！\r\n");
	}else if( nalu_type == NALU_SIGNEL ) {
	// 	整个包的解码方式就是在nalu前加上 00001 nalu帧头字节即可
		offset = 13;
		type = Buffer[12] ;
		rtp_pack->offset = 0;
		rtp_pack->offset += AddH264Head(rtp_pack->buf, type );
		rtp_pack->offset += AppendData(rtp_pack->buf,rtp_pack->offset, Buffer + offset, BufferSize - offset);
		// 对于单一的nalu帧，我们还无法判定是否该直接push到解码通道里，因为还不清楚I帧是否到来
		return Append_frame( rtp_pack, type );
	}else{
		printf("unknow package !\r\n");
	}
	return 0;
}

