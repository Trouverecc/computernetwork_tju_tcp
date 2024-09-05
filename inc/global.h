#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define CLIENT_IP "172.17.0.5"
#define SERVER_IP "172.17.0.6"


#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "global.h"
#include <pthread.h>
#include <sys/select.h>
#include <arpa/inet.h>

FILE* clientfp;
FILE* serverfp;

// 单位是byte
#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

// 一些Flag
#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2
#define TRUE 1
#define FALSE 0

// 定义最大包长 防止IP层分片
#define MAX_DLEN 1375 	// 最大包内数据长度
#define MAX_LEN 1400 	// 最大包长度

// TCP socket 状态定义
#define CLOSED 0
#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECV 3
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 6
#define CLOSE_WAIT 7
#define CLOSING 8
#define LAST_ACK 9
#define TIME_WAIT 10

// TCP 拥塞控制状态
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// TCP 接受窗口大小
#define TCP_RECVWN_SIZE 32*MAX_DLEN // 比如最多放32个满载数据包

// TCP 发送窗口
#define TCP_SENDWN_SIZE 64	// 最大发送窗口数

// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {

	//发送窗口的大小,单位是字节
	uint16_t window_size;

	//发送窗口记录第一个发出去没有ack的数据包seq
	uint32_t base;
	//发送窗口记录下一个进入发送窗口的数据包seq/ack
	uint32_t nextseq;
	uint32_t estmated_rtt;
	uint32_t nextack;

	//	估算rtt
    // struct timeval estmated_rtt;	// 估计RTT
	// struct timeval  RTO;			// 超时时间
	// struct timeval  DevRTT;			// RTT偏差

	int ack_cnt;
    pthread_mutex_t ack_cnt_lock;
    struct timeval send_time;
    char* retran[2];
    int clk;
//   struct timeval timeout;
	////记录接收窗口的余量大小，根据这个调整拥塞状态
	uint16_t rwnd; 
	//拥塞状态
	//int congestion_status;
    //uint16_t cwnd; 
	//发送窗口上限，体现发送速率
	//uint16_t ssthresh; 

} sender_window_t;

// TCP 接受窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	//接收窗口缓冲区
	char * outoforder[MAX_LEN];

    //received_packet_t* head;(没有这个数据类型？？？)
	//用于标记所有缓冲区有没有数据
	//uint8_t marked[TCP_RECVWN_SIZE];
	//预期的seq，用于判定是否收到应收到的seq
	uint32_t expect_seq;
} receiver_window_t;

// TCP 窗口 每个建立了连接的TCP都包括发送和接受两个窗口
typedef struct {
	sender_window_t* wnd_send;
  	receiver_window_t* wnd_recv;
} window_t;

typedef struct {
	uint32_t ip;
	uint16_t port;
} tju_sock_addr;

// typedef struct{
//     uint32_t ack;
//     uint32_t seq;
//     char* firstByte;
//     int len;

//     struct timeval sendTime;
//     struct timeval RTO;
//     struct sendTimer* next;
// }sendTimer;

// #define MAX_TIME 0.04
// typedef struct {
// 	int timer_state;		//标志计时器启动情况

// 	struct timeval set_time;//启动计时器的时间
// 	struct timeval now_time;//当前时间
// }sock_timer_t;

#define MAX_SOCK 32
// TJU_TCP 结构体 保存TJU_TCP用到的各种数据
typedef struct {
	int state; // TCP的状态

	tju_sock_addr bind_addr; // 存放bind和listen时该socket绑定的IP和端口
	tju_sock_addr established_local_addr; // 存放建立连接后 本机的 IP和端口
	tju_sock_addr established_remote_addr; // 存放建立连接后 连接对方的 IP和端口

	pthread_mutex_t send_lock; // 发送数据锁
	char* sending_buf; // 发送数据缓存区
	int sending_len; // 发送数据缓存长度
	//char* lastByteSentPoint;	// 上一次发送的最后一个字节
	//char* endDataPoint;		// 缓冲区最后一个字节

	pthread_mutex_t recv_lock; // 接收数据锁
	char* received_buf; // 接收数据缓存区
	int received_len; // 接收数据缓存长度

	pthread_cond_t wait_cond; // 可以被用来唤醒recv函数调用时等待的线程

	window_t window; // 发送和接受窗口

	int close_same;
	//sock_timer_t timer;//计时器

	// tju_tcp_t* synqueue[MAX_SOCK];//半连接列表：存放收到SYN但是没收到ACK的sock
	// tju_tcp_t* acceptqueue[MAX_SOCK];//全连接列表：存放收到SYN但是没收到ACK的sock

} tju_tcp_t;

#endif


