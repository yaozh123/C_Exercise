#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "stringutils.h"

#define STRING_SIZE_INC 64

//对字符串进行扩展，将字符串大小扩展到new_len的长度
void string_extend(string *s, size_t new_len) {
    assert(s != NULL);

    if (new_len >= s->size) {
        s->size += new_len - s->size;
        s->size += STRING_SIZE_INC - (s->size % STRING_SIZE_INC);
        s->ptr = realloc(s->ptr, s->size);
    }
}

//对string类型进行初始化，为string在堆区中分配空间
string* string_init() {
    string *s;
    s = malloc(sizeof(*s));
    s->ptr = NULL;
    s->size = s->len = 0;

    return s;
}

//将str指向的空间的字符串，复制到string中
string* string_init_str(const char *str) {
    string *s = string_init();
    string_copy(s, str);

    return s;
}

//释放string空间
void string_free(string *s) {
    if (!s) return;

    free(s->ptr);
    free(s);
}

//字符串重置，不论原来字符串是什么，一律变为空
void string_reset(string *s) {
    assert(s != NULL);

    if (s->size > 0) {
        s->ptr[0] = '\0';
    }
    s->len = 0;
}

//将str指向的字符串复制到s指向的字符串空间中，指定复制的长度为str_len
int string_copy_len(string *s, const char *str, size_t str_len) {
    assert(s != NULL);//允许诊断信息被写入到标准错误文件中。换句话说，它可用于在 C 程序中添加诊断。
    assert(str != NULL);//如果 expression 为 TRUE，assert() 不执行任何动作。如果 expression 为 FALSE，assert() 会在标准错误 stderr 上显示错误消息，并中止程序执行。

    if (str_len <= 0) return 0;

    string_extend(s, str_len + 1);
    strncpy(s->ptr, str, str_len);//把str所指向的字符串复制到s-ptr,最多复制str_len个字符.当str的长度小于str_len时，s-ptr的剩余部分将用空字节填充。
    s->len = str_len;
    s->ptr[s->len] = '\0';

    return str_len;
}

//复制，不指定长度，将str复制到s中
int string_copy(string *s, const char *str) {
    return string_copy_len(s, str, strlen(str));
}

// 添加字符串s2到s末尾
int string_append_string(string *s, string *s2) {
    assert(s != NULL);
    assert(s2 != NULL);

    return string_append_len(s, s2->ptr, s2->len);
}

// 添加数字i到字符串末尾
int string_append_int(string *s, int i) {
    assert(s != NULL);
    char buf[30];
    char digits[] = "0123456789";
    int len = 0;
    int minus = 0;

    if (i < 0) {
        minus = 1;
        i *= -1;
    } else if (i == 0) {
        string_append_ch(s, '0');
        return 1;
    }

    while (i) {
        buf[len++] = digits[i % 10];
        i = i / 10;
    }

    if (minus)
        buf[len++] = '-';

    for (int i = len - 1; i >= 0; i--) {
        string_append_ch(s, buf[i]);
    }

    return len;

}

//添加str到字符串s末尾，添加的长度为str_len
int string_append_len(string *s, const char *str, size_t str_len) {
    assert(s != NULL);
    assert(str != NULL);

    if (str_len <= 0) return 0;

    string_extend(s, s->len + str_len + 1);

    memcpy(s->ptr + s->len, str, str_len);
    s->len += str_len;
    s->ptr[s->len] = '\0';

    return str_len;
}

//添加str到字符串s末尾
int string_append(string *s, const char *str) {
    return string_append_len(s, str, strlen(str));
}

// 添加字符ch到字符串s末尾
int string_append_ch(string *s, char ch) {
    assert(s != NULL);

    string_extend(s, s->len + 2);

    s->ptr[s->len++] = ch;
    s->ptr[s->len] = '\0';

    return 1;
}

