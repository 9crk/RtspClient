/*
 * =====================================================================================
 *
 *       Filename:  
 *
 *    Description:  video queue
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


typedef struct tagMediaQueue
{
	// _longtime time_stamp;	//
	void *video;
	int size;
}MediaQuequ_S;

MediaQuequ_S VideoQueue[128];

// �������һ����Ƶ֡
MediaQuequ_S* Push( void *buf, int size )
{
	
	
}

// push de ʱ�����
MediaQuequ_S* Pop()
{
	
	
	
}
