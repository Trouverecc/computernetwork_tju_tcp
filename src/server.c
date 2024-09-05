#include "tju_tcp.h"
#include <string.h>


int TEST_TYPE;  //0 测试先后断开
                //1 测试同时断开

int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();
    // printf("开启仿真环境\n");      

    tju_tcp_t* my_server = tju_socket();
    // printf("my_tcp state %d\n", my_server->state);
    
    tju_sock_addr bind_addr;
    bind_addr.ip = inet_network(SERVER_IP);
    bind_addr.port = 1234;

    tju_bind(my_server, bind_addr);

    tju_listen(my_server);

    tju_tcp_t* new_conn = tju_accept(my_server);
    // printf("new_conn state %d\n", new_conn->state);      

    // uint32_t conn_ip;
    // uint16_t conn_port;

    // conn_ip = new_conn->established_local_addr.ip;
    // conn_port = new_conn->established_local_addr.port;
    // printf("new_conn established_local_addr ip %d port %d\n", conn_ip, conn_port);

    // conn_ip = new_conn->established_remote_addr.ip;
    // conn_port = new_conn->established_remote_addr.port;
    // printf("new_conn established_remote_addr ip %d port %d\n", new_conn->established_remote_addr.ip,new_conn->established_remote_addr.port);
    // printf("new_conn established_local_addr ip %d port %d\n", new_conn->established_local_addr.ip, new_conn->established_local_addr.port);
    // printf("返回new—_conn（server和client之间的TCP\n");
    // printf("client ip %d\n", inet_network("172.17.0.2"));
    // printf("server ip %d\n",inet_network("172.17.0.3"));

    
    sleep(5); 

    
    // tju_send(new_conn, "hello world", 12);
    // tju_send(new_conn, "hello tju", 10);

    // char buf[2021];
    // // tju_recv(new_conn, (void*)buf, 12);
    // // printf("server recv %s\n", buf);

    // // tju_recv(new_conn, (void*)buf, 10);
    // // printf("server recv %s\n", buf);

    printf("--------可靠传输测试--------\n");


    char recv_buf[4096];
    u_int32_t ack, seq;
    while(1){
        tju_recv(new_conn, recv_buf, sizeof(recv_buf));
        ack = get_ack(recv_buf);
        seq = get_seq(recv_buf);
        printf("接收到数据:%s\n",recv_buf);
        break;
    }

    printf("--------close测试--------\n");

    if (argc==2){
        TEST_TYPE = atoi(argv[1]);
        if (TEST_TYPE==0){
            printf("[服务端] 测试双方先后断开连接的情况\n");
            while(new_conn->state != CLOSED){}
            printf("--------服务端已断开--------\n");

        }
        else if (TEST_TYPE==1){
            printf("[服务端] 测试双方同时断开连接的情况\n");
            tju_close(new_conn);
            printf("--------服务端已断开--------\n");

        }
        else{
            printf("[服务端] 未知测试类型\n");
        }
    }
    

    printf("STATE TRANSFORM TO CLOSED\n");   


    return EXIT_SUCCESS;
}
