#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include "request.h"
#include "http_header.h"

// 初始化HTTP请求
http_request* http_request_init() {
    http_request *req;

    req = malloc(sizeof(*req));
    req->content_length = 0;
    req->version = HTTP_VERSION_UNKNOWN;
    req->content_length = -1;

    req->headers = http_headers_init();
    return req;
}

// 释放HTTP请求
void http_request_free(http_request *req) {
    if (!req) return;

    http_headers_free(req->headers);
    free(req);
}


// 一直读取字符直到遇到delims字符串，将先前读取的返回
// 可以用在解析HTTP请求时获取HTTP方法名（第一段字符串）
static char* match_until(char **buf, const char *delims) {
    char *match = *buf;
    char *end_match = match + strcspn(*buf, delims);//strcspn(const char *str1, const char *str2)检索字符串str1开头连续有几个字符都不含字符串str2中的字符
    char *end_delims = end_match + strspn(end_match, delims);//检索字符串end_match中第一个不在字符串delims中出现的字符下标

    for (char *p = end_match; p < end_delims; p++) {
        *p = '\0';
    }

    *buf = end_delims;

    return (end_match != end_delims) ? match : NULL;
}

// 根据字符串获得请求中的HTTP方法，目前只支持GET和HEAD
static http_method get_method(const char *method) {
    if (strcasecmp(method, "GET") == 0)//strcasecmp()用来比较参数s1和s2字符串，比较时会自动忽略大小写的差异
        return HTTP_METHOD_GET;
    else if (strcasecmp(method, "HEAD") == 0)//若参数s1和s2字符串相同则返回0。s1长度大于s2长度则返回大于0 的值，s1 长度若小于s2 长度则返回小于0的值.
        return HTTP_METHOD_HEAD;
    else if(strcasecmp(method, "POST") == 0 || strcasecmp(method, "PUT") == 0)
        return HTTP_METHOD_NOT_SUPPORTED;

    return HTTP_METHOD_UNKNOWN;
}

// 解析URI，获得文件在服务器上的绝对路径
static int resolve_uri(char *resolved_path, char *root, char *uri) {
    int ret = 0;
    string *path = string_init_str(root);
    string_append(path, uri);

    char *res = realpath(path->ptr, resolved_path);//realpath是用来将参数path所指的相对路径转换成绝对路径，然后存于参数resolved_path所指的字符串数组或指针中的一个函数。
//如果resolved_path为NULL，则该函数调用malloc分配一块大小为PATH_MAX的内存来存放解析出来的绝对路径，并返回指向这块区域的指针。程序员应调用free来手动释放这块内存。
    if (!res) {
        ret = -1;
        goto cleanup;
    }

    size_t resolved_path_len = strlen(resolved_path);
    size_t root_len = strlen(root);

    if (resolved_path_len < root_len) {
        ret = -1;
    } else if (strncmp(resolved_path, root, root_len) != 0) { //strncmp比较两个字符串前root_len位
        ret = -1;
    } else if(uri[0] == '/' && uri[1] == '\0') {
        strcat(resolved_path, "/index.html");//将两个char类型连接
    }

cleanup:
    string_free(path);
    return ret;
}

/*
 * 设置状态码
 */
static void try_set_status(connection *con, int status_code) {
    if (con->status_code == 0)
        con->status_code = status_code;
}

// 解析HTTP请求消息
void http_request_parse(server *serv, connection *con) {
    http_request *req = con->request;//定义一个指针req，指向客户端的HTTP请求内容
    char *buf = con->recv_buf->ptr;

    req->method_raw = match_until(&buf, " ");//match_until函数在上文中

    if (!req->method_raw) { //如果method_raw为0，将状态码设置为400
        con->status_code = 400;
        return;
    }

    // 获得HTTP方法
    req->method = get_method(req->method_raw);//get_method函数在上文

    if (req->method == HTTP_METHOD_NOT_SUPPORTED) {
        try_set_status(con, 501);
    } else if(req->method == HTTP_METHOD_UNKNOWN) {
        con->status_code = 400;
        return;
    }

    // 获得URI
    req->uri = match_until(&buf, " \r\n");

    if (!req->uri) {
        con->status_code = 400;
        return;
    }

    /*
     * 判断访问的资源是否在服务器上
     *
     */
     // resolve_uri()解析URI，获得文件在服务器上的绝对路径
    if (resolve_uri(con->real_path, serv->conf->doc_root, req->uri) == -1) {
        try_set_status(con, 404);
    }

    // 如果版本为HTTP_VERSION_09立刻退出
    if (req->version == HTTP_VERSION_09) {
        try_set_status(con, 200);
        req->version_raw = "";
        return;
    }

    // 获得HTTP版本
    req->version_raw = match_until(&buf, "\r\n");

    if (!req->version_raw) {
        con->status_code = 400;
        return;
    }

    // 支持HTTP/1.0或HTTP/1.1
    //strcasecmp()用于忽略大小写比较字符串
    if (strcasecmp(req->version_raw, "HTTP/1.0") == 0) {
        req->version = HTTP_VERSION_10;
    } else if (strcasecmp(req->version_raw, "HTTP/1.1") == 0) {
        req->version = HTTP_VERSION_11;
    } else {
        try_set_status(con, 400);
    }

    if (con->status_code > 0)
        return;

    // 解析HTTP请求头部

    char *p = buf;
    char *endp = con->recv_buf->ptr + con->request_len;

    while (p < endp) {
        const char *key = match_until(&p, ": ");
        const char *value = match_until(&p, "\r\n");

        if (!key || !value) {
            con->status_code = 400;
            return;
        }

        http_headers_add(req->headers, key, value);
    }

    con->status_code = 200;
}

/*
 * 根据HTTP协议验证HTTP请求是否合法
 *
 * 返回值
 * -1 不合法
 * 0 不完整
 * 1 完整
 */
int http_request_complete(connection *con) {
    char c;
    for (; con->request_len < con->recv_buf->len; con->request_len++) {
        c = con->recv_buf->ptr[con->request_len];

        switch (con->recv_state) {
            case HTTP_RECV_STATE_WORD1:
                if (c == ' ')
                    con->recv_state = HTTP_RECV_STATE_SP1;
                else if (!isalpha(c))//字符c是否为英文字母，若为英文字母，返回非0（小写字母为2，大写字母为1）。若不是字母，返回0
                    return -1;
            break;

            case HTTP_RECV_STATE_SP1:
                if (c == ' ')
                    continue;
                if (c == '\r' || c == '\n' || c == '\t')
                    return -1;
                con->recv_state = HTTP_RECV_STATE_WORD2;
            break;

            case HTTP_RECV_STATE_WORD2:
                if (c == '\n') {
                    con->request_len++;
                    con->request->version = HTTP_VERSION_09;
                    return 1;
                } else if (c == ' ')
                    con->recv_state = HTTP_RECV_STATE_SP2;
                else if (c == '\t')
                    return -1;
            break;

            case HTTP_RECV_STATE_SP2:
                if (c == ' ')
                    continue;
                if (c == '\r' || c == '\n' || c == '\t')
                    return -1;
                con->recv_state = HTTP_RECV_STATE_WORD3;
            break;

            case HTTP_RECV_STATE_WORD3:
                if (c == '\n')
                    con->recv_state = HTTP_RECV_STATE_LF;
                else if (c == ' ' || c == '\t')
                    return -1;
            break;

            case HTTP_RECV_STATE_LF:
                if (c == '\n') {
                    con->request_len++;
                    return 1;
                } else if (c != '\r')
                    con->recv_state = HTTP_RECV_STATE_LINE;
            break;

            case HTTP_RECV_STATE_LINE:
                if (c == '\n')
                    con->recv_state = HTTP_RECV_STATE_LF;
            break;
        }
    }

    return 0;
}
