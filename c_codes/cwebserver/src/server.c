#include <sys/types.h> //linux/unix 系统基本系统数据类型的头文件，含有size_t，time_t，pid_t等类型
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/in.h>

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "server.h"
#include "log.h"
#include "connection.h"
#include "config.h"

// 默认端口号
#define DEFAULT_PORT 8080
#define BACKLOG 10

// 子进程信号处理函数
static void sigchld_handler(int s) {
    pid_t pid; //子进程识别码pid
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0); //waitpid()会暂时停止目前进程的执行，直到有信号来到或子进程结束。
}

// 初始化服务器结构体
static server* server_init() {
    server *serv;
    serv = malloc(sizeof(*serv));  //为服务器申请内存空间
    memset(serv, 0, sizeof(*serv)); //为新申请的内存进行初始化工作，memset为初始化内存的“万能函数”
    return serv;
}

// 释放服务器结构体
static void server_free(server *serv) {
    config_free(serv->conf);
    free(serv);
}

// 以守护进程的方式启动
static void daemonize(server *serv, int null_fd) {
    struct sigaction sa;
    int fd0, fd1, fd2;

    //在创建新文件或目录时 屏蔽掉新文件或目录不应有的访问允许权限。
    umask(0);//设置允许当前进程创建文件或者目录最大可操作的权限

    // fork()新的进程
    /*
    1）在父进程中，fork返回新创建子进程的进程ID；
    2）在子进程中，fork返回0；
    3）如果出现错误，fork返回一个负值
    */
    switch(fork()) {
        case 0:
            break;
        case -1:
            log_error(serv, "daemon fork 1: %s", strerror(errno));
            exit(1);
        default:
            exit(0);
    }
    setsid();//setsid函数用于创建一个新的会话，并担任该会话组的组长。

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP,  &sa, NULL) < 0) {
        log_error(serv, "SIGHUP: %s", strerror(errno));
        exit(1);
    }

    switch(fork()) {
        case 0:
            break;
        case -1:
            log_error(serv, "daemon fork 2: %s", strerror(errno));
            exit(1);
        default:
            exit(0);
    }

    chdir("/");  //改变工作目录

    // 关闭标准输入，输出，错误
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    fd0 = dup(null_fd);
    fd1 = dup(null_fd);
    fd2 = dup(null_fd);

    if (null_fd != -1)
        close(null_fd);

    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        log_error(serv, "unexpected fds: %d %d %d", fd0, fd1, fd2);
        exit(1);
    }

    log_info(serv, "pid: %d", getpid());
}

/*
 * 检查日志文件和web文件目录是否在chroot_path下，如果在则执行chroot限制web服务器访问的目录
 */
static void jail_server(server *serv, char *logfile, const char *chroot_path) {
    size_t root_len = strlen(chroot_path);
    size_t doc_len = strlen(serv->conf->doc_root);
    size_t log_len = strlen(logfile);

    // 检查web文件目录是否在chroot_path下
    //strncmp函数若str1与str2的前n个字符相同，则返回0；若s1大于s2，则返回大于0的值；若s1 小于s2，则返回小于0的值。
    if (root_len < doc_len && strncmp(chroot_path, serv->conf->doc_root, root_len) == 0) {
        // 更新web文件目录为chroot_path的相对路径
        //char *strncpy(char *dest, const char *src, int n)
        //把src所指向的字符串中以src地址开始的前n个字节复制到dest所指的数组中，并返回被复制后的dest
        strncpy(serv->conf->doc_root, &serv->conf->doc_root[0] + root_len, doc_len - root_len + 1);
    } else {
        fprintf(stderr, "document root %s is not a sub-directory in chroot %s\n", serv->conf->doc_root, chroot_path);
        exit(1);
    }

    // 检查日志文件是否在chroot_path下
    if (serv->use_logfile) {
        if (logfile[0] != '/')
            fprintf(stderr, "warning: log file is not an absolute path, opening it will fail if it's not in chroot\n");
        else if (root_len < log_len && strncmp(chroot_path, logfile, root_len) == 0) {
            // 更新日志文件为chroot_path的相对路径
            strncpy(logfile, logfile + root_len, log_len - root_len + 1);
        } else {
            fprintf(stderr, "log file %s is not in chroot\n", logfile);
            exit(1);
        }
    }

    // 执行chroot, chroot命令用来在指定的根目录下运行指令
    if (chroot(chroot_path) != 0) {
        perror("chroot");
        exit(1);
    }

    chdir("/");
}

// bind()绑定端口并listen()监听
static void bind_and_listen(server *serv) {
    struct sockaddr_in serv_addr;

    // 创建socket
    serv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv->sockfd < 0) {
        perror("socket");
        log_error(serv, "socket: %s", strerror(errno));
        exit(1);
    }

    int yes = 0;
    if ((setsockopt(serv->sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) { //用于任意类型、任意状态套接口的设置选项值
        perror("setsockopt");
        log_error(serv, "socket: %s", strerror(errno));
        exit(1);
    }

    memset(&serv_addr, 0, sizeof serv_addr);
    serv_addr.sin_family = AF_INET;//sa_family是地址家族，一般都是“AF_xxx”的形式。通常大多用的是都是AF_INET,代表TCP/IP协议族。
    serv_addr.sin_addr.s_addr = INADDR_ANY;//in_addr是一个结构体，可以用来表示一个32位的IPv4地址
    serv_addr.sin_port = htons(serv->port);//将主机字节顺序转换为网络字节顺序
    // bind() 绑定
    if (bind(serv->sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        log_error(serv, "bind: %s", strerror(errno));
        exit(1);
    }

    // listen() 监听，等待客户端连接
    if (listen(serv->sockfd, BACKLOG) < 0) { //sockfd是通过sockfd()函数拿到的fd, backlog允许几路客户端和服务器进行正在连接的过程
        perror("listen");
        log_error(serv, "listen: %s", strerror(errno));
        exit(1);
    }
}

/*
 * 启动并初始化服务器
 */
static void start_server(server *serv, const char *config, const char *chroot_path, char *logfile) {
    int null_fd = -1;

    // 1. 加载配置文件
    serv->conf = config_init();
    config_load(serv->conf, config);

    // 2. 设置端口号
    if (serv->port == 0 && serv->conf->port != 0) {
        serv->port = serv->conf->port;
    }
    else if (serv->port == 0) {
        serv->port = DEFAULT_PORT;
    }

    printf("port: %d\n", serv->port);

    if (serv->is_daemon) {
        null_fd = open("/dev/null", O_RDWR);
    }

    // 3. 判断是否chroot
    if (serv->do_chroot) {
        jail_server(serv, logfile, chroot_path);
    }

    // 4. 打开日志文件
    log_open(serv, logfile);

    // 5. 判断是否以守护进程方式启动
    if (serv->is_daemon) {
        daemonize(serv, null_fd);
    }

    // 6. 绑定并监听
    bind_and_listen(serv);
}

/*
 * 使用fork()新的进程来处理新的客户端连接和HTTP请求
 */
static void do_fork_strategy(server *serv) {
    pid_t pid;//pid_t定义的类型都是进程号类型,表示进程的id
    struct sigaction sa;//支持信号传递信息,实现信号的安装登记
    connection *con;

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);//将信号集初始化为空
    sa.sa_flags = SA_RESTART;//SA_RESTART用在为某个信号设置信号处理函数时，给该信号设置的一个标记,一旦给信号设置了SA_RESTART标记，那么当执行某个阻塞系统调用时，收到该信号时，进程不会返回，而是重新执行该系统调用

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        if ((con = connection_accept(serv)) == NULL) {
            continue;
        }

        if ((pid = fork()) == 0) {

            // 子进程中处理HTTP请求
            close(serv->sockfd);

            connection_handler(serv, con);
            connection_close(con);

            exit(0);
        }

        printf("child process: %d\n", pid);
        connection_close(con);
    }
}

// 主函数
int main(int argc, char** argv) {

    // 初始化服务器
    server *serv;
    char logfile[PATH_MAX];
    char chroot_path[PATH_MAX];
    int opt;

    serv = server_init();

    // 解析命令行参数 int getopt(int argc,char * const argv[], const char * optstring)
    while((opt = getopt(argc, argv, "p:l:r:d")) != -1) {  //如果所有命令行选项都解析完毕，返回 -1
        switch(opt) {     //optarg--指向当前选项参数(如果有)的指针
            // 设置端口号
            case 'p':
                serv->port = atoi(optarg);//把参数optarg所指向的字符串转换为一个整数(类型为int型)
                if (serv->port == 0) {
                    fprintf(stderr, "error: port must be an integer\n");
                    exit(1);
                }
                break;
            // 以守护进程方式启动
            case 'd':
                serv->is_daemon = 1;
                break;
            // 使用日志文件
            case 'l':
                strcpy(logfile, optarg);
                serv->use_logfile = 1;
                break;
            // 使用chroot
            case 'r':
                if (realpath(optarg, chroot_path) == NULL) { //realpath(const char *path, char *resolved_path)
                    perror("chroot"); //参数path所指的相对路径转换成绝对路径，然后存于参数resolved_path所指的字符串数组或指针中的一个函数
                    exit(1);
                }
                serv->do_chroot = 1;
                break;
        }
    }

    // 启动服务
    start_server(serv, "web.conf", chroot_path, logfile);

    // 进入主循环等待并处理客户端连接
    do_fork_strategy(serv);

    // 关闭日志文件
    log_close(serv);

    // 释放服务器结构体
    server_free(serv);

    return 0;
}
