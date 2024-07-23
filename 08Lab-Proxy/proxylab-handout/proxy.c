#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CAHCE_NUM 10
#define MAX_READ_NUM 10
sem_t mutex; // 互斥锁，用于防止竞争
sem_t w; // 写锁
sem_t r; // 读锁
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *eof_hdr = "\r\n";
// MAX_CACHE_SIZE + T * MAX_OBJECT_SIZE
typedef struct  
{
    char url[MAXLINE];  // 一个URL对应一个数据
    char data[MAX_OBJECT_SIZE];   //缓存的数据
    unsigned int length; //数据大小
    unsigned int count;   // 被使用过的次数， 读写都算, 用于LRU
    /* data */
} CacheObject;
// 缓存
typedef struct {
    CacheObject list[MAX_CAHCE_NUM];
    unsigned int length;  // 使用了多少个缓存
} ProxyCache;


typedef struct {
    char protocol[255];
    char port[6];
    char hostname[255];
    char content[MAXLINE];
} end_server_t;

ProxyCache proxy_cache;
void initCache(ProxyCache *cache_ptr);
void write_cache(ProxyCache *ptr_cache, CacheObject *ptr_data);
CacheObject* read_cache(ProxyCache *ptr_cache, char* url);
int parse_uri(char *url, end_server_t *ptr);
void trans(int connect_fd);
// 转发请求头
void transRequestLine(rio_t* conn_rio_ptr, int  server_fd, char *host_hdr);
int parse_key(char *buf, char *key);
void* thread(void *vargp);

void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}




/**
 * 您的代理应在命令行中指定的端口上监听传入连接
 * 一切皆文件
 * */


int main(int argc, char **argv)
{
    // 规范命令行参数
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // 初始化缓存
    initCache(&proxy_cache);
    // 客户端发起的链接文件描述符， 代理服务器的链接文件描述符
    int proxy_fd;
    char hostname[MAXLINE], port[MAXLINE]; 
    struct sockaddr_storage client_addr; // 兼容socket地址
    socklen_t client_len = sizeof(client_addr);
    proxy_fd = Open_listenfd(argv[1]);  // 代理服务器的文件描述符
    while (1) {
        // 阻塞，等待连接，转换socket地址为通用结构
        int* connect_fd = (int *)Malloc(sizeof(int));
        *connect_fd = Accept(proxy_fd, (SA *)&client_addr, &client_len);
        // 打印对应的连接客户端信息
        Getnameinfo((SA *) &client_addr, client_len, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        pthread_t tid;
        Pthread_create(&tid, NULL, thread, (void *)connect_fd);
    }
}

void* thread(void *vargp) {
    // 分离主进程。自动回收
    Pthread_detach(pthread_self());
    int connfd = *(int *)vargp;
    Free(vargp);
    trans(connfd);
    Close(connfd);
    return NULL;
}
// curl -o 1.html -v --proxy http://localhost:8070 http://localhost:8090/home.html
//  curl -o 2.html  http://localhost:8090/home.html 
// 解析URL
// example: https://www.yoojia.com:80/comment/s-1684
int parse_uri(char *url, end_server_t *ptr) {

    // 协议
    if (strncasecmp(url, "http://", strlen("http://")) != 0) {
        fprintf(stderr, "Not http protocol: %s\n", url);
        return -1;
    }
 
    // host www.yoojia.com:8080/comment/s-1684
    char *host_start = url + strlen("http://");
    char *host_end = strchr(host_start, '/');
    char *port = strchr(host_start, ':');
    if (host_end == NULL) {
        fprintf(stderr, "HOST ERROR: %s\n", url);
        return -1;
    }
    if (port == NULL) {
        strcpy(ptr->port, "80");
    } else {
        strncpy(ptr->port, port + 1, host_end - port - 1);
    }
    strncpy(ptr->hostname, host_start, port - host_start);
    strcpy( ptr->content, host_end);
    return 0;
}

/**
 * 转发请求帮助函数 
 * client -> proxy -> server
 * client <- proxy <- server
 */

void trans(int connect_fd) {
    // HTTP请求的信息, 方法： get， url, http/1.1 http/1.0
    char method[MAXLINE], url[MAXLINE], version[MAXLINE], buf[MAXLINE];
    rio_t connect_rio, server_rio;
    Rio_readinitb(&connect_rio, connect_fd);
    // Rio_readinitb(&server_rio, server_fd);

    // 此时等待客户端发送请求，挂起
    if (Rio_readlineb(&connect_rio, buf, MAXLINE) <= 0) {
        return; // 若为空行或者连接中断则退出
    }
    fputs(buf, stdout);
    sscanf(buf, "%s %s %s", method, url, version);

    CacheObject* c_ptr = read_cache(&proxy_cache, url);

    if (c_ptr != NULL) {
        Rio_writen(connect_fd, c_ptr->data, c_ptr->length);
        return;
    }

    end_server_t e_s;
    if (parse_uri(url, &e_s) == -1) {
        return;
    }
    // printf("hostname: %s, port: %s\n", e_s.hostname, e_s.port);
    // 建立end server的连接
    int end_server_fd = Open_clientfd(e_s.hostname, e_s.port);
    Rio_readinitb(&server_rio, end_server_fd);
    // 请求头设置  
    char request_header[MAXLINE];    
    strcpy(request_header, method);
    strcat(request_header, " ");
    strcat(request_header, e_s.content);
    strcat(request_header, " HTTP/1.0\r\n");
    // 发送  method url protocol
    Rio_writen(end_server_fd, request_header, strlen(request_header));
    printf("request_header: %s\n", request_header);
    char host_hdr[MAXLINE];    
    strcpy(host_hdr, "Host: ");
    strcat(host_hdr, e_s.hostname);
    strcat(host_hdr, ":");
    strcat(host_hdr, e_s.port);
    strcat(host_hdr, "\r\n");
    // 转发请求头
    transRequestLine(&connect_rio, end_server_fd, host_hdr);


    int contentLength;

    // 缓存对象
    CacheObject co;

    int flag = 0;
    // 获取响应
    while (Rio_readlineb(&server_rio, buf, MAXLINE)) {
        char *ptr = strstr(buf, "Content-length: ");
        if (ptr != NULL) {
            contentLength = atoi(ptr + 15);
        }
        if (flag == 0) {
            if (strlen(buf) + strlen(co.data) > MAX_OBJECT_SIZE) {
                flag = -1;
            } else {
                memcpy(co.data + strlen(co.data), buf, strlen(buf));
            }
        }
        Rio_writen(connect_fd, buf, strlen(buf));
        if (strcmp(buf, "\r\n") == 0 ) {
            break;
        }
    }
    char fileBuf[MAX_CACHE_SIZE];
    Rio_readnb(&server_rio, fileBuf, contentLength);
    if (flag == 0 && strlen(co.data) + strlen(fileBuf) < MAX_OBJECT_SIZE) {
        memcpy(co.data + strlen(co.data), fileBuf, strlen(fileBuf));
        co.count = 1;
        co.length = strlen(co.data);
        printf("fileBuf: %s, co.data: %s\n", fileBuf, co.data);
        strcpy(co.url, url);
        write_cache(&proxy_cache, &co);
    }
    Rio_writen(connect_fd, fileBuf, contentLength);
    Close(end_server_fd);
}

// 转发请求头
void transRequestLine(rio_t* conn_rio_ptr, int  server_fd, char *host_hdr) {
    // 始终发送的请求头， User-Agent、Connection、Proxy-Connection, Host
    char* connect_hdr = "Connection: close\r\n";
    char* proxy_conn_hdr = "Proxy-Connection: close\r\n";
    char buf[MAXLINE];
    Rio_readlineb(conn_rio_ptr, buf, MAXLINE);
    fputs(buf, stdout);
    while(strcmp(buf, eof_hdr)) {
        int flag = -1;
        if (parse_key(buf, "User-Agent") == 0) {
            flag +=1;
        };
        if (parse_key(buf, "Connection") == 0) {
            flag +=1;
        }
        if (parse_key(buf, "Proxy-Connection") == 0) {
            flag +=1;
        };
        if (parse_key(buf, "Host") == 0) {
            flag +=1;
        }
        if (flag == -1) {
            Rio_writen(server_fd, buf, strlen(buf));
        }
        Rio_readlineb(conn_rio_ptr, buf, MAXLINE);
        fputs(buf, stdout);
    }
    if (strcmp(buf, eof_hdr) == 0) {
        Rio_writen(server_fd, host_hdr, strlen(host_hdr));
        Rio_writen(server_fd, user_agent_hdr, strlen(user_agent_hdr));
        Rio_writen(server_fd, connect_hdr, strlen(connect_hdr));
        Rio_writen(server_fd, proxy_conn_hdr, strlen(proxy_conn_hdr));
        Rio_writen(server_fd, eof_hdr, strlen(eof_hdr));
    } else {
        exit(0);
    }
}

// 解析请求行的KEY
// key: value

int parse_key(char *buf, char *key) {
    char *k = strtok(buf, ":");
    return strcasecmp(k, key);
}

// 初始化缓存
void initCache(ProxyCache *cache_ptr) {
    cache_ptr->length = 0;
    Sem_init(&w, 0, 1);
    Sem_init(&mutex, 0, 1);
    Sem_init(&r, 0, MAX_READ_NUM);
}
/**
 * 多个读取者
 * 一个写入者
 * 
 * 对缓存的访问必须是线程安全的，
 * 确保缓存访问不存在竞争条件可能是这一部分实验中更有趣的方面。
 * 事实上，有一个特别要求是多个线程必须能够同时从缓存中读取。
 */
// 写入缓存 
void write_cache(ProxyCache *ptr_cache, CacheObject *ptr_data) {
    P(&w);  // 获取写互斥锁，如果正在写入，则等待。

    // 等待所有读者读取完毕
    for (size_t i = 0; i < MAX_READ_NUM; i++)
    {
       P(&r);
    }
    
    // 未写满缓存
    if (ptr_cache->length != MAX_CAHCE_NUM) {
       memcpy(&ptr_cache->list[(ptr_cache->length)++], ptr_data, sizeof(CacheObject));
    } else {
        // 找出最小使用数
        int used_c = ptr_cache->list[0].count;
        for (size_t i = 1; i < ptr_cache->length; i++)
        {   
            if (used_c > ptr_cache->list[i].count) {
                used_c = ptr_cache->list[i].count;
            }
        }
        for (size_t i = 0; i < ptr_cache->length; i++)
        {   
            if (used_c == ptr_cache->list[i].count) {
               memcpy(&ptr_cache->list[i], ptr_data, sizeof(CacheObject));
               break;
            }
        }
        // 将最小使用的数据给替换掉
    }
    // 返回互斥锁
    for (size_t i = 0; i < MAX_READ_NUM; i++)
    {
       V(&r);
    }
    V(&w); // 返回互斥锁
}


CacheObject* read_cache(ProxyCache *ptr_cache, char* url) {
    P(&r);
    for (size_t i = 0; i < ptr_cache->length; i++)
    {   
        if (strncmp(ptr_cache->list[i].url, url, strlen(url)) == 0) {
            P(&mutex);  // 防止多个读取相同内容时候出现竞争问题
            ptr_cache->list[i].count+=1;
            V(&mutex);
            return &ptr_cache->list[i];
        }
    }
    V(&r);
    return NULL;
}
