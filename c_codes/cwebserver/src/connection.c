#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "log.h"
#include "connection.h"
#include "request.h"
#include "response.h"
#include "stringutils.h"

// 关闭连接
void connection_close(connection *con) {
    if (!con) return;

    // 释放连接对应的请求和响应
    http_request_free(con->request);
    http_response_free(con->response);

    // 释放客户端连接中的缓存
    string_free(con->recv_buf);

    // 关闭连接socket
    if (con->sockfd > -1)
        close(con->sockfd);

    free(con);
}

/*
 * 等待并接受新的连接
 */
connection* connection_accept(server *serv) {
    struct sockaddr_in addr;//sockaddr_in这个结构体用来处理网络通信的地址,定义了网络通信地址
    connection *con;
    int sockfd;
    socklen_t addr_len = sizeof(addr);//socket编程中的accept函数的第三个参数的长度必须和int的长度相同。于是便有了socklen_t类型

    // accept() 接受新的连接
    sockfd = accept(serv->sockfd, (struct sockaddr *) &addr, &addr_len);

    if (sockfd < 0) {
        log_error(serv, "accept: %s", strerror(errno));
        perror("accept");
        return NULL;
    }

    // 创建连接结构实例
    con = malloc(sizeof(*con));

    // 初始化连接结构
    con->status_code = 0;
    con->request_len = 0;
    con->sockfd = sockfd;
    con->real_path[0] = '\0';

    con->recv_state = HTTP_RECV_STATE_WORD1;
    con->request = http_request_init();
    con->response = http_response_init();
    con->recv_buf = string_init();
    memcpy(&con->addr, &addr, addr_len);//从源内存地址&addr的起始位置开始拷贝addr_len字节到目标内存地址&con->addr

    return con;
}

/*
 * HTTP请求处理函数
 * - 从socket中读取数据并解析HTTP请求
 * - 解析请求
 * - 发送响应
 * - 记录请求日志
 */
int connection_handler(server *serv, connection *con) {
    char buf[512];
    int nbytes;
    int ret;

    printf("socket: %d\n", con->sockfd);

    while ((nbytes = recv(con->sockfd, buf, sizeof(buf), 0)) > 0) { //返回所有可用的信息，最大可达缓冲区的大小
        string_append_len(con->recv_buf, buf, nbytes);

        if (http_request_complete(con) != 0)//根据HTTP协议验证HTTP请求是否合法
            break;
    }

    if (nbytes <= 0) {
        ret = -1;
        if (nbytes == 0) {
            printf("socket %d closed\n", con->sockfd);
            log_info(serv, "socket %d closed", con->sockfd);

        } else if (nbytes < 0) {
            perror("read");
            log_error(serv, "read: %s", strerror(errno));
        }
    } else {
        ret = 0;
    }

    http_request_parse(serv, con);// 解析HTTP请求
    http_response_send(serv, con);// 发送HTTP响应
    log_request(serv, con);// 记录HTTP请求

    return ret;
}
