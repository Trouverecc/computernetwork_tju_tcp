#include "tju_tcp.h"
#include <string.h>
#include <signal.h>

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

    tju_tcp_t* my_socket = tju_socket();
    // printf("my_tcp state %d\n", my_socket->state);
    
    tju_sock_addr target_addr;
    target_addr.ip = inet_network(SERVER_IP);
    target_addr.port = 1234;

    tju_connect(my_socket, target_addr);
    printf("my_socket state %d, client start\n", my_socket->state);   

    sleep(10000);
    sleep_no_wake(1);
    printf("[断开连接测试-客户端] 调用 tju_close\n");
    tju_close(my_socket);

    printf("[断开连接测试-客户端] 等待10s确保连接完全断开\n");
    sleep_no_wake(10);   


    return EXIT_SUCCESS;
}
