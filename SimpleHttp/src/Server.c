#include "Server.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

struct FdInfo
{
    int tid,fd,epfd;
};


int initListenFd(unsigned short port)
{
    //1.creader listener
    int lfd=socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1)
    {
        perror("socket");
        return -1;
    }
    //2.set port reuse
    int opt=1;
    int ret=setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof opt);
    if(ret==-1)
    {
        perror("setsockopt");
        return -1;
    }
    //3.bind
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;
    ret=bind(lfd,(struct sockaddr *)&addr, sizeof addr);
    if(ret==-1)
    {
        perror("bind");
        return -1;
    }
    //4. set listen
    ret=listen(lfd,128);
    if(ret==-1)
    {
        perror("listen");
        return -1;
    }
    return lfd;
}


int epollRun(int lfd)
{
    printf("Run epoll\n");
    int epfd=epoll_create(1);
    if(epfd==-1)
    {
        perror("epoll create");
        return -1;
    }
    struct epoll_event ev;
    ev.data.fd=lfd;
    ev.events=EPOLLIN;
    int ret=epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret==-1)
    {
        perror("epoll ctl");
        return -1;
    }
    struct epoll_event evs[1024];
    int size=(sizeof(evs))/(sizeof(struct epoll_event));
    while(1)
    {
        printf("Run epoll ING\n");
        int num=epoll_wait(epfd,evs,size,-1);
        printf("epoll is Running,num=%d\n",num);
        for (int i = 0; i < num; i++)
        {
            int fd=evs[i].data.fd;

            struct FdInfo* info=(struct FdInfo*)malloc(sizeof(struct FdInfo));
            info->epfd=epfd;
            info->fd=fd;
            if(fd==lfd)
            {
                //construct connection
                
                //acceptClient(lfd,epfd);
                pthread_create(&info->tid,NULL,acceptClient,info);
            }else{
                //recive message
                
                //recvHttpReuqest(fd,epfd);
                pthread_create(&info->tid,NULL,recvHttpReuqest,info);
            }
        }
        
    }    
    return 0;
}

void* acceptClient(void* arg)
{
    struct FdInfo* info=(struct FdInfo*)arg;
    printf("accept connection...");
    //set accept
    int cfd=accept(info->fd,NULL,NULL);
    if(cfd==-1)
    {
        perror("accept");
        return -1;
    }
    //set unblock
    int flag=fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);
    //add cfd to epoll
    struct epoll_event ev;
    ev.data.fd=cfd;
    ev.events=EPOLLIN | EPOLLET; //EPOLLET:bianyan model
    int ret=epoll_ctl(info->epfd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret==-1)
    {
        perror("epoll ctl cfd");
        return -1;
    }
    free(info);
    return 0;
}

void* recvHttpReuqest(void* arg)
{
    struct FdInfo* info=(struct FdInfo*)arg;
    printf("beigin receive data ...");
    int len=0,total;
    char tmp[1024]={0};
    char buf[4096]={0};
    while((len=recv(info->fd,tmp,sizeof tmp,0))>0)
    {
        if(total+len<sizeof buf)
        {
            memcpy(buf+total,tmp,len);
        }
        total+=len;

    }
    //if data receive complish
    if(len==-1 && errno==EAGAIN)
    {
        //Analys Request Handle
        char* pt=strstr(buf,"\r\n");
        int reqLen=pt-buf;
        buf[reqLen]='\0';
        parseRequestLine(buf,info->fd);
    }
    else if(len==0)
    {
        //client close connection
        epoll_ctl(info->epfd,EPOLL_CTL_DEL,info->fd,NULL);
        close(info->fd);
    }
    else
    {
        perror("receive http request");            
    }
    free(info);
    return 0;
}

int parseRequestLine(const char* line, int cfd)
{
    //analysis
    char method[12];
    char path[1024];
    sscanf(line,"%[^ ] %[^ ]",method,path);
    if(strcasecmp(method,"get")!=0)
    {
        return -1;
    }

    urldecode(path);

    // handle static res
    char* file=NULL;
    if(strcmp(path,"/")==0){
        file="./";
    }else{
        file=path+1;
    }
    //get file attribute
    struct stat st;
    int ret=stat(file,&st);
    if(ret==-1){
        //file dont exist, reply 404
        sendHeadMsg(cfd,404,"Not Found",getFileType(".html"), -1);
        sendFile("404.html",cfd);
        return 0;
    }
    if(S_ISDIR(st.st_mode)){
        //send directory to client
        sendHeadMsg(cfd,200,"OK",getFileType(".html"), -1);
        sendDir(file, cfd);
    }else{
        //send file content to client
        sendHeadMsg(cfd,200,"OK",getFileType(file), st.st_size);
        sendFile(file,cfd);
    }
    
    return 0;
}

const char* getFileType(const char* name){
    //substr : from right to left
    const char* dot=strrchr(name,'.');
    if(dot==NULL){
        return "text/plain; charset=utf-8";
    }
    if(strcmp(dot,".html")==0 || strcmp(dot,".htm")==0){
        return "text/html; charset=utf-8";
    }
    if(strcmp(dot,".css")==0){
        return "text/css; charset=utf-8";
    }
    return "text/plain; charset=utf-8";
}

int sendFile(const char* filename,int cfd){
    int fd=open(filename,O_RDONLY);
    assert(fd>0);

#if 0
    while(1){
        char buf[1024];
        int len=read(fd, buf ,sizeof buf);
        if(len>0){
            send(cfd, buf, len, 0);
            usleep(10);
        }else if(len==0){
            break;
        }else{
            perror("read");
        }
    }
#else
    int size=lseek(fd, 0, SEEK_END);
    sendfile(cfd, fd, NULL, size);
#endif
    return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length)
{
    //status line
    char buf[4096]={0};
    sprintf(buf, "http/1.1 %d %s\r\n",status,descr);
    //response head
    sprintf(buf+strlen(buf),"content-type:%s\r\n",type);
    sprintf(buf+strlen(buf),"content-length:%d\r\n",length);
    sprintf(buf+strlen(buf),"\r\n");
    
    send(cfd, buf, strlen(buf), 0);
    return 0;
}

/*
<html>
    <head>
        <title>test</title>
    </head>
    <body>
        <table>
            <tr>
                <td><a href="%s/"></a></td>
                <td></td>
            </tr>
        </table>
    </body>
</html>
*/
int sendDir(const char* dirname,int cfd)
{
    char buf[4096]={0};
    sprintf(buf,"<html><head><title>%s</title></head><body><table>",dirname);
    struct dirent** namelist;
    int num=scandir(dirname, &namelist, NULL, alphasort);
    for (int i = 0; i < num; i++)
    {
        char* name=namelist[i]->d_name;
        struct stat st;
        char subPath[1024]={0};
        sprintf(subPath,"%s/%s",dirname, name);        
        stat(subPath, &st);
        if(S_ISDIR(st.st_mode)){
            sprintf(buf+strlen(buf),"<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",name,name,st.st_size);
        }else{
            sprintf(buf+strlen(buf),"<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",name,name,st.st_size);
        }
        send(cfd, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf,"</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    free(namelist);
    
}

static int hex2dec (char c) {
    if ('0' <= c && c <= '9') return c - '0';
    else if ('a' <= c && c <= 'f') return c - 'a' + 10;
    else if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return 0;
}
 
static char dec2hex (short int c) {
    if (0 <= c && c <= 9) return c + '0';
    else if (10 <= c && c <= 15) return c + 'A' - 10;
    return 0;
}
 //编码一个url
void urlencode(char* url)
{
    int i = 0;
    int len = strlen(url);
    int res_len = 0;
    unsigned char* res = (unsigned char*)malloc(3*len+1);        // 动态分配3倍的长度 
    for (i = 0; i < len; ++i) 
    {
        char c = url[i];
        if (    ('0' <= c && c <= '9') ||
                ('a' <= c && c <= 'z') ||
                ('A' <= c && c <= 'Z') || 
                c == '/' || c == '.') 
        {
            res[res_len++] = c;
        } 
        else 
        {
            int j = (short int)c;
            if (j < 0)
                j += 256;
            int i1, i0;
            i1 = j / 16;
            i0 = j - i1 * 16;
            res[res_len++] = '%';
            res[res_len++] = dec2hex(i1);
            res[res_len++] = dec2hex(i0);
        }
    }
    res[res_len] = '\0';
    strcpy(url, (const char*)res);
    free(res);
}

// 解码url 
void urldecode(char* url)
{
    int i = 0;
    unsigned int len = strlen(url);       // 长度 
    int res_len = 0;
    unsigned char* res = (unsigned char*)malloc(len+1);        // 动态分配同样的长度 
    for (i = 0; i < len; ++i) 
    {
        char c = url[i];
        if (c != '%') 
        {
            res[res_len++] = c;
        }
        else 
        {
            char c1 = url[++i];
            char c0 = url[++i];
            int num = 0;
            num = hex2dec(c1) * 16 + hex2dec(c0);
            res[res_len++] = num;
        }
    }
    res[res_len] = '\0';
    strcpy(url, (const char*)res);
    free(res);
}
