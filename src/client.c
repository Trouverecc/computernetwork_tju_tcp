#include "tju_tcp.h"
#include <string.h>



void fflushbeforeexit(int signo){
    exit(0);
}

void sleep_no_wake(int sec){  
    do{        
        sec =sleep(sec);
    }while(sec > 0);             
}

int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();
    // printf("开启仿真环境\n");      

    tju_tcp_t* my_socket = tju_socket();
    // printf("my_tcp state %d\n", my_socket->state);
    
    tju_sock_addr target_addr;
    target_addr.ip = inet_network(SERVER_IP);
    target_addr.port = 1234;

    tju_connect(my_socket, target_addr);
    // printf("my_socket state %d\n", my_socket->state);      

    // uint32_t conn_ip;
    // uint16_t conn_port;

    // conn_ip = my_socket->established_local_addr.ip;
    // conn_port = my_socket->established_local_addr.port;
    // printf("my_socket established_local_addr ip %d port %d\n", conn_ip, conn_port);

    // conn_ip = my_socket->established_remote_addr.ip;
    // conn_port = my_socket->established_remote_addr.port;
    // printf("my_socket established_remote_addr ip %d port %d\n", conn_ip, conn_port);
    // printf("my_socket established_remote_addr ip %d port %d\n", my_socket->established_remote_addr.ip,my_socket->established_remote_addr.port);
    // printf("my_socket established_local_addr ip %d port %d\n", my_socket->established_local_addr.ip, my_socket->established_local_addr.port);
    sleep(3);    

    printf("--------可靠传输测试--------\n");

    tju_send(my_socket, "hello world", 12);
    // printf("client send a massege\n");
    // tju_send(my_socket, "hello tju", 10);

    // char buf[10000];
    // FILE* fp = fopen("./test/rdt_send_file.txt", "r");
    // fscanf(fp, "%s", buf);
    // while(1){
    //     getchar();
    //     tju_send(my_socket, buf, strlen(buf));
    // }

    // tju_recv(my_socket, (void*)buf, 12);
    // printf("client recv %s\n", buf);

    // tju_recv(my_socket, (void*)buf, 10);
    // printf("client recv %s\n", buf);
    
    sleep_no_wake(1);
    printf("--------close测试--------\n");

    printf("[断开连接测试-客户端] 调用 tju_close\n");
    tju_close(my_socket);
    printf("--------客户端已断开--------\n");


    printf("[断开连接测试-客户端] 等待10s确保连接完全断开\n");
    sleep_no_wake(10);

    return EXIT_SUCCESS;
}

