#include "tju_tcp.h"
#include <sys/time.h>
#include "time.h"


/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED; //初始化socket的状态为CLOSED
    
    pthread_mutex_init(&(sock->send_lock), NULL); //初始化用于发送操作的互斥锁
    sock->sending_buf = NULL; //初始化发送缓冲区为空
    sock->sending_len = 0; //初始化发送缓冲区的长度为0

    pthread_mutex_init(&(sock->recv_lock), NULL); //初始化用于接收操作的互斥锁
    sock->received_buf = NULL; //初始化接收缓冲区为空
    sock->received_len = 0; //初始化接收缓冲区的长度为0
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){ 
        //初始化条件变量，失败则退出程序
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    sock->window.wnd_send = NULL;
    sock->window.wnd_recv = NULL; //初始化发送和接收窗口为空

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    sock->bind_addr = bind_addr; //将传入的绑定地址保存到socket的bind_addr字段中
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t* sock){
    init_queue(); // 初始化半连接和全连接队列
    sock->state = LISTEN; //开始监听
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock; //将监听的socket存在监听socket的哈希表中
    return 0;
}

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
    // tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    // memcpy(new_conn, listen_sock, sizeof(tju_tcp_t));

    // tju_sock_addr local_addr, remote_addr;
    // /*
    //  这里涉及到TCP连接的建立
    //  正常来说应该是收到客户端发来的SYN报文
    //  从中拿到对端的IP和PORT
    //  换句话说 下面的处理流程其实不应该放在这里 应该在tju_handle_packet中
    // */ 
    // remote_addr.ip = inet_network("172.17.0.2");  //具体的IP地址
    // remote_addr.port = 5678;  //端口

    // local_addr.ip = listen_sock->bind_addr.ip;  //具体的IP地址
    // local_addr.port = listen_sock->bind_addr.port;  //端口

    // new_conn->established_local_addr = local_addr;
    // new_conn->established_remote_addr = remote_addr; //保存本地和远程地址到新连接socket

    // 判断全连接队列中是否有 socket
    fprintf(stdout, "开始监听全连接队列\n");
    tju_tcp_t* accept_socket=get_from_accept();     // 队列为空 阻塞
    fprintf(stdout, "从全连接队列中取出一个sock\n");

    // // 这里应该是经过三次握手后才能修改状态为ESTABLISHED
    // new_conn->state = ESTABLISHED;

    tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    memcpy(new_conn, accept_socket, sizeof(tju_tcp_t));
    free(accept_socket);

    // 将新的conn放到内核建立连接的socket哈希表中
    // int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
    int hashval = cal_hash(new_conn->established_local_addr.ip, new_conn->established_local_addr.port, \
                new_conn->established_remote_addr.ip, new_conn->established_remote_addr.port);
    established_socks[hashval] = new_conn;

    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 
    fprintf(stdout, "server端三次握手完成\n");
    return new_conn;
}


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){
    //设置目标服务器的地址（IP和端口）
    // sock->established_remote_addr = target_addr;

    //socket绑定本地地址
    tju_sock_addr local_addr;
    local_addr.ip = inet_network("172.17.0.2");
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    sock->established_local_addr = local_addr;

    // 这里也不能直接建立连接 需要经过三次握手
    // 实际在linux中 connect调用后 会进入一个while循环
    // 循环跳出的条件是socket的状态变为ESTABLISHED 表面看上去就是 正在连接中 阻塞
    // 而状态的改变在别的地方进行 在我们这就是tju_handle_packet
    // sock->state = ESTABLISHED;

    // 向客户端发送 SYN 报文，并将状态改为 SYN_SENT
    uint32_t seq=CLIENT_ISN;
    char* packet_SYN=create_packet_buf(local_addr.port,target_addr.port,seq,0,\
            DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,SYN_FLAG_MASK,1,0,NULL,0);
    sendToLayer3(packet_SYN,DEFAULT_HEADER_LEN);
    sock->state=SYN_SENT;
    fprintf(stdout, "client端发送SYN---第一次握手\n");

    // 将即将建立连接的socket放入内核 已建立连接哈希表中
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;

    // 阻塞等待
    while (sock->state!=ESTABLISHED) ;

    // 三次握手完成
    sock->established_remote_addr = target_addr;    // 绑定远端地址
    fprintf(stdout, "client端三次握手完成\n");

    return 0;
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    char* data = malloc(len);
    memcpy(data, buffer, len);

    // 设置序列号和包长度
    char* msg;
    uint32_t seq = 464;
    uint16_t plen = DEFAULT_HEADER_LEN + len;

    // 创建TCP数据包，包括包头和数据部分
    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);

    //将创建的TCP数据包发送到网络层
    sendToLayer3(msg, plen);
    
    return 0;
}
int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len<=0){
        // 阻塞
    }

    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0;
    if (sock->received_len >= len){ // 从中读取len长度的数据
        read_len = len;
    }else{
        read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
    }

    memcpy(buffer, sock->received_buf, read_len);

    if(read_len < sock->received_len) { // 还剩下一些
        char* new_buf = malloc(sock->received_len - read_len);
        memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
        free(sock->received_buf);
        sock->received_len -= read_len;
        sock->received_buf = new_buf;
    }else{
        free(sock->received_buf);
        sock->received_buf = NULL;
        sock->received_len = 0;
    }
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    return 0;
}

int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    
    // 判断收到报文的 socket 的状态是否为 SYN_SENT
    if (sock->state==SYN_SENT){
        if (get_ack(pkt)==CLIENT_ISN+1){
            char* packet_SYN_ACK2=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                        DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_SYN_ACK2,DEFAULT_HEADER_LEN);
            sock->state=ESTABLISHED;
            fprintf(stdout, "client端发送SYN_ACK----第三次握手\n");
        }
    }
    else if (sock->state==LISTEN){
        if (get_flags(pkt)==SYN_FLAG_MASK){
            // 将 socket 存入半连接队列中
            tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
            memcpy(new_conn, sock, sizeof(tju_tcp_t));
            new_conn->state=SYN_RECV;
            en_syn_queue(new_conn);
            fprintf(stdout, "sock进入半连接队列\n");

            // 向客户端发送 SYN_ACK 报文
            char* packet_SYN_ACK1=create_packet_buf(get_dst(pkt),get_src(pkt),SERVER_ISN,get_seq(pkt)+1,\
                        DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,SYN_FLAG_MASK|ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_SYN_ACK1,DEFAULT_HEADER_LEN);
            fprintf(stdout, "server发送SYN_ACK----第二次握手\n");
        }
        else if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==SERVER_ISN+1){
            // 取出半连接中的socket加入全连接队列中
            tju_tcp_t* tmp_conn=get_from_syn();
            fprintf(stdout, "从半连接队列中取出sock\n");
            tmp_conn->established_local_addr=tmp_conn->bind_addr;
            tmp_conn->established_remote_addr.ip=inet_network("172.17.0.2");
            tmp_conn->established_remote_addr.port=get_src(pkt);
            tmp_conn->state=ESTABLISHED;

            en_accept_queue(tmp_conn);
            fprintf(stdout, "sock加入全连接队列\n");
        }
    }

    else if (sock->state==ESTABLISHED){
        if (get_flags(pkt)==FIN_FLAG_MASK|ACK_FLAG_MASK){
            // 发送FIN_ACK报文
            char* packet_FIN_ACK=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_FIN_ACK,DEFAULT_HEADER_LEN);
            fprintf(stdout, "发送FIN_ACK报文\n");

            // 更新状态
            sock->state=CLOSE_WAIT;
            sleep(1);          // 等待，防止混入 同时关闭 的情况

            // 调用close
            tju_close(sock);
        }
    }
    else if (sock->state==FIN_WAIT_1){
        if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==FIN_SEQ+1){    // 双方先后关闭
            sock->state=FIN_WAIT_2;
        }
        else if (get_flags(pkt)==FIN_FLAG_MASK|ACK_FLAG_MASK){    // 同时关闭
            // 发送FIN_ACK报文
            char* packet_FIN_ACK3=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_FIN_ACK3,DEFAULT_HEADER_LEN);

            // 状态更新
            sock->state=CLOSING;
        }
    }
    else if (sock->state==FIN_WAIT_2){
        if (get_flags(pkt)==FIN_FLAG_MASK|ACK_FLAG_MASK){
            // 发送FIN_ACK报文
            char* packet_FIN_ACK2=create_packet_buf(get_dst(pkt),get_src(pkt),get_ack(pkt),get_seq(pkt)+1,\
                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,ACK_FLAG_MASK,1,0,NULL,0);
            sendToLayer3(packet_FIN_ACK2,DEFAULT_HEADER_LEN);

            // 更新socket状态并等待2MSL
            sock->state=TIME_WAIT;
            sleep(10);
            sock->state=CLOSED;
        }
    }
    else if (sock->state==LAST_ACK){
        if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==FIN_SEQ+1){
            sock->state=CLOSED;
        }
    }
    else if (sock->state==CLOSING){
        if (get_flags(pkt)==ACK_FLAG_MASK&&get_ack(pkt)==FIN_SEQ+1){
            sock->state=TIME_WAIT;
            sleep(10);          // 等待2MSL
            sock->state=CLOSED;
        }
    }

    //计算数据包中的数据长度
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;
    if (data_len==0) return 0;

    // 把收到的数据放到接受缓冲区
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    //如果接收缓冲区为空，则分配新的缓冲区；否则扩展现有的缓冲区以容纳新的数据
    if(sock->received_buf == NULL){
        sock->received_buf = malloc(data_len);
    }else {
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    }
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    //将新数据复制到接收缓冲区，并更新接收数据的长度
    sock->received_len += data_len;

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁


    return 0;
}

int tju_close (tju_tcp_t* sock){
    // 发送FIN报文
    char* packet_FIN=create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,FIN_SEQ,\
                    0,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,FIN_FLAG_MASK|ACK_FLAG_MASK,1,0,NULL,0);
    sendToLayer3(packet_FIN,DEFAULT_HEADER_LEN);

    // 状态更新
    if (sock->state==ESTABLISHED){  // 主动调用
        sock->state=FIN_WAIT_1;
    }
    else if (sock->state==CLOSE_WAIT){  // 被动调用
        sock->state=LAST_ACK;
    }

    // 阻塞等待
    while (sock->state!=CLOSED) ;

    // 释放资源
    free(sock);
    return 0;
}
//establish ok, close not ok
// #include "tju_tcp.h"
// #include <arpa/inet.h>
// #include <stdio.h>



// /*
// 创建 TCP socket 
// 初始化对应的结构体
// 设置初始状态为 CLOSED
// */
// tju_tcp_t* tju_socket(){
//     tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
//     sock->state = CLOSED;
    
//     pthread_mutex_init(&(sock->send_lock), NULL);
//     sock->sending_buf = NULL;
//     sock->sending_len = 0;

//     pthread_mutex_init(&(sock->recv_lock), NULL);
//     sock->received_buf = NULL;
//     sock->received_len = 0;
    
//     if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
//         perror("ERROR condition variable not set\n");
//         exit(-1);
//     }

//     sock->window.wnd_send = NULL;
//     sock->window.wnd_recv = NULL;

//     sock->half_queue = q_init();
//     sock->full_queue = q_init();

//     return sock;
// }

// /*
// 绑定监听的地址 包括ip和端口
// */
// int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
//     if(bhash[bind_addr.port]){
//         printf("端口 %d 已经被占用, 绑定失败\n",bind_addr.port);
//         return -1;
//     }
//     sock->bind_addr = bind_addr;
//     return 0;
// }

// /*
// 被动打开 监听bind的地址和端口
// 设置socket的状态为LISTEN
// 注册该socket到内核的监听socket哈希表
// */
// int tju_listen(tju_tcp_t* sock){
//     sock->state = LISTEN;
//     int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
//     listen_socks[hashval] = sock;
//     return 0;
// }

// /*
// 接受连接 
// 返回与客户端通信用的socket
// 这里返回的socket一定是已经完成3次握手建立了连接的socket
// 因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
// */
// tju_tcp_t* tju_accept(tju_tcp_t* listen_sock){
//     // tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
//     // memcpy(new_conn, listen_sock, sizeof(tju_tcp_t));
//     while(!listen_sock->full_queue->len); // 注: 当 listen 的全连接中没有新来的时候阻塞
//     tju_tcp_t * new_connection = q_pop(listen_sock->full_queue);  

//     tju_sock_addr local_addr, remote_addr;
//     /*
//      这里涉及到TCP连接的建立
//      正常来说应该是收到客户端发来的SYN报文
//      从中拿到对端的IP和PORT
//      换句话说 下面的处理流程其实不应该放在这里 应该在tju_handle_packet中
//     */ 
//     // remote_addr.ip = inet_network("172.17.0.2");  //具体的IP地址
//     // remote_addr.port = 5678;  //端口

//     // local_addr.ip = listen_sock->bind_addr.ip;  //具体的IP地址
//     // local_addr.port = listen_sock->bind_addr.port;  //端口

//     // new_conn->established_local_addr = local_addr;
//     // new_conn->established_remote_addr = remote_addr;

//     // 这里应该是经过三次握手后才能修改状态为ESTABLISHED
//     // new_conn->state = ESTABLISHED;

//     local_addr.ip = new_connection->established_local_addr.ip;
//     local_addr.port = new_connection->established_local_addr.port;
//     remote_addr.ip = new_connection->established_remote_addr.ip;
//     remote_addr.port = new_connection->established_remote_addr.port;

//     // 将新的conn放到内核建立连接的socket哈希表中
//     int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
//     established_socks[hashval] = new_connection;
//     // established_socks[hashval] = new_conn;

//     // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
//     // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
//     // 每次调用accept 实际上就是取出这个队列中的一个元素
//     // 队列为空,则阻塞 
//     return new_connection;
//     // return new_conn;
// }


// /*
// 连接到服务端
// 该函数以一个socket为参数
// 调用函数前, 该socket还未建立连接
// 函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
// 因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
// */
// int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr){

//     // sock->established_remote_addr = target_addr;

//     tju_sock_addr local_addr;
//     local_addr.ip = inet_network("172.17.0.2");
//     local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
//     sock->established_local_addr = local_addr;
//     sock->established_remote_addr = target_addr;

//     // 将建立了连接的socket放入内核 已建立连接哈希表中
//     int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
//     established_socks[hashval] = sock;

//     char *msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, CLIENT_ISN, 0, DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, SYN_FLAG_MASK, 1, 0, NULL, 0);

//     sendToLayer3(msg, DEFAULT_HEADER_LEN);
//     sock->state = SYN_SENT;

//     // 这里也不能直接建立连接 需要经过三次握手
//     // 实际在linux中 connect调用后 会进入一个while循环
//     // 循环跳出的条件是socket的状态变为ESTABLISHED 表面看上去就是 正在连接中 阻塞
//     // 而状态的改变在别的地方进行 在我们这就是tju_handle_packet
//     // sock->state = ESTABLISHED;
//     printf("waiting 4 establish\n");

//     // // 将建立了连接的socket放入内核 已建立连接哈希表中
//     // int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
//     // established_socks[hashval] = sock;

//     while(sock->state != ESTABLISHED); // 等待对方将本方状态转成 ESTABLISHED


//     return 0;
// }



// int tju_send(tju_tcp_t* sock, const void *buffer, int len){
//     // 这里当然不能直接简单地调用sendToLayer3
//     char* data = malloc(len);
//     memcpy(data, buffer, len);
//     printf("Sending to Client\n");

//     char* msg;
//     uint32_t seq = 464;
//     uint16_t plen = DEFAULT_HEADER_LEN + len;

//     printf("tju_send, 长度len:%d, 内容>%s<\n",len,data);

//     msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
//               DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);

//     sendToLayer3(msg, plen);
    
//     return 0;
// }
// int tju_recv(tju_tcp_t* sock, void *buffer, int len){
//     while(sock->received_len<=0){
//         // 阻塞
//     }

//     while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

//     int read_len = 0;
//     if (sock->received_len >= len){ // 从中读取len长度的数据
//         read_len = len;
//     }else{
//         read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
//     }

//     memcpy(buffer, sock->received_buf, read_len);

//     if(read_len < sock->received_len) { // 还剩下一些
//         char* new_buf = malloc(sock->received_len - read_len);
//         memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
//         free(sock->received_buf);
//         sock->received_len -= read_len;
//         sock->received_buf = new_buf;
//     }else{
//         free(sock->received_buf);
//         sock->received_buf = NULL;
//         sock->received_len = 0;
//     }
//     pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

//     return 0;
// }

// int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    
//     uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

//     // 把收到的数据放到接受缓冲区
//     while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

//     if(sock->received_buf == NULL){
//         sock->received_buf = malloc(data_len);
//     }else {
//         sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
//     }
//     memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
//     sock->received_len += data_len;

//     pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

//     int pkt_seq = get_seq(pkt);
//     int pkt_src = get_src(pkt);
//     int pkt_ack = get_ack(pkt);
//     int pkt_plen = get_plen(pkt);
//     int pkt_flag = get_flags(pkt);

//     printf("==> 开始 handle packet\n");

//     if(sock->state==LISTEN){

//         if(pkt_flag==SYN_FLAG_MASK){
//             // 收到 SYN_FLAG --> 连接第一次握手
//             printf("LISTEN 状态 收到 SYN_FLAG\n");
//             tju_tcp_t * new_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
//             memcpy(new_sock, sock, sizeof(tju_tcp_t));

//             tju_sock_addr remote_addr, local_addr;
//             remote_addr.ip = inet_network("172.0.0.2"); // Listen 是 server 端的行为，所以远程地址就是 172.0.0.2 
//             remote_addr.port = pkt_src;
//             local_addr.ip = sock->bind_addr.ip;
//             local_addr.port = sock->bind_addr.port;

//             new_sock->established_local_addr = local_addr;
//             new_sock->established_remote_addr = remote_addr;

//             new_sock->state = SYN_RECV;


//             q_push(sock->half_queue, new_sock);
//             printf("新 socket 准备好，可以push\n");

//             tju_packet_t * ret_pack = create_packet(local_addr.port, remote_addr.port, SERVER_ISN, pkt_seq+1,
//                                                     DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK | SYN_FLAG_MASK, 1 , 0 , NULL, 0);
//             char *msg = packet_to_buf(ret_pack);
//             sendToLayer3(msg, DEFAULT_HEADER_LEN);
//             printf("Server 接受到 client 的第一次握手，返回第二次握手\n");

//         }else if(pkt_flag==ACK_FLAG_MASK){
//             // 在 Listen 状态接收到 ACK 表明是第三次握手
//             // 检查对方 Ack Number 是否正确
//             if(pkt_ack==SERVER_ISN+1){
//                 tju_tcp_t * established_conn = q_pop(sock->half_queue);
//                 if(established_conn == NULL){
//                     printf("Establshed 是 NULL\n");
//                 }
//                 established_conn->state = ESTABLISHED;
//                 q_push(sock->full_queue, established_conn);
//                 printf("收到正确的 Ack 三次握手建立成功（至少server 端这么认为）\n");
//                 return 0;
//             }else{
//                 printf("client 端发送的 ack 错误，期待>%d<，接收到>%d<\n",SERVER_ISN+1, pkt_ack);
//             }
//             }else{
//                 printf("当前为 LISTEN，接收到其他 flag 的报文，丢弃之\n");
//         }
//     }

//     if(sock->state == SYN_SENT){
//         if(pkt_ack == CLIENT_ISN + 1){
//             tju_packet_t * new_pack = create_packet(sock->established_local_addr.port, sock->established_remote_addr.port, pkt_ack, pkt_seq+1,
//                                                     DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK, 1, 0, NULL, 0);
//             char *msg = packet_to_buf(new_pack);
//             sendToLayer3(msg, DEFAULT_HEADER_LEN);
//             printf("Client 端发送了第三次握手，建立成功\n");
//             sock->state = ESTABLISHED;
//             return 0;
//         }else{
//             printf("Client 端接收到了不正确的ack\n");
//             return 0;
//         }
//   }

//     return 0;
// }


// int tju_close (tju_tcp_t* sock){
//     return 0;
    
// }

// // 定义 socket queue 的 init/pop/push/size

// sock_node* new_node(tju_tcp_t* p){
//     sock_node *ret = (sock_node *)malloc(sizeof(sock_node));
//     ret->sock = p;
//     ret->next = NULL;
//     return ret;
// }

// sock_queue* q_init(){
//     sock_queue *q = (sock_queue*)malloc(sizeof(sock_queue));
//     q->sock_end = q->sock_head=NULL;
//     q->len = 0;
//     return q;
// }

// int q_size(sock_queue*q){
//     return q->len;
// }

// tju_tcp_t* q_pop(sock_queue * q){
//     if(q->len){
//         q->len--;
//         tju_tcp_t* ret = q->sock_head->sock;
//         sock_node *free_it = q->sock_head;
//         q->sock_head = q->sock_head->next;
//         if(q->sock_head==NULL){
//             q->sock_end = NULL;
//         }
//         free(free_it);
//         return ret;
//     }
//     return NULL;
// }

// void q_print(sock_queue *q){
//     sock_node *w = q->sock_head;
//     int i=0;
//     printf("\n************ PRINT_QUEUE %d ************\n",q->len);
//     while(w!=NULL){
//         if(w->sock==NULL){
//             printf("%d: NULL,",i++);
//         }else printf("%d: %d,",i++,w->sock->state);
//         w = w->next;
//     }
//     printf("\n************  QUEUE PRINT END ************\n\n");
// }

// int q_push(sock_queue *q, tju_tcp_t *sock){
//     sock_node *tmp = new_node(sock);
//     if(q->sock_head==NULL){
//         q->sock_head=q->sock_end=tmp;
//         q->len++;
//         return 0;
//     }
//     q->sock_end->next = tmp;
//     q->sock_end = tmp;
//     q->len++;
//     return 0;
// }

