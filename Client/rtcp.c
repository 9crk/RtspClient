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

#include "rtcp.h"

#define RTCP_SR	200

#define HeadInit( head, type ) do{\
	rtcp_common_header_set_version(head, 2);\
	rtcp_common_header_set_padbit(head, 0);\
	rtcp_common_header_set_rc(head, 0);\
	rtcp_common_header_set_packet_type(head, type);\
	rtcp_common_header_set_length(head, sizeof( report_block_t ) );\
	}while(0);

int create_rtcp_reportinfo( char *Buffer, int maxBuffer )
{
	rtcp_common_header_t *head = (rtcp_common_header_t*)Buffer;
	report_block_t *block = (report_block_t*)(Buffer + sizeof(rtcp_common_header_t));
	rtcp_common_header_set_version(head, 2);
	rtcp_common_header_set_padbit(head, 0);
	rtcp_common_header_set_rc(head, 0);
	rtcp_common_header_set_packet_type(head, RTCP_RR);
	rtcp_common_header_set_length(head, sizeof( report_block_t ) );\
	// 设置
	memset( block, 0, sizeof( report_block_t ));
	report_block_set_fraction_lost(block, 0);
	report_block_set_cum_packet_lost(block, 0);
	return sizeof( rtcp_common_header_t ) + sizeof( report_block_t );
}


int rtcp_parse(  char *Buffer, int BufferSize )
{
	rtcp_common_header_t *header = (rtcp_common_header_t*)Buffer;
	printf("version:%d , pad:%d , rc:%d , type:%d, len:%d \r\n",
		rtcp_common_header_get_version(header),
		rtcp_common_header_get_padbit(header),
		rtcp_common_header_get_rc(header),
		rtcp_common_header_get_packet_type(header),
		rtcp_common_header_get_length(header) );
		if( rtcp_common_header_get_packet_type(header) == RTCP_SR )
		{
			printf("收到rtcp RTCP_SR包，解析!\r\n");
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_RR) {
			printf("收到rtcp RTCP_RR包，解析!\r\n");
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_SDES) {
			printf("收到rtcp RTCP_SDES包，解析!\r\n");
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_BYE) {
			printf("收到rtcp RTCP_BYE包，解析!\r\n");
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_APP) {
			printf("收到rtcp RTCP_APP包，解析!\r\n");
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_RTPFB) {
			printf("收到rtcp RTCP_RTPFB包，解析!\r\n");	
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_PSFB) {
			printf("收到rtcp RTCP_PSFB包，解析!\r\n");
		}else if(rtcp_common_header_get_packet_type(header) == RTCP_XR) {
			printf("收到rtcp RTCP_XR包，解析!\r\n");
		}else{
			printf("收到rtcp unkonw包，解析!\r\n");
		}			
		return 1;
}

static int read_rtcp( char *buf , int index )
{
	int len;
	char filename[120];
	sprintf(filename, "rtcpslice%d.hex", index );
	if( access( filename, F_OK ) == 0 )
	{
		int fd = open( filename, O_RDONLY );
		if( fd < 0 )
			return;
		len = read( fd, buf, 1023 );
		close( fd );
		return len; 
	}
	return -1;
}
#if 0
const char *show_hex( const char *ch, int rlen)
{
	int i = 0;
	char *ptr =(char*)ch;
	static char buf[1024];
	memset( buf, 0, sizeof( buf ));
	char *off = buf;
	unsigned char val;
	int len = rlen > 300 ? 300 : rlen;
	memset( buf, 0, sizeof( buf ));
	for (i = 0; i < len; i++)
	{
		val = *ptr++;
		sprintf( off, "%02x ", val); 
		off+=3;
	}
	*off = '\0';
	return buf;
}
#endif

/*

void main()
{
	int i = 0;
	char buf[1023];
	int len;
	for( i = 0; i < 20 ; i++ )
	{
		len = read_rtcp( buf, i );
		if( len > 0 )
		{
			rtcp_parse( buf, len );
			len = create_rtcp_reportinfo( buf, sizeof( buf ));
			printf("return size %d \r\n", len );
			if( len > 0 )
				printf("return value:%s \r\n", show_hex( buf , len ));
		}
		
	}
	return;
}
*/
