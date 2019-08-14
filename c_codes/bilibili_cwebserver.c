/*
时间：2019-8-13
功能：用c语言开发一个最简单的服务器
版本：cwebserver 1.0
*/
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<strings.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<errno.h>
#include<arpa/inet.h>

#define SERV_PORT 8080
#define SERV_IP_ADDR "127.0.0.1"
#define BACKLOG 5
#define QUIT_STR "quit"

int main(void)
{
    int fd=-1;
    struct sockaddr_in sin;
    /*1、创建socket*/
    if( (fd=socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror("socket");
        exit(1);
    }
    /*2、绑定*/
    /*2.1 填充sockaddr_in结构体*/
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERV_PORT);
#if 0
    sin.sin_addr.s_addr = inet_addr(SERV_IP_ADDR);
#else
    if(inet_pton(AF_INET, SERV_IP_ADDR, (void *)&sin.sin_addr)!=1)
    {
        perror("inet_pton");
        exit(1);
    }
#endif
    /*2.2绑定*/
    if( bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("bind");
        exit(1);
    }
    /*3.调用listen()把主动套接字变成被动套接字*/
    if(listen(fd, BACKLOG) < 0)
    {
        perror("listen");
        exit(1);
    }
    /*4.阻塞等待客户端连接请求*/
    int newfd = -1;
    newfd = accept(fd, NULL, NULL);
    if(newfd < 0)
    {
        perror("accept");
        exit(1);
    }
    /*5.读写*/
    //和newfd进行数据读写
    int ret = -1;
    char buf[BUFSIZ];
    while(1){
        bzero(buf, BUFSIZ);
        do
        {
            ret = read(newfd, buf, BUFSIZ-1);
        }while(ret < 0 && EINTR == errno);
        if(ret < 0)
        {
            perror("read");
            exit(1);
        }
        if(!ret)
        {
            break;
        }
        printf("Receive data: %s\n", buf);

        if( !strncasecmp( buf, QUIT_STR, strlen(QUIT_STR) ) )
        {
            printf("Client is existing!\n");
            break;
        }
    }
    close(newfd);
    close(fd);
    return 0;
}
