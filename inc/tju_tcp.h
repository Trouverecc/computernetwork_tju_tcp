#ifndef _TJU_TCP_H_
#define _TJU_TCP_H_

#include "global.h"
#include "tju_packet.h"
#include "kernel.h"


/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket();

//时间函数
#define timercpy(dst, src) \
    do{     \
        (dst)->tv_sec = (src)->tv_sec;      \
        (dst)->tv_usec = (src)->tv_usec;    \
    }while(0)

#define __Dynamic_Allocation_Sending_Buf__ 0    // 动态分配缓冲区内存
#define __Dynamic_Set_RTO__ 0       // 动态调整RTO
#define __Timeout_Resend__ 1        // 超时重传
#define __DEBUG__ 1                 // 调试信息

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr);

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
*/
int tju_listen(tju_tcp_t* sock);

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
tju_tcp_t* tju_accept(tju_tcp_t* sock);


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr);


int tju_send (tju_tcp_t* sock, const void *buffer, int len);
int tju_recv (tju_tcp_t* sock, void *buffer, int len);

/*
关闭一个TCP连接
这里涉及到四次挥手
*/
int tju_close (tju_tcp_t* sock);

void* send_thread(void* arg);


int tju_handle_packet(tju_tcp_t* sock, char* pkt);

// void start_timer(tju_tcp_t* sock);      //开启计时器，设置strat_time,timer_state置位1
// void end_timer(tju_tcp_t* sock);        //结束计时器，timer_state置位0
// double time_interval(tju_tcp_t* sock);  //计算计时器长度

void PutDataToBuf(tju_tcp_t *sock, char *pkt);
char *cSYNpkt(tju_tcp_t *sock);
char *getExpectedpkt(tju_tcp_t *sock);
uint32_t getrecvsize(tju_tcp_t* sock);
int Telloutoforder(tju_tcp_t* sock);
void* startTimer(void* arg);
void CreatePKTandsend(tju_tcp_t*sock,const void * buffer,int len);
long getctime();
#endif



