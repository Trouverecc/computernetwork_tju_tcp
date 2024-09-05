#include "tju_tcp.h"
// #include "initial.h"

/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    //初始化发送缓冲区initsendbuf
    pthread_mutex_init(&(sock->send_lock), NULL);
    sock->sending_buf = NULL;
    sock->sending_len = 0;

    //初始化接收缓冲区initrecvbuf
    pthread_mutex_init(&(sock->recv_lock), NULL);
    sock->received_buf = NULL;
    sock->received_len = 0;

    // initsendbuf(sock);
    // initrecvbuf(sock);
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){// 初始化条件变量
        perror("ERROR condition variable not set\n");// 如果条件变量初始化失败，打印错误信息
        exit(-1);
    }

    //初始化发送窗口
    sock->window.wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));// 分配发送窗口结构体
    sock->window.wnd_send->window_size = 1 * MAX_DLEN;// 设置发送窗口大小
    sock->window.wnd_send->base = 0;// 发送窗口的基序号初始化为0
    sock->window.wnd_send->nextseq = 0;// 下一个要发送的序列号初始化为0
    sock->window.wnd_send->ack_cnt = 0;// 未确认的ack计数初始化为0
    pthread_mutex_init(&(sock->window.wnd_send->ack_cnt_lock),NULL);// 初始化ack计数锁
    sock->window.wnd_send->rwnd = 32*MAX_DLEN;// 接收方窗口大小初始化
    //sock->window.wnd_send->congestion_status = SLOW_START;
    //sock->window.wnd_send->ssthresh = TCP_RECVWN_SIZE ;

    //初始化接收窗口
    sock->window.wnd_recv = (receiver_window_t*)malloc(sizeof(receiver_window_t));
    sock->window.wnd_recv->expect_seq = 0;// 期待接收的序列号初始化为0
    // for(int i=0;i<TCP_RECVWN_SIZE;i++){
    //     sock->window.wnd_recv->marked[i] = 0;//初始标记每一个B的位置都没有数据
    //     sock->window.wnd_recv->buf[i] = 0;//初始化每一个B的位置都是空的
    // }


    // sock->synqueue=(tju_tcp_t*)malloc(32*sizeof(tju_tcp_t));
    // sock->acceptqueue=(tju_tcp_t*)malloc(32*sizeof(tju_tcp_t));
    

    //初始化计时器
    sock->window.wnd_send->estmated_rtt =0;// 估计的往返时间初始化为0
    sock->window.wnd_send->clk = FALSE;// 时钟标志初始化为FALSE
    sock->window.wnd_send->retran[0]=NULL;// 重传缓冲区初始化为空
    
    // timerclear(&(sock->window.wnd_send->estmated_rtt));         // estmated_rtt初值
    // timerclear(&(sock->window.wnd_send->DevRTT));               // Dev_RTT初值
    // sock->window.wnd_send->RTO.tv_sec = 0;                      // RTO
    // sock->window.wnd_send->RTO.tv_usec = 15000;
    // gettimeofday(&(sock->timer.now_time), NULL);
    // sock->timer.timer_state=0;
    // gettimeofday(&(sock->timer.set_time), NULL);

    // pthread_t send_t;   // 发送线程
    // pthread_create(&send_t, NULL, send_thread, (void*)sock);    // 创建发送线程

    // sock->sending_buf = malloc(50*1024*1024);                   // 静态分配缓冲区大小
    // memset(sock->sending_buf, 0, 50*1024*1024);                 // 缓冲区清空(注意这里至关重要)
    // sock->endDataPoint = sock->sending_buf;                     // 设置缓冲区尾指针
    // sock->lastByteSentPoint = sock->sending_buf;                // 发送端尾指针初始化

    sock->window.wnd_send->nextack = 0; // 下一个期待确认的序列号初始化为0                    
    sock->window.wnd_recv->expect_seq = 0;
    for(int i=0;i<MAX_LEN;i++){ //失序报文缓冲区为空
    sock->window.wnd_recv->outoforder[i]=NULL;
    }

    sock->close_same =0;// 关闭标志初始化为0

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr){
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t* sock){
    // 记录事件跟踪
    serverfp = fopen("/vagrant/tju_tcp/test/server.event.trace", "w");
    if (serverfp == NULL)
    {
        printf("error open the file\n");
        exit(-1);
    }

    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock;// 将套接字添加到监听套接字表中
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
    tju_tcp_t* new_conn ;
    // //这个新socket是记录server和client的连接
    // memcpy(new_conn, listen_sock, sizeof(tju_tcp_t));

    tju_sock_addr local_addr, remote_addr;
    /*
     这里涉及到TCP连接的建立
     正常来说应该是收到客户端发来的SYN报文
     从中拿到对端的IP和PORT
     换句话说 下面的处理流程其实不应该放在这里 应该在tju_handle_packet中
    */ 
    // remote_addr.ip = inet_network("172.17.0.2");  //具体的IP地址
    // remote_addr.port = 5678;  //端口

    // local_addr.ip = listen_sock->bind_addr.ip;  //具体的IP地址
    // local_addr.port = listen_sock->bind_addr.port;  //端口

    // new_conn->established_local_addr = local_addr;
    // new_conn->established_remote_addr = remote_addr;

    while(acceptqueue_n==0){
        //堵塞
    };
    //否则，全连接列表里面有sock,接收一个
    acceptqueue_n--;
    for(int i=0;i<MAX_SOCK;i++){
        if(acceptqueue[i]!=NULL){
            established_socks[i] = acceptqueue[i];// 将接受的连接放入已建立的连接表中
            acceptqueue[i]=NULL;
            new_conn = established_socks[i];// 设置新连接为当前连接
            break;
        }
    }

    
    // 这里应该是经过三次握手后才能修改状态为ESTABLISHED
    // new_conn->state = ESTABLISHED;

    // 将新的conn放到内核建立连接的socket哈希表中
    // int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
    // established_socks[hashval] = new_conn;

    // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
    // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
    // 每次调用accept 实际上就是取出这个队列中的一个元素
    // 队列为空,则阻塞 

    // printf("即将返回new—_conn（server和client之间的TCP\n");
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

    sock->established_remote_addr = target_addr;

    tju_sock_addr local_addr;
    local_addr.ip = inet_network(CLIENT_IP);
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    // 将建立了连接的socket放入内核 已建立连接哈希表中
    int hashval = cal_hash(local_addr.ip, local_addr.port,target_addr.ip, target_addr.port);
    established_socks[hashval] = sock;

    struct timeval start_time, end_time;
    long timeout = 100000L;

    sock->established_local_addr = local_addr;
    sock->established_remote_addr = target_addr;

    //start_timer(sock);

    printf("--------第一次握手--------\n");

    srand(time(0));
    uint32_t j=(rand()%(64-0))+0;//随机产生序号【0,64】序号应该比窗口大两倍
    printf("随机产生序号j:%d \n",j);
    char* syn = create_packet_buf(local_addr.port,target_addr.port,j,1,DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,SYN_FLAG_MASK,TCP_RECVWN_SIZE,0,NULL,0) ;
    printf("one:client send a SYN(shakehand1)\n");
    sendToLayer3(syn ,DEFAULT_HEADER_LEN);
    sock->state = SYN_SENT;
    printf("--------等待SYN确认--------\n\n");

    //对于第一次握手进行备份
    sock->sending_buf = syn;// 将SYN包备份到发送缓冲区
    sock->window.wnd_send->base=j;// 设置发送窗口的基序号
    sock->window.wnd_send->nextseq=j;// 设置下一个要发送的序列号
    sock->window.wnd_send->nextack=j+1;// 设置下一个期待确认的序列号


    // while (time_interval(sock) <= MAX_TIME)
    // {
    //     while(sock->state != ESTABLISHED);//堵塞
    //     // printf("connect成功，返回0\n");
    //     return 0;
    // }

    //否则超时重传
    // tju_connect(sock,target_addr);
    
    while(sock->state != ESTABLISHED);//堵塞
        // printf("connect成功，返回0\n");
        // return 0;

    // while (sock->state!=ESTABLISHED)
    // {   
        // //usleep(50000);
        // gettimeofday(&end_time,NULL);
        // long Time = 1000000L*(end_time.tv_sec-start_time.tv_sec)+(end_time.tv_usec-start_time.tv_usec);
        // if(Time>=timeout){
        //     gettimeofday(&start_time,NULL);
        //     sendToLayer3(SYNpkt,DEFAULT_HEADER_LEN);
        //     printf("超时，重传SYN报文");
        // }
    // }
    pthread_t id = 1500;
    int rst = pthread_create(&id ,NULL,startTimer,(void*)sock);
        if (rst<0){
        printf("ERROR open time thread");
        exit(-1); 
    }
    return 0;

}

void* startTimer(void* arg){
    tju_tcp_t * sock =(tju_tcp_t*)arg;
    struct timeval nowtime;
    while(1){
        if(sock->window.wnd_send->clk==FALSE){
            // 等待时钟启动
        }
        else{
            gettimeofday(&nowtime,NULL);
            // 计算当前时间与上次发送时间的差值
            long Time = 1000000L*(nowtime.tv_sec-sock->window.wnd_send->send_time.tv_sec)+
            (nowtime.tv_usec-sock->window.wnd_send->send_time.tv_usec);
            if(Time>18000L){ //时间大于估计RTT
                printf("重传包%d",get_seq(sock->window.wnd_send->retran[0]));
                sendToLayer3(sock->window.wnd_send->retran[0],get_plen(sock->sending_buf));
                gettimeofday(&sock->window.wnd_send->send_time,NULL);
            }
        }
    }
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    // char* data = malloc(len);
    // memcpy(data, buffer, len);

    // char* msg;
    // uint32_t seq = 0;
    // uint16_t plen = DEFAULT_HEADER_LEN + len;

    // msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
    //           DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);
    
    // //printf("未成功分片？\n");

    // sendToLayer3(msg, plen);

    if(len<=MAX_DLEN){
        CreatePKTandsend(sock,buffer,len);
    }
    else{
    int index = 0;
    while((len-index*MAX_DLEN)>MAX_DLEN){
        char * msg = malloc(MAX_DLEN);
        memcpy(msg,buffer+index*MAX_DLEN,MAX_DLEN);
        CreatePKTandsend(sock,msg,MAX_DLEN);
        index++;
        free(msg);
    }
    char * msg =malloc(len-index*MAX_DLEN);
    memcpy(msg,buffer+index*MAX_DLEN,len-index*MAX_DLEN);
    CreatePKTandsend(sock,msg,len-index*MAX_DLEN);
    free(msg);
    }

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

    return read_len;
}



int tju_handle_packet(tju_tcp_t* sock, char* pkt){//socket sock收到一个pkt

    uint8_t flags = get_flags(pkt);   
    uint16_t src = get_src(pkt);
    uint16_t dst = get_dst(pkt);
    uint32_t seq = get_seq(pkt);   
    uint32_t ack = get_ack(pkt);  
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;
   
    //服务端server
    if(sock->state == LISTEN){      //一定是server正在监听SYN
        //如果收到不是SYN或者目的地不是server，忽视它
        if(flags != SYN_FLAG_MASK || dst != sock->bind_addr.port)return 0;

        //否则，分配new_sock建立连接
        tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));

        //gettimeofday(&(sock->timer.now_time), NULL);
        // if(time_interval(sock) > MAX_TIME){           
        //     sock->state=CLOSED;
        //     sock= tju_socket();
        //     end_timer(sock);
        // }
        // end_timer(sock);
        // start_timer(new_conn);
        // start_timer(sock);

        sock->established_local_addr.ip = sock->bind_addr.ip;
        sock->established_local_addr.port = sock->bind_addr.port;
        sock->established_remote_addr.port = src;
        sock->established_remote_addr.ip = inet_network(CLIENT_IP);
        memcpy(new_conn, sock, sizeof(tju_tcp_t));
        
        //更改状态，sock等待SYN确认
        new_conn->state = SYN_RECV;
        sock->state = SYN_RECV;

        //把new_conn放到半连接列表
        int hashval=cal_hash(new_conn->established_local_addr.ip,new_conn->established_local_addr.port,new_conn->established_remote_addr.ip,new_conn->established_remote_addr.port);
        synqueue[hashval]=new_conn;

        printf("--------第二次握手--------\n");        
        uint32_t k=(rand()%(64-0))+0;//随机产生序号【0,64】
        // printf("dest_port:%d \n",src);
        char* syn = create_packet_buf(dst,src,k,seq+1,
                                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                    ACK_FLAG_MASK+SYN_FLAG_MASK,TCP_RECVWN_SIZE,0,NULL,0) ;
        printf("src:%d\tseq:%d\t ack:%d\t flags:%d\n",dst,k,seq+1,flags);
        printf("two:server send a SYN(shakehand2)\n");
        sendToLayer3(syn ,DEFAULT_HEADER_LEN);
        //给第二次握手做备份
        sock->sending_buf=syn;
        sock->window.wnd_send->base = k;
        sock->window.wnd_send->nextseq = k;
        sock->window.wnd_send->nextack = k+1;
        printf("--------等待ACK确认--------\n\n");

    }
    
    else if(sock->state == SYN_RECV){
        //一定是server在等待第三次握手
        printf("--------收到ACK--------\n");   
        if(ack != sock->window.wnd_send->base+1 || dst != sock->bind_addr.port || flags!= ACK_FLAG_MASK ){
             printf("该数据包不是new期待的ack，忽略\n");
            // printf("sock->bind_addr.port:%d\t\n",sock->bind_addr.port);
            // printf("dst:%d\t ack:%d\t flags:%d\n",dst,ack,flags);
            return 0;  
        }     
        
        // tju_sock_addr local_addr, remote_addr;        
        // remote_addr.ip = inet_network("172.17.0.2");  //具体的IP地址
        // remote_addr.port = src;  //端口
        // local_addr.ip = sock->bind_addr.ip;  //具体的IP地址
        // local_addr.port = sock->bind_addr.port;  //端口

        //半连接删除，全连接放入
        int hashval=cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, sock->established_remote_addr.ip, sock->established_remote_addr.port);

        acceptqueue[hashval]=synqueue[hashval];
        // printf("acceptqueue_n:%d\n",acceptqueue_n);
        synqueue[hashval]=NULL;
        
        sock->state = ESTABLISHED;
        acceptqueue[hashval]->state = ESTABLISHED;
        sock->window.wnd_recv->expect_seq = seq+1;
        acceptqueue_n++;
        printf("--------建立连接--------\n\n");
    }
    //客户端client
    else if(sock->state == SYN_SENT){//一定是客户端在等第二次握手的syn|ack
        printf("--------第三次握手--------\n"); 
        if(dst != sock->established_local_addr.port || ack != sock->window.wnd_send->base+1 || flags != ACK_FLAG_MASK+SYN_FLAG_MASK){
            printf("dst:%d\t ack:%d\t flags:%d\n",dst,ack,flags);
            printf("该数据包不是第三次握手期待数据包，忽略\n");
            return 0;
        }

        //int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, inet_network("172.17.0.3"), src);
        //established_socks[hashval] = sock;       
        //该ack是否携带数据都可以
        char* syn_ack = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,ack,seq+1,
                                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                    ACK_FLAG_MASK,0,0,NULL,0) ;
        printf("dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,ack,seq+1,flags);
        printf("three:client send a ACK(shakehand3)\n");

        // printf("src:%d\n",sock->established_local_addr.port);
        // printf("content:%s\n",syn_ack);
        sendToLayer3(syn_ack ,DEFAULT_HEADER_LEN);
        sock->window.wnd_send->base=ack;
        sock->window.wnd_send->nextseq=ack+1;

        //更改状态，client确认TCP建立成功
        sock->state = ESTABLISHED;
        printf("--------建立连接--------\n\n");
    }

    else if(sock->state==ESTABLISHED){
        char hostname[8];
        gethostname(hostname,8);

        //服务端
        if(strcmp(hostname,"server")==0){

           //服务端收到普通包
           if(get_flags(pkt)==ACK_FLAG_MASK){
            printf("收到序号为%d的包\n",get_seq(pkt));
              if(get_seq(pkt) < sock->window.wnd_recv->expect_seq){//序号小于期望的序号，丢弃这个包，发送一个ack告知发送端期望序号
                 printf("序号%d小于期望%d，丢弃\n",get_seq(pkt),sock->window.wnd_recv->expect_seq);
                 sock->window.wnd_send->nextseq = get_ack(pkt);             
                 sendToLayer3(getExpectedpkt(sock),DEFAULT_HEADER_LEN);
                 printf("发回ack%d\n",sock->window.wnd_recv->expect_seq);
              }
              else if(get_seq(pkt)==sock->window.wnd_recv->expect_seq){ //序号等于期望的序号，接收这个包，放入缓冲区
                 sock->window.wnd_send->nextseq =get_ack(pkt);
                 printf("序号%d满足要求\n",get_seq(pkt));
                 if(getrecvsize(sock)+get_plen(pkt)>5000*MAX_LEN){
                    printf("接收缓冲区已满，丢弃分组");
                    sendToLayer3(getExpectedpkt(sock),DEFAULT_HEADER_LEN);
                    return 0;
                 }
            
                 if(Telloutoforder(sock)==0){ //失序报文缓冲区为空
                    printf("之前无失序报文，直接放入顺序缓存");
                    PutDataToBuf(sock,pkt);
                    fprintf(serverfp, "[%ld] [DELV] [seq:%d size:%d]\n", getctime(), get_seq(pkt), get_plen(pkt) - DEFAULT_HEADER_LEN);
                    sock->window.wnd_recv->expect_seq+= get_plen(pkt)-DEFAULT_HEADER_LEN;
                    char* msg= getExpectedpkt(sock);
                    sendToLayer3(msg,DEFAULT_HEADER_LEN);
                    printf("发送ACK%d\n",sock->window.wnd_recv->expect_seq);
                 }
                 else{  //失序报文段缓冲区里有报文
                    //这时要将失序缓冲区与该包序号连续的报文取出发送到顺序缓冲区
                    printf("之前有失序报文段，整理");
                    int count = Telloutoforder(sock);
                    printf("失序报文段的数量为%d\n",count);
                    PutDataToBuf(sock,pkt);
                    fprintf(serverfp, "[%ld] [DELV] [seq:%d size:%d]\n", getctime(), get_seq(pkt), get_plen(pkt) - DEFAULT_HEADER_LEN);
                    sock->window.wnd_recv->expect_seq+=get_plen(pkt)-DEFAULT_HEADER_LEN;
                    for(int i = 0;i < count;i++){ //将失序缓冲区按序号排序,将序号相连的送入顺序缓冲区
                        int k = i;
                        for(int j = i + 1;j < count;j++){
                        if(get_seq(sock->window.wnd_recv->outoforder[j])<
                           get_seq(sock->window.wnd_recv->outoforder[k])){
                            k = j;
                           }
                        }
                        char* temp =malloc(get_plen(sock->window.wnd_recv->outoforder[i]));
                        memcpy(temp,sock->window.wnd_recv->outoforder[i],get_plen(sock->window.wnd_recv->outoforder[i]));
                        free(sock->window.wnd_recv->outoforder[i]);
                        sock->window.wnd_recv->outoforder[i]=sock->window.wnd_recv->outoforder[k];
                        sock->window.wnd_recv->outoforder[k]=temp;
                        if(get_seq(sock->window.wnd_recv->outoforder[i])==sock->window.wnd_recv->expect_seq){
                            PutDataToBuf(sock,sock->window.wnd_recv->outoforder[i]);
                            fprintf(serverfp, "[%ld] [DELV] [seq:%d size:%d]\n", getctime(), sock->window.wnd_recv->expect_seq, get_plen(sock->window.wnd_recv->outoforder[i]));
                            sock->window.wnd_recv->expect_seq+=get_plen(sock->window.wnd_recv->outoforder[i])-DEFAULT_HEADER_LEN;
                        }
                        else{
                            printf("here\n");
                            break;
                        }
                    }
                    printf("所有满足条件的报文都存入缓冲区");
                    //接下来恢复无序缓冲区，使得其依然从游标0开始存储报文。
                    int index = count;
                    for(int i=0;i<count;i++){
                        if(get_seq(sock->window.wnd_recv->outoforder[i])<sock->window.wnd_recv->expect_seq){
                        free(sock->window.wnd_recv->outoforder[i]);
                        sock->window.wnd_recv->outoforder[i]=NULL;
                        }
                        else {
                            index = i;
                            break;
                        }
                    }
                    if(index!=0){
                    for(int i=index;i<count;i++){
                        sock->window.wnd_recv->outoforder[i-index]=sock->window.wnd_recv->outoforder[i];
                        sock->window.wnd_recv->outoforder[i]=NULL;
                    }
                    }
                    printf("发送ACK%d\n",sock->window.wnd_recv->expect_seq);
                    sendToLayer3(getExpectedpkt(sock),DEFAULT_HEADER_LEN);
                 }
              }
              else{ //序号大于期望的序号,放入失序缓冲区。
                printf("收到失序报文段");
                sock->window.wnd_send->nextseq = get_ack(pkt);
                int count =Telloutoforder(sock);
                if(count==MAX_LEN){
                    printf("存放失序报文的缓冲区已满");
                    printf("发送ack%d\n",sock->window.wnd_recv->expect_seq);
                    sendToLayer3(getExpectedpkt(sock),DEFAULT_HEADER_LEN);
                    return 0;
                }
                else{
                sock->window.wnd_recv->outoforder[count]=malloc(get_plen(pkt));
                memcpy(sock->window.wnd_recv->outoforder[count],pkt,get_plen(pkt));
                printf("缓存的包为%s\n",sock->window.wnd_recv->outoforder[count]);
                //if(count<=4){
                printf("发送ack%d\n",sock->window.wnd_recv->expect_seq);
                sendToLayer3(getExpectedpkt(sock),DEFAULT_HEADER_LEN);
                //}
                /*else{
                    printf("防止出现过多冗余ack,缓存后不发送ACK");
                }
                */
              }
           }
          }
            else if(get_flags(pkt) == FIN_FLAG_MASK){
                if(sock->close_same ==0){
                    printf("--------第二次挥手--------\n");
                    // if(dst != sock->established_local_addr.port || flags != FIN_FLAG_MASK){
                    //     printf("dst:%d\t seq:%d\t ack:%d\t flags:%d\n",dst,seq,ack,flags);
                    //     printf("该数据包不是第二次挥手期待数据包，忽略\n");
                    //     return 0;
                    // }
                    while(sock->received_len>0);//堵塞
                    char* ack_pkt = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,
                                                DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                                ACK_FLAG_MASK,0,0,NULL,0) ;
                    printf("dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,ack,seq+1,flags);
                    printf("two: send a ACK(whand2)\n\n");

                    // printf("src:%d\n",sock->established_local_addr.port);
                    // printf("content:%s\n",syn_ack);
                    sendToLayer3(ack_pkt ,DEFAULT_HEADER_LEN);
                    sock->window.wnd_send->base=sock->window.wnd_send->nextseq;
                    sock->window.wnd_recv->expect_seq=seq+1;
                    sock->window.wnd_send->nextseq++;

                    sock->state = CLOSE_WAIT;

                    sleep(1);
                    printf("--------第三次挥手--------\n");
                    char* fin_ack = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,
                                                DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                                FIN_FLAG_MASK+ACK_FLAG_MASK,0,0,NULL,0) ;
                    printf("dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,ack,seq+1,flags);
                    printf("three: send a FIN_ACK(whand3)\n\n");

                    sendToLayer3(fin_ack ,DEFAULT_HEADER_LEN);
                    sock->window.wnd_send->base=sock->window.wnd_send->nextseq;
                    sock->window.wnd_recv->expect_seq=seq+1;
                    sock->window.wnd_send->nextseq++;

                    sock->state = LAST_ACK;            
                }
                else{
                    if(dst != sock->established_local_addr.port || flags != FIN_FLAG_MASK){
                        printf("dst:%d\t seq:%d\t ack:%d\t flags:%d\n",dst,seq,ack,flags);
                        printf("该数据包不是期待的FIN，忽略\n");
                        return 0;
                    }
                    while(sock->received_len>0);//堵塞

                    printf("--------第三次挥手--------\n");
                    char* fin_ack = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,
                                                DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                                FIN_FLAG_MASK+ACK_FLAG_MASK,0,0,NULL,0) ;
                    printf("dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,FIN_FLAG_MASK+ACK_FLAG_MASK);
                    printf("three: send a FIN_ACK(whand3)\n\n");

                    sendToLayer3(fin_ack ,DEFAULT_HEADER_LEN);
                    sock->window.wnd_send->base=sock->window.wnd_send->nextseq;
                    sock->window.wnd_recv->expect_seq=seq+1;
                    sock->window.wnd_send->nextseq++;

                    sock->state = LAST_ACK; 

                    fprintf(stdout, "--------第二次挥手--------\n");
                    char* ack_pkt = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,
                                                DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                                ACK_FLAG_MASK,0,0,NULL,0) ;
                    printf("dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,ACK_FLAG_MASK);
                    printf("two: send a ACK(whand2)\n\n");

                    // printf("src:%d\n",sock->established_local_addr.port);
                    // printf("content:%s\n",syn_ack);
                    sendToLayer3(ack_pkt ,DEFAULT_HEADER_LEN);
                    sock->window.wnd_send->nextseq++;
                                                           
                }
            }

          
        }
        //客户端
        else{//客户端收到ACK
            if(get_flags(pkt)==ACK_FLAG_MASK){
                printf("客户端收到ack：%d,此时发缓base为%d\n",get_ack(pkt),sock->window.wnd_send->base);
                //没有待确认数据 或者 ack<base
                if(sock->window.wnd_send->base==sock->window.wnd_send->nextseq || sock->window.wnd_send->base>get_ack(pkt)){
                    fprintf(stdout, "目前没有数据需要确认");
                    return 0;
                }

                if(get_ack(pkt)==sock->window.wnd_send->base){
                //收到的ack==base 冗杂ack
                 sock->window.wnd_send->ack_cnt++; //冗余ACK计数
                 printf("收到冗杂ACK,ack_cnt=%d\n",sock->window.wnd_send->ack_cnt);
                 sock->window.wnd_send->rwnd = get_advertised_window(pkt);
                 if(sock->window.wnd_send->ack_cnt==3){
                    fprintf(stdout, "进入快速重传");
                    printf("重传序号为%d的包",get_seq(sock->window.wnd_send->retran[0]));
                    sendToLayer3(sock->window.wnd_send->retran[0],get_plen(sock->sending_buf));
                    sock->window.wnd_send->ack_cnt=0;
                    gettimeofday(&sock->window.wnd_send->send_time,NULL);
                 }
                 return 0;
              }
              
              //收到的ack大于等于最小未被确认的包的序号，累积确认
              fprintf(stdout, "收到期望ack，清理发送缓冲区\n");
              uint32_t newseq = get_ack(pkt);
              sock->window.wnd_send->base = get_ack(pkt);

              //更新sending_buf 和sending_len  
              while(pthread_mutex_lock(&(sock->send_lock))!=0);//加锁
              while(sock->sending_buf!=NULL && get_seq(sock->sending_buf) < newseq){
                int tempsize =sock->sending_len-get_plen(sock->sending_buf);//计算待确认的空间的大小
                if(tempsize==0){
                    free(sock->sending_buf);
                    sock->sending_buf = NULL;
                    sock->sending_len = 0;
                }
                else{
                char * temp = malloc(tempsize);
                memcpy(temp,sock->sending_buf+get_plen(sock->sending_buf),tempsize);
                free(sock->sending_buf);
                sock->sending_buf = temp;//将新开辟的tempsize长度的空间设置为缓冲区
                sock->sending_len =tempsize;
                }
              }
              pthread_mutex_unlock(&(sock->send_lock));//解锁
              
              printf("发送窗口：base：%d, next_seq=%d\n",sock->window.wnd_send->base,sock->window.wnd_send->nextseq);
              if(sock->window.wnd_send->base==sock->window.wnd_send->nextseq){
                sock->window.wnd_send->clk = FALSE;
              }
              else{
                gettimeofday(&sock->window.wnd_send->send_time,NULL);
                free(sock->window.wnd_send->retran[0]);
                sock->window.wnd_send->retran[0] = malloc(get_plen(sock->sending_buf));
                memcpy(sock->window.wnd_send->retran[0],sock->sending_buf,get_plen(sock->sending_buf));
                sock->window.wnd_send->ack_cnt=0;
              }
              sock->window.wnd_send->rwnd = get_advertised_window(pkt);
              
            //todo:检查base有无更新，对定时器进行操作
            }
              printf("发送窗口：sending_len=%d\n",sock->sending_len);

        }
    }
    else if(sock->state == FIN_WAIT_1){
        fprintf(stdout, "--------FIN1确认--------\n"); 
        printf("收到数据包dst:%d\tseq:%d\t ack:%d\t flags:%d\n",dst,seq,ack,flags);

        if(flags == ACK_FLAG_MASK && ack==sock->window.wnd_send->base+1){
            sock->window.wnd_send->base++;
            sock->state = FIN_WAIT_2;
            printf("收到ACK\n");
            printf("--------client state FIN_WAIT_2--------\n\n"); 

            
        }else if(flags == ACK_FLAG_MASK+FIN_FLAG_MASK){
            printf("收到FIN_ACK\n");
            char* ack_pkt = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,
                            DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                            ACK_FLAG_MASK,0,0,NULL,0) ;
            printf("发送ack数据包dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,sock->window.wnd_send->nextseq,seq+1,flags);
            printf("four: send a ACK(whand4)\n");     // printf("src:%d\n",sock->established_local_addr.port);
            // printf("content:%s\n",syn_ack);
            sendToLayer3(ack_pkt ,DEFAULT_HEADER_LEN);
            sock->state = CLOSING;
            printf("--------client state CLOSING，等待ack--------\n\n"); 

        }
    }
    else if(sock->state == FIN_WAIT_2){
        printf("--------FIN2确认--------\n");
        if(dst != sock->established_local_addr.port || flags != ACK_FLAG_MASK+FIN_FLAG_MASK){
            printf("dst:%d\t seq:%d\t ack:%d\t flags:%d\n",dst,seq,ack,flags);
            printf("该数据包不是第四次挥手期待的FIN_ACK，忽略\n");
            return 0;
        }
        
        if(flags == ACK_FLAG_MASK+FIN_FLAG_MASK ){
            char* ack_pkt = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,ack,seq+1,
                                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                    ACK_FLAG_MASK,0,0,NULL,0) ;
            printf("dst:%d\tseq:%d\t ack:%d\t flags:%d\n",sock->established_remote_addr.port,ack,seq+1,flags);
            printf("four: send a ACK(whand4)\n\n");     // printf("src:%d\n",sock->established_local_addr.port);
            // printf("content:%s\n",syn_ack);
            sendToLayer3(ack_pkt ,DEFAULT_HEADER_LEN);
            sock->window.wnd_send->nextack = sock->window.wnd_send->nextseq+1;

            sock->state = TIME_WAIT;
            sleep(1);
            sock->state = CLOSED;
        }

    }  
    else if(sock->state == CLOSING && get_flags(pkt) ==  ACK_FLAG_MASK){
        if(ack == sock->window.wnd_send->base+1){
            sock->state = TIME_WAIT;
            sleep(1);
            sock->state = CLOSED;
        }
    }
    else if(sock->state == LAST_ACK){
        printf("--------whand4_ACK确认--------\n"); 
        if(dst != sock->established_local_addr.port || ack != sock->window.wnd_send->base+1 || flags != ACK_FLAG_MASK){
            printf("dst:%d\t ack:%d\t flags:%d\n",dst,ack,flags);
            fprintf(stdout, "该数据包不是期待的LAST_ACK，忽略\n");
            return 0;
        }
        sock->window.wnd_send->nextseq++;
        sock->state = CLOSED;
    } 

    else{
        PutDataToBuf(sock,pkt);
    }
    return 0;
}

//把收到的数据放到接受缓冲区
void PutDataToBuf(tju_tcp_t *sock, char *pkt)
{
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

    // 把收到的数据放到接受缓冲区
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf == NULL){//如果缓冲区空着，就分配data_len的长度
        sock->received_buf = malloc(data_len);
    }else {//若缓冲区本身有东西，则在此基础上再分配data_len的长度
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    }
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    sock->received_len += data_len;

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁
    return;
}

//封装一个个包，一次就一次性发送一个缓冲区(rwnd)大小的包裹
char *getExpectedpkt(tju_tcp_t *sock){
    char* msg;
    uint32_t seq = sock->window.wnd_send->nextseq;
    uint32_t ack = sock->window.wnd_recv->expect_seq;
    uint8_t flag = 0xff & ACK_FLAG_MASK;
    uint32_t Rwnd = 5000*MAX_LEN -getrecvsize(sock);//接受窗口的大小
    if(Rwnd >= 32*MAX_DLEN)
    Rwnd = 32*MAX_DLEN;
    msg = create_packet_buf(sock->established_local_addr.port,sock->established_remote_addr.port,seq,ack,
                            DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,flag,Rwnd,0,NULL,0);
    
    return msg;
}

//统计接收到的包的大小，并返回包的大小
uint32_t getrecvsize(tju_tcp_t* sock){
    uint32_t count = 0;
    int index = 0;
    while(index<MAX_LEN&&sock->window.wnd_recv->outoforder[index]!=NULL){
        count+=get_plen(sock->window.wnd_recv->outoforder[index]);
        index++;
    }
    count+=sock->received_len;
    return count;
}

//判断有无失序报文，如果失序报文为空且包长度小于MAX_LEN,则index++，返回包的长度index
int Telloutoforder(tju_tcp_t* sock){
    int index = 0;
    while(index<MAX_LEN &&sock->window.wnd_recv->outoforder[index]!=NULL){
        index++;
    } 
    return index;
}

//创建包并发送
void CreatePKTandsend(tju_tcp_t*sock,const void * buffer,int len){
    // printf("创建包并发送\n");
    char *data = malloc(len);
    memcpy(data, buffer, len);

    char *msg;
    uint32_t seq = sock->window.wnd_send->nextseq;
    printf("发送窗口：base:%d,nextseq:%d\n",sock->window.wnd_send->base,sock->window.wnd_send->nextseq);
    uint16_t plen = DEFAULT_HEADER_LEN + len;
    while((sock->window.wnd_send->nextseq - sock->window.wnd_send->base +len)>=sock->window.wnd_send->rwnd);//窗口未满，可以创建包并发送包
    fprintf(stdout, "窗口未满，可以放入包\n");
    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, seq+len,
                            DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, 0, 0, data, len);
    while(pthread_mutex_lock(&sock->send_lock)!=0);//加锁
    if(sock->sending_buf == NULL){
        sock->sending_buf = malloc(plen);
    }
    else{
        sock->sending_buf = realloc(sock->sending_buf,sock->sending_len+plen);
    }
    memcpy(sock->sending_buf + sock->sending_len,msg,plen);
    sock->sending_len+=plen;
    pthread_mutex_unlock(&(sock->send_lock));
    printf("数据包seq=%d放入发送缓冲\n",seq);
    sock->window.wnd_send->nextseq += len;

    //如果要发送的报文是序号最小的未被确认报文段
    if(sock->window.wnd_send->nextseq - sock->window.wnd_send->base==len){
        sock->window.wnd_send->clk=TRUE;
        gettimeofday(&(sock->window.wnd_send->send_time),NULL);
        sock->window.wnd_send->retran[0]=malloc(len+DEFAULT_HEADER_LEN);
        memcpy(sock->window.wnd_send->retran[0],msg,plen);
        sock->window.wnd_send->ack_cnt=0;
    }
    
    sendToLayer3(msg,plen);
    printf("发送数据包序号%d\n",get_seq(msg));
    printf("发送窗口：base:%d,nextseq:%d\n\n",sock->window.wnd_send->base,sock->window.wnd_send->nextseq);

    return;
}

int tju_close (tju_tcp_t* sock){
    //客户端调用close（）
    if(sock->state == ESTABLISHED && sock->established_local_addr.port == 5678){
        //当发送缓冲区内还有数据没发完，堵塞
        while(sock->sending_len >0 );

        // fprintf(stdout, "--------四次挥手--------\n\n");
        // fprintf(stdout, "--------第一次挥手--------\n");
        char* msg = create_packet_buf(sock->established_local_addr.port ,sock->established_remote_addr.port ,sock->window.wnd_send->nextseq,1,
                                    DEFAULT_HEADER_LEN,DEFAULT_HEADER_LEN,
                                    FIN_FLAG_MASK,TCP_RECVWN_SIZE,0,NULL,0) ;
        printf("dst:%d\t seq:%d\t ack:%d\t flags:%d\n",get_dst(msg),sock->window.wnd_send->nextseq,1,FIN_FLAG_MASK);

        fprintf(stdout, "第一次挥手: send a FIN\n");
        sendToLayer3(msg ,DEFAULT_HEADER_LEN);
        sock->state = FIN_WAIT_1;
        //给第一次挥手做备份
        sock->sending_buf=msg;
        sock->window.wnd_send->base = sock->window.wnd_send->nextseq;
        sock->window.wnd_send->nextseq = sock->window.wnd_send->nextseq+1;
        sock->window.wnd_send->nextack = sock->window.wnd_send->nextseq+1;
        fprintf(stdout, "--------等待FIN1确认--------\n\n");
    }
    
    //服务端调用close（）
    else if(sock->state == ESTABLISHED && sock->established_local_addr.port == 1234){
        sleep(1);
        sock->close_same = 1;
    }    


    while(sock->state!=CLOSED);//阻塞
    int hashval = cal_hash(sock->established_local_addr.ip, sock->established_local_addr.port, 
                sock->established_remote_addr.ip, sock->established_remote_addr.port);
    established_socks[hashval] = NULL;
    free(sock);
    sock = NULL;


    return 0;
}


long getctime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

