#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
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
// #include "./dbg_printf.h"

#define OUR_DEV
//#define HOME
typedef void * HANDLE;
#ifdef OUR_DEV
#define URL			"192.168.1.95"
#define DEF_RTSP_URL "rtsp://192.168.1.95/264.264"
// #define DEF_RTSP_URL "rtsp://192.168.0.28/trackID1"
// #define DEF_RTSP_URL "rtsp://192.168.2.112/264.264"
#else
#define DEF_RTSP_URL "rtsp://192.168.0.29/"
#endif

#define RTSP_PORT 554
#define PRG_NAME	"main"
#define UDP_RECV_PORT0 	1023
#define UDP_RECV_PORT1 	1024
#define max( a, b ) ( a > b ? a : b )

typedef enum RTSP_STAT
{
	RTSP_NONE = 0,
	RTSP_OPTION,
	RTSP_DESCRIPT,
	RTSP_SETUP,
	RTSP_PLAY,
	RTSP_GETPARM,
	RTSP_KEEP,
	RTSP_PAUSE,
	RTSP_STOP
}RTPS_STAT_E;

typedef enum LINK_STAT
{
	LINK_DISCONNECT = 0,
	LINK_CONNECTING,
	LINK_CONNECTED
}LINK_STAT_E;

typedef enum TRANSPORT
{
	TRANS_UDP = 0,
	TRANS_TCP,
	TRANS_RAW
}TRANSPORT_E;

#define MAGIC_NUM 0x1a2b4c3d
#define RTSP_INVALID( obj ) ( obj->magic != MAGIC_NUM )

typedef struct tagRtspClient
{
	unsigned int magic;
	int fd; 	//rtsp web socket
	TRANSPORT_E stream_type;	// 码流方式
	int recv_fd[2]; //
	int recv_port[2]; // 监听端口
	char rtsp_url[128]; // rtsp request url
	char server_ip[128];
	int server_port[2];
	char session[20];
	char authName[64];
	char authPwd[64];
	int support_cmd;
	int bQuit;
	int trackId;
	time_t tou;
	RTPS_STAT_E stat;
	LINK_STAT_E link;
	int CSeq;
	void *recv_buf;
	int maxBufSize;
	HANDLE Filter;
	HANDLE Vdec;
	HANDLE rtp;
	pthread_t threadID;
//	struct list_head list;
}RtspClient, *PVHRtspClient;

#define SOCK_ERROR() do{\
	if( len <= 0 )\
	{\
		fd = 0;\
		printf("sock error, %s!\r\n", strerror(errno));\
		break;\
	}\
}while(0)

/*  TCP and UDP API*/
int sock_listen( int port, const char *ipbind, int backlog )
{
	struct sockaddr_in 	my_addr;
	int 			fd, tmp = 1;

	fd = socket(AF_INET,SOCK_STREAM,0);
	if (fd < 0) 
  	 return -1;

 setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));

	memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons ((short)port);
	if( ipbind != NULL ) {
		inet_aton( ipbind, & my_addr.sin_addr );
	} else {
  	my_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	}

	if( 0 == bind (fd, (struct sockaddr *) &my_addr, sizeof (my_addr)) )
	{
		if( 0 == listen(fd, backlog) ) {
			return fd;
		}
	}
	close(fd);
	return -1;
}


int sock_dataready( int fd, int tout )
{
	fd_set	rfd_set;
	struct	timeval tv, *ptv;
	int	nsel;

	FD_ZERO( &rfd_set );
	FD_SET( fd, &rfd_set );
	if ( tout == -1 )
	{
		ptv = NULL;
	}
	else
	{
		tv.tv_sec = 0;
		tv.tv_usec = tout * 1000;
		ptv = &tv;
	}
	nsel = select( fd+1, &rfd_set, NULL, NULL, ptv );
	if ( nsel > 0 && FD_ISSET( fd, &rfd_set ) )
		return 1;
	return 0;
}


int sock_udp_bind( int port )
{
	struct sockaddr_in 	my_addr;
	int			tmp=0;
	int			udp_fd;

	// signal init, to avoid app quit while pipe broken
//	signal(SIGPIPE, SIG_IGN);

	if ( (udp_fd = socket( AF_INET, SOCK_DGRAM, 0 )) >= 0 )
	{
		setsockopt( udp_fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));

		memset(&my_addr, 0, sizeof(my_addr));
		my_addr.sin_family = AF_INET;
		my_addr.sin_port = htons ((short)port);
		my_addr.sin_addr.s_addr = htonl (INADDR_ANY); // get_ifadapter_ip("eth0",NULL);

		if (bind ( udp_fd, (struct sockaddr *) &my_addr, sizeof (my_addr)) < 0)
		{
    		close( udp_fd );
    		udp_fd = -EIO;
  		}
	}
  return udp_fd;
}
	
int sock_udp_send( const char *ip, int port, const void* msg, int len )
{
	// SOCKADDR_IN	udp_addr;
	struct sockaddr_in 	udp_addr;
	int	sockfd, ret;

	sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( sockfd <= 0 ) return -1;

  //setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));

	memset( & udp_addr, 0, sizeof(udp_addr) );
	udp_addr.sin_family = AF_INET;
	inet_aton( ip, &udp_addr.sin_addr );
	udp_addr.sin_port = htons( (short)port );

	ret = sendto( sockfd, msg, len, 0, (const struct sockaddr *) & udp_addr, sizeof(udp_addr) );
	close( sockfd );
	return ret;
}

static sock_read( int fd , char *buf, int maxBuf )
{
	return read( fd, buf, maxBuf );
}

int sock_connect( const char *host, int port )
{
	struct sockaddr_in	destaddr;
  struct hostent 		*hp;
	int 			fd = 0;

	memset( & destaddr, 0, sizeof(destaddr) );
	destaddr.sin_family = AF_INET;
	destaddr.sin_port = htons( (short)port );
  if ((inet_aton(host, & destaddr.sin_addr)) == 0)
  {
      hp = gethostbyname(host);
      if(! hp) return -EINVAL;
      memcpy (& destaddr.sin_addr, hp->h_addr, sizeof(destaddr.sin_addr));
  }

	fd = socket(PF_INET, SOCK_STREAM, 0);
	if (fd < 0)   return -EIO;

	if ( connect(fd, (struct sockaddr *)&destaddr, sizeof(destaddr)) < 0 )
	{
		close( fd );
		return -EIO;
	}
	return fd;
}
		
static const char* to_b64 =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

/* encode 72 characters	per	line */
#define	CHARS_PER_LINE	72

typedef unsigned char	byte;

/* return value:
 * >0: encoded string length, -1 buf too small
 * Encoded string is terminated by '\0'.
 */
int str_b64enc(const char *src, char *buf, int bufsize )
{
	int	size	= strlen( src );
	int	div	= size / 3;
	int	rem	= size % 3;
	int	chars 	= div*4 + rem + 1;
	int	newlines = (chars + CHARS_PER_LINE - 1)	/ CHARS_PER_LINE;
	int	outsize  = chars + newlines;
	const byte*	data = (const byte *)src;
	byte*		enc = (byte *)buf;

	if ( bufsize < outsize + 1 ) return -1;
	chars =	0;
	while (div > 0)
	{
		enc[0] = to_b64[ (data[0] >> 2)	& 0x3f];
		enc[1] = to_b64[((data[0] << 4)	& 0x30)	+ ((data[1] >> 4) & 0xf)];
		enc[2] = to_b64[((data[1] << 2)	& 0x3c)	+ ((data[2] >> 6) & 0x3)];
		enc[3] = to_b64[ data[2] & 0x3f];
		data +=	3;
		enc  += 4;
		div--;
		chars += 4;
		if (chars == CHARS_PER_LINE)
		{
			chars =	0;
//			*(enc++) = '\n';	/* keep the encoded string in single line */
		}
	}

	switch (rem)
	{
		case 2:
			enc[0] = to_b64[ (data[0] >> 2)	& 0x3f];
			enc[1] = to_b64[((data[0] << 4)	& 0x30)	+ ((data[1] >> 4) & 0xf)];
			enc[2] = to_b64[ (data[1] << 2)	& 0x3c];
			enc[3] = '=';
			enc   += 4;
			chars += 4;
			break;
		case 1:
			enc[0] = to_b64[ (data[0] >> 2)	& 0x3f];
			enc[1] = to_b64[ (data[0] << 4)	& 0x30];
			enc[2] = '=';
			enc[3] = '=';
			enc   += 4;
			chars += 4;
			break;
	}

	*enc = '\0';
	return strlen(buf);		// exclude the tail '\0'
}

/*
 * decode a base64 encoded string.
 * return -1: bufsize too small, \
 *         0: string content error,
 *       > 0: decoded string (null terminated).
 */
int  str_b64dec(const char* string, char *buf, int bufsize)
{
	register int length = string ? strlen(string) : 0;
	register byte* data = (byte *)buf;

	/* do a	format verification first */
	if (length > 0)
	{
		register int count = 0,	rem	= 0;
		register const char* tmp = string;

		while (length >	0)
		{
			register int skip = strspn(tmp,	to_b64);
			count += skip;
			length -= skip;
			tmp += skip;
			if (length > 0)
			{
				register int i,	vrfy = strcspn(tmp, to_b64);

				for (i = 0; i < vrfy; i++)
				{
					if (isspace(tmp[i])) continue;
					if (tmp[i] == '=')
					{
						/* we should check if we're close to the end of	the string */
						if ( (rem = count % 4) < 2 )
							/* rem must be either 2	or 3, otherwise	no '=' should be here */
							return 0;
						/* end-of-message recognized */
						break;
					}
					else
					{
						/* Invalid padding character. */
						return 0;
					}
				}
				length -= vrfy;
				tmp    += vrfy;
			}
		}
		if ( bufsize < (count/4 * 3 + (rem ? (rem-1) : 0)) )
			return -1;

		if (count > 0)
		{
			register int i,	qw = 0;

			length = strlen(string);
			for (i = 0; i < length; i++)
			{
			register char ch = string[i];
				register byte bits;

				if (isspace(ch)) continue;

				bits = 0;
				if ((ch	>= 'A')	&& (ch <= 'Z'))
				{
					bits = (byte) (ch - 'A');
				}
				else if	((ch >=	'a') &&	(ch <= 'z'))
				{
					bits = (byte) (ch - 'a'	+ 26);
				}
				else if	((ch >=	'0') &&	(ch <= '9'))
				{
					bits = (byte) (ch - '0'	+ 52);
				}
				else if	(ch == '=')
					break;

				switch (qw++)
				{
					case 0:
						data[0] = (bits << 2) & 0xfc;
						break;
					case 1:
						data[0] |= (bits >> 4) & 0x03;
						data[1] = (bits << 4) & 0xf0;
						break;
					case 2:
						data[1] |= (bits >> 2) & 0x0f;
						data[2] = (bits << 6) & 0xc0;
						break;
					case 3:
						data[2] |= bits & 0x3f;
						break;
				}
				if (qw == 4)
				{
					qw = 0;
					data += 3;
				}
			}
			data += qw;
			*data = '\0';
		}
	}
	return data - (unsigned char *)buf;
}

int strstartwith(const char *string, const char *prefix, int minmatch, char ** pleft)
{
	int len = strlen(prefix);
	int n = strlen( string );
	if ( n < len && n >=minmatch && minmatch ) len = n;
	
	if( 0 == strncmp(string, prefix, len) ) {
		if(pleft) *pleft = (char*)(string + len);
		return 1;
	}
	if(pleft) *pleft = NULL;
	return 0;
}

int stridxinargs( const char *key, int minmatch,...)
{
	va_list va;
	char *matcharg;
	int  matched_arg = -1;
	int arg_index = 0;
	
	va_start( va, minmatch );
	while ( (matcharg = va_arg( va , char * )) != NULL )
	{
		if ( (minmatch==0 && strcmp(key, matcharg)==0) ||
		    (minmatch && strstartwith( key, matcharg, minmatch, NULL) ) )
		{
			matched_arg = arg_index;
			break;
		}
		arg_index++;
	}
	va_end(va);
	return matched_arg;
}

int strgetword( const char *string, char *buf, int size, char **pleft)
{
	const char *ptr, *q;
	int len;

	/* skip leading white-space */
	for(ptr=string; *ptr && (*ptr==' ' || *ptr=='\t' || *ptr=='\n'); ptr++);
	if ( *ptr=='\0' ) return 0;
	for(q=ptr; *q && *q!=' ' && *q!='\t' && *q!='\n'; q++);
	len = q - ptr;
	if ( len < size )
	{
		memcpy( buf, ptr, len );
		buf[len] = '\0';
		if ( pleft )
			*pleft=(char*)q;
		return len;
	}
	return 0;
}
/* TCP and UDP API EDN  */

static const char *read_line( char *buf, char *line, char **dptr )
{
	if( !buf  || *buf == '\0') return;
	char *ptr = buf;
	char *lineS = line;
	for( ;*ptr != '\n'; ptr++ ) *line++ = *ptr;
	*line++ = '\n';
	*line = '\0';
	*dptr = (ptr+1);
	return lineS;
}

static int new_sock( int type, PVHRtspClient client )
{
	int i = 0;
	client->stream_type = TRANS_TCP;
	if( type == 0 )
	{
		client->stream_type = TRANS_UDP;
		printf("使用UDP方式获取码流!\r\n");
	}
	else
		printf("使用TCP方式获取码流!\r\n");
	int fd0, fd1;
	if( client->recv_fd[0] > 0 )
		close( client->recv_fd[0] );
	if( client->recv_fd[1] > 0 )
		close( client->recv_fd[1] );
	client->recv_port[0] = 0;
	client->recv_port[1] = 0;
	for( i; i < 100; i++ )
	{
		if( client->stream_type == TRANS_UDP )
		{
			fd0 = sock_udp_bind( 1024 + i );
			if( fd0 < 0 )
				continue;
			fd1 = sock_udp_bind( 1024 + i+1 );
			if( fd1 < 0 )
			{
				i++;
				close( fd0 );
				continue;
			}
			client->recv_fd[0] = fd0;
			client->recv_fd[1] = fd1;
			client->recv_port[0] = 1024 + i;
			client->recv_port[1] = 1024 + i+1;
			printf("绑定本地UDP端口:%d -%d \r\n", client->recv_port[0], client->recv_port[1] );
			break;
		}else if( client->stream_type == TRANS_TCP ){
			fd0 = sock_listen(1024 + i, NULL, 0);
			if( fd0 < 0 )
				continue;
			fd1 = sock_listen(1024 + i + 1, NULL, 0);
			if( fd1 < 0 )
			{
				i++;
				close( fd0 );
				continue;
			}
			client->recv_fd[0] = fd0;
			client->recv_fd[1] = fd1;
			client->recv_port[0] = 1024 + i;
			client->recv_port[1] = 1024 + i+1;
			break;
		}	
	}
	if( client->recv_port[0] > 0 )
		return 0;
	else
		return -1;
}


static int IsAnsOK( char *buf )
{
	int code = 0;
	if( buf == NULL )
		return -1;
	sscanf( buf, "RTSP/1.0 %d", &code );
	if( code == 200 )
		return 0;
	printf("ANS ERROR:%s \r\n", buf );
	return -1;
}

static void AnsOption( HANDLE handle, char *buf )
{
	PVHRtspClient obj = (PVHRtspClient)handle;
	obj->support_cmd = 0;
	if( IsAnsOK( buf ) != 0 )
		return;
	char *ptr = NULL;
	ptr = strstr( buf, "\r\n\r\n" );
	if( ptr )
	{
		ptr += 4;
		if( *ptr == '\0' )
			return;
		printf("Content:%s\r\n", ptr);
	}
	char *options = NULL;
	char key[30];
	while( strgetword( options, key, sizeof( key ), &options ) != 0 )
	{
		int index = stridxinargs( key, 5, "OPTIONS", "DESCRIBE", "SETUP", "PLAY", "TEARDOWN", "GET_PARAMETER" );
		switch( index )
		{
			case 0:  obj->support_cmd |= (1)<<0;  break;
			case 1:  obj->support_cmd |= (1)<<1;  break;
			case 2:  obj->support_cmd |= (1)<<2;  break;
			case 3:  obj->support_cmd |= (1)<<3; break;
			case 4:  obj->support_cmd |= (1)<<4;  break;
			case 5:  obj->support_cmd |= (1)<<5;  break;
			case 6:  obj->support_cmd |= (1)<<6;  break;
			default:
				 break;
		}
	}
	obj->stat = RTSP_DESCRIPT;	
}

static void AnsDescript( HANDLE handle, char *buf )
{
	PVHRtspClient obj = (PVHRtspClient)handle;
	if( IsAnsOK( buf ) != 0 )
		return;
	char *ptr , *q;
	char tmp[128] = {0};
	ptr = strstr( buf, "\r\n\r\n" );
	if( ptr )
	{
		ptr += 4;
		if( *ptr == '\0' )
			return;
	}
	// 获取Descript中关于Track的内容，由于Reply的Content里会有a=control:*部分，这部分我们不用，直接跳过，获取第一个track部分
    //定位到视频部分
    ptr = strstr( buf, "m=video" );
    if( ptr )
    {   
		ptr = strstr( ptr, "a=control" );
		if( ptr )
		{
			for( ;*ptr != ':'; ptr++ );
			ptr++;
			q = tmp;
			while( *ptr != '\r' && *ptr != ';' && *ptr != '\t' && *ptr != '\n')
				if( *ptr != ' ' )
					*q++ = *ptr++;
			else
				ptr++;
			*q = '\0';
			// 获取到的control后设置rtsp请求头，不然有问题
		//	if( obj->rtsp_url[0] == '\0' )
				sprintf( obj->rtsp_url, "rtsp://%s/%s", obj->server_ip, tmp );
			printf("rtsp_url ip:%s \r\n", obj->rtsp_url );
		}else{
			printf("can not find the rtsp source");
			obj->stat = RTSP_OPTION;
		}
	}else
	{
		printf("can not find the rtsp source");
		obj->stat = RTSP_OPTION;
	}
	obj->stat = RTSP_SETUP;	
}

static void AnsSetup( HANDLE handle, char *buf )
{
	PVHRtspClient obj = (PVHRtspClient)handle;
	if( IsAnsOK( buf ) != 0 )
		return;
	char tmp[128];
	char *ptr, *q;
	ptr = strstr( buf, "server_port=" );
	if( ptr )
	{
		ptr += 12;
		q = tmp;
		while( *ptr != '\r' && *ptr != ';' && *ptr != '\t' && *ptr != '\n')
			if( *ptr != ' ' )
				*q++ = *ptr++;
		else
			ptr++;
		*q = '\0';
		int start, end;
		sscanf( tmp, "%d-%d", &start, &end );
		obj->server_port[0] = start;
		obj->server_port[1] = end;
		printf("Server listen port:%d - %d\r\n", start, end );
	}
	ptr = strstr( buf, "Session:" );
	if( ptr )
	{
		q = tmp;
		ptr += 8;
		while( *ptr != '\r' && *ptr != ';' && *ptr != '\t' && *ptr != '\n')
		{
			if( *ptr != ' ' )
				*q++ = *ptr++;
			else
				ptr++;
		}
		printf("Session:%s \r\n", tmp );
		*q = '\0';
		strcpy( obj->session, tmp );
	}else{
		printf("Not find ptr!\r\n");
		obj->stat = RTSP_OPTION;	
	}
	obj->stat = RTSP_PLAY;	
}

static void AnsPlay( HANDLE handle, char *buf )
{
	PVHRtspClient obj = (PVHRtspClient)handle;
	if( IsAnsOK( buf ) != 0 )
		return;
	char *ptr = NULL;
	obj->stat = RTSP_KEEP;	
	ptr = strstr( buf, "\r\n\r\n" );
	if( ptr )
	{
		ptr += 4;
		if( *ptr == '\0' )
			return;
	}
}

// 该接口没用过，等到以后需要使用的时候再说，也不知道该接口干嘛的
static void AnsGetParam( HANDLE handle, char *buf )
{
	PVHRtspClient obj = (PVHRtspClient)handle;
	if( IsAnsOK( buf ) != 0 )
		return;
	char *ptr = NULL;
	ptr = strstr( buf, "\r\n\r\n" );
	if( ptr )
	{
		ptr += 4;
		if( *ptr == '\0' )
			return;
		printf("Content:%s\r\n", ptr);
	}
	obj->stat = RTSP_KEEP;	
}

//  关闭数据流
static void AnsStop( HANDLE handle, char *buf )
{
	if( IsAnsOK( buf ) != 0 )
		return;
	char *ptr = NULL;
	ptr = strstr( buf, "\r\n\r\n" );
	if( ptr )
	{
		ptr += 4;
		if( *ptr == '\0' )
			return;
		printf("Content:%s\r\n", ptr);
	}
}

static const char *getAuthurationInfo( HANDLE handle )
{
	//这边的内容待定，暂时没有用户名密码输入的需求，后期有的话再开发，大概这这么写的
	PVHRtspClient obj = (PVHRtspClient)handle;	
	static char authon[256] = {0};
	if( obj->authName[0] == '\0')
	{
		memset(authon, 0, sizeof( authon ) );
		return authon;
	}
	char body[128] = {0};
	char enBody[256] = {0};
	char const* const authFmt = "Authorization: Basic %s\r\n";
	unsigned usernamePasswordLength = strlen(obj->authName) + 1 + strlen(obj->authPwd);
	sprintf(body, "%s:%s", obj->authName, obj->authPwd);
//	int len = str_b64enc(body, enBody, sizeof( enBody ));
//	unsigned const authBufSize = strlen(authFmt) + strlen(enBody) + 1;
	sprintf(authon, authFmt, enBody );
	return authon;
}

int SendRequest( HANDLE handle , RTPS_STAT_E type )
{
	PVHRtspClient obj = (PVHRtspClient)handle;
	char *cmd = NULL;
	if( type == RTSP_NONE )
		obj->CSeq = 0;
	if( type != RTSP_PLAY )
		obj->CSeq++;
	char Agent[128] = {0}, StrCSeq[30] = {0}, Authoration[128] = {0};
	char contentLengthHeader[128] = {0}, extraHeaders[128] = {0};
	char contentStr[512] = {0};
	char const* protocolStr = "RTSP/1.0"; // by default
	char const* const cmdFmt =
	"%s %s %s\r\n"	//  type, url, rtsp
	"%s"	// CSeq
	"%s"	// Authuration
	"%s"	// Agent 
	"%s"	// extraHeader
	"%s"	// contentlen
	"\r\n"
	"%s"; // content
	switch( type )
	{
	case RTSP_NONE:
	case RTSP_OPTION:
		cmd = "OPTIONS";
		break;
	case RTSP_DESCRIPT:
		cmd = "DESCRIBE";
		break;
	case RTSP_SETUP:
		cmd = "SETUP";
		break;
	case RTSP_PLAY:
		cmd = "PLAY";
		break;
	case RTSP_GETPARM:
		cmd = "GET_PARAMETER";
		break;
	case RTSP_PAUSE:
	case RTSP_STOP:
		cmd = "TEARDOWN";
		break;
	case RTSP_KEEP:
		return;
	break;
		default:
	break;
	}
	sprintf( Agent, "User-Agent: %s\r\n",  PRG_NAME );
	sprintf( StrCSeq, "CSeq: %d\r\n", obj->CSeq );
	if( obj->session[0] != '\0' &&  obj->stat >  RTSP_SETUP )
		sprintf( extraHeaders, "Session: %s\r\nRange: npt=0.000-\r\n", obj->session );
	else  
		sprintf( extraHeaders, "Range: npt=0.000-\r\n" );
	if(  obj->stat == RTSP_SETUP )
	{
		new_sock( TRANS_UDP, obj );
		sprintf( extraHeaders, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n", obj->recv_port[0], obj->recv_port[1] );
	}
	if( obj->stat <= RTSP_DESCRIPT )
		sprintf( extraHeaders,"Accept: application/sdp\r\n");
	// if we have contelen and content info 
	sprintf( Authoration, "%s", getAuthurationInfo(obj));
	sprintf( obj->recv_buf , cmdFmt, cmd, obj->rtsp_url, protocolStr,\
	StrCSeq, Authoration, Agent, extraHeaders,\
	contentLengthHeader, contentStr );
	if( obj->fd > 0 )
	{
		printf("Send Requtst:%s \r\n", obj->recv_buf );
		write( obj->fd, obj->recv_buf, strlen( obj->recv_buf) );
	}	
	if( sock_dataready(obj->fd, 2000) )
	{
		memset( obj->recv_buf, 0, obj->maxBufSize );
		if( 0 >=  read( obj->fd, obj->recv_buf, obj->maxBufSize ) )
		{
			printf("No response!\r\n");
			return;
		}
		printf("ANS:\n%s\r\n", obj->recv_buf );
		switch( type )
		{
		case RTSP_NONE:
		case RTSP_OPTION:
			 AnsOption( obj,  obj->recv_buf );
			 break;
		case RTSP_DESCRIPT:
			 AnsDescript( obj,  obj->recv_buf );
			 break;
		case RTSP_SETUP:
			 AnsSetup( obj,  obj->recv_buf );
			 break;
		case RTSP_PLAY:
			 AnsPlay( obj,  obj->recv_buf );
			 break;
		case RTSP_GETPARM:
			 AnsGetParam( obj,  obj->recv_buf );
			 break;
		case RTSP_PAUSE:
		case RTSP_STOP:
			 AnsStop( obj,  obj->recv_buf );
			 break;
		break;
		 default: 
			break;
		}
	}
	return 0;
}

// extern HANDLE NewFilter( int size , HANDLE Vde);
extern int FilterWrite( HANDLE h, char *buf, int size );
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

void RTCP_PackParse( PVHRtspClient client , char *buf, int size )
{
	memset( buf, 0, size );
	int len = create_rtcp_reportinfo( buf, 128 );
	// RTCP 包就回复一下就好了， 无所谓的，基本不用的
	sock_udp_send( client->server_ip, client->server_port[1], buf, len );
//	printf("==================>>>>>>>>> Receive RTCP Rackage %s !\r\n", show_hex( buf, len ));
}

void RTP_PackParse( PVHRtspClient client, char *buf, int size )
{
	printf("receive pack:%d\r\n", size );
	if( ParseRtp( client->rtp , buf, size ) == 1 )
	{
		printf("Get a Full Nalu Packet!\r\n");
		// 如果获取到足够发送的码流，发送到解码通道
//		if( client->Vdec )
//			RTP_Send( client->rtp, client->Vdec );
	}
}

void *rtsp_work_thread(void *args)
{
	PVHRtspClient obj = (PVHRtspClient)args;
	obj->link = LINK_CONNECTING;
	int len;	
	int fd;
	time_t tout;
	// 先创建访问RTSP的OPTION部分内容
	if( obj->rtsp_url[0] == '\0' ) 
		sprintf( obj->rtsp_url, "rtsp://%s/", obj->server_ip );
	while( !obj->bQuit )
	{
		if( obj->link != LINK_CONNECTED )
			obj->link = LINK_CONNECTING;
		obj->stat = RTSP_NONE;
		if(obj->link != LINK_CONNECTED && obj->stat == RTSP_NONE )
		{
			fd = sock_connect( obj->server_ip, 554 );
			if( fd < 0 )
			{
				printf("connect failed! %s \r\n", strerror( errno ));
				usleep( 1000*1000 );
				obj->link = LINK_DISCONNECT;
				continue;
			}else{
				printf("connect success!\r\n");
				obj->link = LINK_CONNECTED;
			}
		}
		obj->fd = fd;
		if( SendRequest(obj, RTSP_OPTION ) == 0 )
		{
			printf("====> option OK!\r\n");
		}else
			continue;
		if( SendRequest(obj, RTSP_DESCRIPT ) == 0 )
		{
			printf("====> Descriput OK!\r\n");
			
		}else
			continue;
		if( SendRequest(obj, RTSP_SETUP ) == 0 )
		{
			
			printf("====> Setup OK!\r\n");
		}else
			continue;
		if( SendRequest(obj, RTSP_PLAY ) == 0 )
		{
			printf("Play OK!\r\n");
			obj->stat = RTSP_KEEP;
			obj->tou = time(NULL) + 60;
		}else
		{
			printf("Play Error!\r\n");
			continue;
		}
		tout = time( NULL );	
		while( obj->stat == RTSP_KEEP )
		{
			// 如果一直连接，则接收码流数据，如果5秒还没收到码流数据,或者1分钟超时，就发送一次心跳包，就发个OPTION帧好了
			if( obj->tou < time(NULL) || ( tout + 5 ) < time( NULL )  )
			{
				if( SendRequest(obj, RTSP_OPTION ) == 0 )
				{	
					obj->stat = RTSP_KEEP;
					// each 60s should be reconnect to rtsp send play as the heartbeat
					obj->tou = time(NULL) + 60;
					tout = time( NULL );
				}else{
					printf("has already disconnect!\r\n");
					obj->stat = RTSP_NONE;
				}
			}
			if( obj->stream_type == TRANS_TCP )
			{
				// tcp方式的话直接解析就好了，已经包含了RTP包了
				// not supprt now!
			}else if( obj->stream_type == TRANS_UDP )
			{	
				int maxFd = max( obj->recv_fd[0], obj->recv_fd[1] );
				struct	timeval tv;
				fd_set rfd_set;
				int	nsel;
				FD_ZERO( &rfd_set );
				FD_SET( obj->recv_fd[0], &rfd_set );
				FD_SET( obj->recv_fd[1], &rfd_set );
				tv.tv_sec = 0;
				tv.tv_usec = 10 * 1000;
				nsel = select( maxFd + 1, &rfd_set, NULL, NULL, &tv );
				if ( nsel > 0 && FD_ISSET( obj->recv_fd[1], &rfd_set ) )
				{
					len = sock_read( obj->recv_fd[1], obj->recv_buf, obj->maxBufSize );
					if( len > 4 )
						RTCP_PackParse( obj, obj->recv_buf, len );
				}else if( nsel > 0 && FD_ISSET( obj->recv_fd[0], &rfd_set ))
				{
					len = sock_read( obj->recv_fd[0], obj->recv_buf, obj->maxBufSize );
					if( len > 4 )
						RTP_PackParse( obj, obj->recv_buf, len );
					tout = time( NULL );
				}
			}
		}
	}
	close( obj->fd );
}

// 主码流次码流？现在不支持选择，只是放这边好了
HANDLE RTSP_New( const char *strIP , int maj, const char *usr, const char *pwd , HANDLE Vdec )
{
	if ( inet_addr(strIP) == -1 )
	{
		printf("input ip invalid! :%s \r\n", strIP );
		return NULL;
	}
	RtspClient	*client = malloc( sizeof( RtspClient ));
	memset( client, 0, sizeof( RtspClient ));
	client->magic = MAGIC_NUM;
	strcpy( client->server_ip, strIP );
	if( usr )
		strcpy( client->authName, usr );
	if( pwd )
		strcpy( client->authPwd , pwd );
	client->maxBufSize = 5*1024;
	client->recv_buf = malloc(client->maxBufSize);
	client->Vdec = Vdec;
	HANDLE rtp = RTP_Init();
	client->rtp = rtp;
	pthread_create( &client->threadID, NULL, rtsp_work_thread, (void *)client );
	return client;
}

void RTSP_Delete( HANDLE handle )
{
	RtspClient *obj = (RtspClient*)handle;
	if( RTSP_INVALID( obj ) )
		return;
	obj->bQuit = 1;
	if( obj->recv_buf )
		free( obj->recv_buf );
	pthread_cancel( obj->threadID );
	pthread_join( obj->threadID, NULL );
}

int main(int argc, char *argv[])
{
	HANDLE rtsp = RTSP_New( "192.168.0.27", 0, NULL, NULL, NULL );
	while( 1 )
		usleep( 1 );
}
