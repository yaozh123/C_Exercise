#include <limits.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "http_header.h"
#include "response.h"

// 文件扩展名与MimeType数据结构
//MIME (Multipurpose Internet Mail Extensions) 是描述消息内容类型的因特网标准
typedef struct {
    // 文件扩展名
    const char *ext;
    // Mime类型名
    const char *mime;
} mime;

// 目前支持的MimeType
static mime mime_types[] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".jpg", "image/jpg"},
    {".png", "image/png"}
};

// 出错页面
static char err_file[PATH_MAX];
static const char *default_err_msg = "<HTML><HEAD><TITLE>Error</TITLE></HEAD>"
                                      "<BODY><H1>Something went wrong</H1>"
                                      "</BODY></HTML>";

// 初始化HTTP响应
http_response* http_response_init() {
    http_response *resp;
    resp = malloc(sizeof(*resp));
    memset(resp, 0, sizeof(*resp));

    resp->headers = http_headers_init();
    resp->entity_body = string_init();
    resp->content_length = -1;

    return resp;
}

// 释放HTTP响应
void http_response_free(http_response *resp) {
    if (!resp) return;

    http_headers_free(resp->headers);
    string_free(resp->entity_body);

    free(resp);
}

// 根据状态码构建响应结构中的状态消息
static const char* reason_phrase(int status_code) {
    switch (status_code) {
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

/*
 * 将响应消息发送给客户端
 */
static int send_all(connection *con, string *buf) {
    int bytes_sent = 0;
    int bytes_left = buf->len;
    int nbytes = 0;

    while (bytes_sent < bytes_left) {
        nbytes = send(con->sockfd, buf->ptr + bytes_sent, bytes_left, 0);

        if (nbytes == -1)
            break;

        bytes_sent += nbytes;
        bytes_left -= nbytes;

    }

    return nbytes != -1 ? bytes_sent : -1;
}

// 根据文件路径获得MimeType
static const char* get_mime_type(const char *path, const char *default_mime) {
    size_t path_len = strlen(path);

    for (size_t i = 0; i < sizeof(mime_types); i++) {
        size_t ext_len = strlen(mime_types[i].ext);
        const char *path_ext = path + path_len - ext_len;

        if (ext_len <= path_len && strcmp(path_ext, mime_types[i].ext) == 0)
            return mime_types[i].mime;

    }

    return default_mime;
}

// 检查文件权限是否可以访问，可以访问返回0，失败返回-1
static int check_file_attrs(connection *con, const char *path) {
    struct stat s;//

    con->response->content_length = -1;

    //int stat(const char *path, struct stat *buf),成功返回0，失败返回-1
    //通过文件路径path获取文件信息，并保存在buf所指的结构体stat中
    if (stat(path, &s) == -1) {
        con->status_code = 404;
        return -1;
    }

    //S_ISREG判断是否是一个常规文件.
    if (!S_ISREG(s.st_mode)) {
        con->status_code = 403;
        return -1;
    }

    con->response->content_length = s.st_size;//普通文件，对应的文件字节数

    return 0;
}

// 读取文件
static int read_file(string *buf, const char *path) {
    FILE *fp;
    int fsize;

    fp = fopen(path, "r");

    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);//fp将指向以SEEK_END为基准，偏移0个字节的位置
    fsize = ftell(fp); //得到文件位置指针当前位置相对于文件首的偏移字节数
    fseek(fp, 0, SEEK_SET);

    string_extend(buf, fsize + 1);// 扩展buf长度到fsize + 1
    //从给定流fp读取数据，最多读取1个项，每个项fsize个字节，如果调用成功返回实际读取到的项个数（小于或等于1），如果不成功或读到文件末尾返回 0
    if (fread(buf->ptr, fsize, 1, fp) > 0) {
        buf->len += fsize;
        buf->ptr[buf->len] = '\0';
    }

    fclose(fp);//关闭文件

    return fsize;
}

// 读取标准错误页面
static int read_err_file(server *serv, connection *con, string *buf) {
    //将可变参数 “…” 按照format的格式格式化为字符串，然后再将其拷贝至err_file中
    snprintf(err_file, sizeof(err_file), "%s/%d.html", serv->conf->doc_root, con->status_code);

    int len = read_file(buf, err_file);

    //E 如果文件不存在则使用默认的出错信息字符串替代
    if (len <= 0) {
        string_append(buf, default_err_msg);
        len = buf->len;
    }

    return len;
}

// 构建并发送响应
static void build_and_send_response(connection *con) {

    // 构建发送的字符串
    string *buf = string_init();
    http_response *resp = con->response;

    string_append(buf, "HTTP/1.0 ");
    string_append_int(buf, con->status_code);
    string_append_ch(buf, ' ');
    string_append(buf, reason_phrase(con->status_code));// 根据状态码构建响应结构中的状态消息
    string_append(buf, "\r\n");

    for (size_t i = 0; i < resp->headers->len; i++) {
        string_append_string(buf, resp->headers->ptr[i].key);
        string_append(buf, ": ");
        string_append_string(buf, resp->headers->ptr[i].value);
        string_append(buf, "\r\n");
    }

    string_append(buf, "\r\n");

    if (resp->content_length > 0 && con->request->method != HTTP_METHOD_HEAD) {
        string_append_string(buf, resp->entity_body);
    }

    // 将字符串缓存发送到客户端
    send_all(con, buf);//将响应消息发送给客户端
    string_free(buf);
}

/*
 * 当出错时发送标准错误页面，页面名称类似404.html
 * 如果错误页面不存在则发送标准的错误消息
 */
static void send_err_response(server *serv, connection *con) {
    http_response *resp = con->response;
    snprintf(err_file, sizeof(err_file), "%s/%d.html", serv->conf->doc_root, con->status_code);

    // 检查错误页面
    if (check_file_attrs(con, err_file) == -1) {
        resp->content_length = strlen(default_err_msg);
        log_error(serv, "failed to open file %s", err_file);
    }

    // 构建消息头部
    http_headers_add(resp->headers, "Content-Type", "text/html");
    http_headers_add_int(resp->headers, "Content-Length", resp->content_length);

    if (con->request->method != HTTP_METHOD_HEAD) {
       read_err_file(serv, con, resp->entity_body);
    }

    build_and_send_response(con);
}

/*
 * 构建响应消息和消息头部并发送消息
 * 如果请求的资源无法打开则发送错误消息
 */
static void send_response(server *serv, connection *con) {
    http_response *resp = con->response;
    http_request *req = con->request;

    http_headers_add(resp->headers, "Server", "cserver");

    if (con->status_code != 200) {
        send_err_response(serv, con);
        return;
    }

    if (check_file_attrs(con, con->real_path) == -1) {  // 如果文件权限是不可以访问
        send_err_response(serv, con);
        return;
    }

    if (req->method != HTTP_METHOD_HEAD) {
        read_file(resp->entity_body, con->real_path);//读取文件
    }

    // 构建消息头部
    const char *mime = get_mime_type(con->real_path, "text/plain");// 根据文件路径获得MimeType
    http_headers_add(resp->headers, "Content-Type", mime);
    http_headers_add_int(resp->headers, "Content-Length", resp->content_length);

    build_and_send_response(con);// 构建并发送响应
}

/*
 * HTTP_VERSION_09时，只发送响应消息内容，不包含头部
 */
static void send_http09_response(server *serv, connection *con) {
    http_response *resp = con->response;

    if (con->status_code == 200 && check_file_attrs(con, con->real_path) == 0) {
        read_file(resp->entity_body, con->real_path);
    } else {
        read_err_file(serv, con, resp->entity_body);
    }

    send_all(con, resp->entity_body);
}

// 发送HTTP响应
void http_response_send(server *serv, connection *con) {
    if (con->request->version == HTTP_VERSION_09) {
        send_http09_response(serv, con);
    } else {
        send_response(serv, con);
    }
}
