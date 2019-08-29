/*
时间：2019-8-13
功能：用c语言开发一个最简单的服务器
版本：cwebserver 1.2 允许多个客户端同时访问服务器
对receive data进行解析，并构建response
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
#include<pthread.h>
#include<assert.h>
#include<sys/stat.h>

#define SERV_PORT 8080
#define SERV_IP_ADDR "127.0.0.1"
#define BACKLOG 5
#define QUIT_STR "quit"
#define PATH_MAX 512
#define HEADER_SIZE_INC 64

typedef struct{
    char *ptr;
    size_t size;
    size_t len;
}string;

typedef struct{
    string *key;
    string *value;
}keyvalue;

//http头部
typedef struct{
    keyvalue *ptr;
    size_t len;
    size_t size;
}http_headers;


typedef struct{
    //文件扩展名
    const char *ext;
    //Mine类型名
    const char *mine;
}mine;

//目前支持的MineType
mine mine_types[] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".jpg", "image/jpg"},
    {".png", "image/png"}
};


//定义http 请求结构体
typedef struct{
    char *method;
    char *uri;
    char *version;
    http_headers *headers;
    int content_length;
}http_request;


typedef struct{
    int sockfd;
    int content_length;
    int status_code;
    string *entity_body;
    http_headers *headers;
    char real_path[PATH_MAX];
    char doc_root[PATH_MAX];
}http_response;


void *cli_data_handle(void *arg);

http_request* http_request_init();

void http_request_free(http_request *req);

http_headers* http_headers_init();

void http_headers_free(http_headers *h);

http_response* http_response_init();

void http_response_free(http_response *resp);

int read_file(string *buf, const char *path);

int read_err_file(http_response *resp);

void http_response_send(http_request *req, http_response *resp);

void build_and_send_response(http_response *resp);

const char* reason_phrase(int status_code);

const char* get_mine_type(const char *path, const char *default_mine);

int send_all(http_response *resp, string *buf);

char* match_until(char **buf, const char *delims);

void extend(http_headers *h);

void http_headers_add(http_headers *h, const char *key, const char *value);

void http_headers_add_int(http_headers *h, const char *key, int value);

void http_request_parse(char *buf, http_request *req, http_response *resp);

string* string_init();

void string_extend(string *s, size_t new_len);

int string_copy_len(string *s, const char *str, size_t str_len);

int string_append(string *s, const char *str);

int string_copy(string *s, const char *str);

string* string_init_str(const char *str);

int string_append_string(string *s, string *s2);

int string_append_len(string *s, const char *str, size_t str_len);

int string_append_ch(string *s, char ch);

int string_append_int(string *s, int i);

void string_free(string *s);


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
    if(inet_pton(AF_INET, SERV_IP_ADDR, (void *)&sin.sin_addr)!=1)
    {
        perror("inet_pton");
        exit(1);
    }

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
    /*优化：使用多线程*/
    int newfd = -1;

    pthread_t tid;
    struct sockaddr_in cin;
    socklen_t addrlen = sizeof(cin);
    while(1)
    {
        newfd = accept(fd, (struct sockaddr *)&cin, &addrlen);
        if(newfd < 0)
        {   
            perror("accept");
            exit(1);
        }

        char ipv4_addr[16];
        if( !inet_ntop(AF_INET, (void *)&cin.sin_addr, ipv4_addr, sizeof(cin)))
        {
            perror("inet_ntop");
            exit(1);
        }
        
        pthread_create(&tid, NULL, cli_data_handle, (void *)&newfd);
    }
    
    close(fd);
    return 0;
}

void *cli_data_handle(void *arg)
{
    int newfd = *(int *)arg;
    http_request *req = http_request_init();
    http_response *resp = http_response_init();
    resp->sockfd = newfd;
    int ret = -1;
    char buf[BUFSIZ];//BUFSIZ represent default cache size 8192
    
    while(1)
    {
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
        req->content_length = strlen(buf);

        //解析http请求
        http_request_parse(buf, req, resp);
       
        //send http response
        http_response_send(req,resp);

        if( !strncasecmp( buf, QUIT_STR, strlen(QUIT_STR) ) )
        {
            printf("Client is existing!\n");
            break;
        }
        
        http_request_free(req);
        http_response_free(resp);
    }
    close(newfd);
}

http_request* http_request_init()
{
    http_request *req;
    req = malloc(sizeof(*req));
    req->content_length = 0;
    req->version = 0;
    req->headers = http_headers_init();

    return req;
}

void http_request_free(http_request *req)
{
    if(!req) return;

    http_headers_free(req->headers);
    free(req);
}

http_headers* http_headers_init()
{
    http_headers *h;
    h = malloc(sizeof(*h));
    memset(h, 0, sizeof(*h));
    
    return h;
}

void http_headers_free(http_headers *h)
{
    if(!h) return;

    for(size_t i = 0; i < h->len; i++)
    {
        string_free(h->ptr[i].key);
        string_free(h->ptr[i].value);
    }

    free(h->ptr);
    free(h);
}

http_response* http_response_init()
{
    http_response *resp;
    resp = malloc(sizeof(*resp));
    memset(resp, 0, sizeof(*resp));

    resp->headers = http_headers_init();
    resp->entity_body = string_init();
    resp->content_length = -1;
    resp->real_path[0] = '\0';
    resp->doc_root[0] = '\0';

    return resp;
}

void http_response_free(http_response *resp)
{
    if(!resp) return;

    http_headers_free(resp->headers);
    string_free(resp->entity_body);

    free(resp);
}

//解析uri,获得文件在服务器上的绝对路径
int resolve_uri(char *resolved_path, char *root, char *uri)
{
    int ret = 0;
    string *path = string_init_str(root);
    string_append(path, uri);

    char *res = realpath(path->ptr, resolved_path);
    
    if(!res)
    {
        ret = -1;
        goto cleanup;
    }

    size_t resolved_path_len = strlen(resolved_path);
    size_t root_len = strlen(root);
    
    if(resolved_path_len < root_len){
        ret = -1;
    }
    else if(uri[0] == '/' && uri[1] == '\0')
    {
        strcat(resolved_path, "/index.html");
    }

cleanup:
    string_free(path);
    return ret;
}

void http_request_parse(char *buf, http_request *req, http_response *resp)
{
    req->method=match_until(&buf, " ");
    req->uri=match_until(&buf, " ");
    req->version=match_until(&buf, "\r\n");

    //根据uri判断要访问的资源是否在服务器上
    struct stat st;
    if(stat("./cwebserver/www", &st) == 0)
    {
        if(!S_ISDIR(st.st_mode))
        {
            perror("invalid directory");
            exit(1);
        }
    }
    else
    {
        perror("get file state fail!");
        exit(1);
    }
    realpath("./cwebserver/www", resp->doc_root);
    if(resolve_uri(resp->real_path, resp->doc_root, req->uri) == -1)
    {
        resp->status_code = 404;
        perror("404: not found");
    }

    //解析http请求头部
    char *p=buf;
    char *endp=buf+req->content_length;

    while(p < endp)
    {
        const char *key = match_until(&p, ": ");
        const char *value = match_until(&p, "\r\n");
        
        if(!key || !value)
        {
            resp->status_code = 400;
            return;
        }

        http_headers_add(req->headers, key, value);
    }
    resp->status_code = 200;
}

int read_file(string *buf, const char *path)
{
    FILE *fp;
    int fsize;
    fp = fopen(path, "r");
    if(!fp)
        return -1;
    
    //将fp移动到距离文件结尾0 offset.
    fseek(fp, 0, SEEK_END);//SEEK_END 代表文件wei
    //得到fp当前位置相对于文件首的字节number.
    fsize = ftell(fp);
    //将fp移动到文件开头
    fseek(fp, 0, SEEK_SET);//SEEK_SET 代表文件头

    string_extend(buf, fsize+1);

    if(fread(buf->ptr, fsize, 1, fp) > 0)
    {
        buf->len += fsize;
        buf->ptr[buf->len]='\0';
    }

    fclose(fp);

    return fsize;
}

//出错页面
const char *default_err_msg = "<HTML><HEAD><TITLE>Error</TITLE></HEAD>"
                              "<BODY><H1>Something went wrong</H1>"
                              "</BODY></HTML>";

int read_err_file(http_response *resp)
{
        int len;
        string_append(resp->entity_body, default_err_msg);
        len = strlen(resp->entity_body->ptr);

        return len;
}

void http_response_send(http_request *req, http_response *resp)
{
    http_headers_add(resp->headers, "Server", "cserver");
    if(strcasecmp(req->method, "GET") == 0)
    {
        if(read_file(resp->entity_body, resp->real_path) == -1)
        {
            resp->status_code = 404;
            read_err_file(resp);
        }
    }
        
    
    //构建消息头部
    const char *mine = get_mine_type(resp->real_path, "text/plain");
    http_headers_add(resp->headers, "Content-Type", mine);
    resp->content_length = strlen(resp->entity_body->ptr);
    http_headers_add_int(resp->headers, "Content-Length", resp->content_length);
    //构建并发送相应
    build_and_send_response(resp);
}
      
void build_and_send_response(http_response *resp)
{
    //构建发送的字符串
    string *buf = string_init();
    string_append(buf, "HTTP/1.0 ");
    string_append_int(buf, resp->status_code);
    string_append_ch(buf, ' ');
    string_append(buf, reason_phrase(resp->status_code));
    string_append(buf, "\r\n");

    for(size_t i=0; i < resp->headers->len; i++)
    {
        string_append_string(buf, resp->headers->ptr[i].key);
        string_append(buf, ": ");
        string_append_string(buf, resp->headers->ptr[i].value);
        string_append(buf, "\r\n");
    }

    string_append(buf, "\r\n");

    if(resp->content_length > 0)
        string_append_string(buf, resp->entity_body);

    send_all(resp, buf);
    string_free(buf);
}

const char* reason_phrase(int status_code)
{
    switch(status_code){
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
    }
    return "";
}

const char* get_mine_type(const char *path, const char *default_mine)
{
    size_t path_len = strlen(path);

    for(size_t i=0; i < sizeof(mine_types); i++)
    {
        size_t ext_len = strlen(mine_types[i].ext);
        const char *path_ext = path + path_len - ext_len;

        if(ext_len <= path_len && strcmp(path_ext, mine_types[i].ext) == 0)
            return mine_types[i].mine;
    }
    
    return default_mine;
}

int send_all(http_response *resp, string *buf)
{
    int bytes_sent = 0;
    int bytes_left = buf->len;
    int nbytes = 0;

    while(bytes_sent < bytes_left)
    {
        nbytes = send(resp->sockfd, buf->ptr + bytes_sent, bytes_left, 0);

        if(nbytes == -1)
            break;

        bytes_sent += nbytes;
        bytes_left -= nbytes;
    }

    return nbytes != -1 ? bytes_sent : -1;
}

char* match_until(char **buf, const char *delims)
{
    char *match = *buf;
    char *end_match = match + strcspn(*buf, delims);
    char *end_delims = end_match + strspn(end_match, delims);
    for(char *p = end_match; p < end_delims; p++)
    {
        *p = '\0';
    }
    *buf = end_delims;
    return (end_match != end_delims) ? match : NULL;
}

void extend(http_headers *h)
{
    if(h->len >= h->size)
    {
        h->size += HEADER_SIZE_INC;
        h->ptr = realloc(h->ptr, h->size * sizeof(keyvalue));
    }   
}

void http_headers_add(http_headers *h, const char *key, const char *value)
{
    assert(h != NULL);
    extend(h);

    h->ptr[h->len].key = string_init_str(key);
    h->ptr[h->len].value = string_init_str(value);
    h->len++;
}

void http_headers_add_int(http_headers *h, const char *key, int value)
{
    assert(h != NULL);
    extend(h);

    string *value_str = string_init();
    string_append_int(value_str, value);

    h->ptr[h->len].key = string_init_str(key);
    h->ptr[h->len].value = value_str;
    h->len++;
}

string* string_init()
{
    string *s;
    s = malloc(sizeof(*s));
    s->ptr = NULL;
    s->len = s->size = 0;
    return s;
}

int string_append(string *s, const char *str)
{
    return string_append_len(s, str, strlen(str));
}

void string_extend(string *s, size_t new_len)
{
    assert(s != NULL);

    if(new_len >= s->size)
    {
        s->size = new_len;
        s->ptr = realloc(s->ptr, s->size);
    }
}

int string_copy_len(string *s, const char *str, size_t str_len)
{
    assert(s != NULL);
    assert(str != NULL);

    if(str_len <= 0)
        return 0;

    string_extend(s, str_len+1);
    strncpy(s->ptr, str, str_len);
    s->len=str_len;
    s->ptr[s->len]='\0';

    return str_len;
}

int string_copy(string *s, const char *str)
{
    return string_copy_len(s, str, strlen(str));
}

string* string_init_str(const char *str)
{
    string *s=string_init();
    string_copy(s, str);
    return s;
}

int string_append_string(string *s, string *s2)
{
    assert(s != NULL);
    assert(s2 != NULL);

    return string_append_len(s, s2->ptr, s2->len);
}

int string_append_len(string *s, const char *str, size_t str_len)
{
    assert(s != NULL);
    assert(s != NULL);
    
    if(str_len <= 0)
        return 0;

    string_extend(s, s->len + str_len + 1);

    memcpy(s->ptr+s->len, str, str_len);

    s->len += str_len;
    s->ptr[s->len] = '\0';

    return str_len;

}

int string_append_ch(string *s, char ch)
{
    assert(s != NULL);

    string_extend(s, s->len+2);

    s->ptr[s->len++] = ch;
    s->ptr[s->len] = '\0';

    return 1;
}

int string_append_int(string *s, int i)
{
    assert(s != NULL);
    char buf[30];
    char digits[] = "0123456789";
    int len = 0;
    int minus = 0;

    if(i < 0)
    {
        minus = 1;
        i *= -1;
    }
    else if(i == 0)
    {
        string_append_ch(s, '0');
        return 1;
    }

    while(i)
    {
        buf[len++] = digits[i % 10];
        i = i / 10;
    }
    
    if(minus)
        buf[len++] = '-';
    
    for(int i = len - 1; i >= 0; i--)
        string_append_ch(s, buf[i]);

    return len;
}

void string_free(string *s)
{
    if(!s) return;
    free(s->ptr);
    free(s);
}
