#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "Server.h"

int main(int argc, char* argv[])
{    
    //check firewall allow 10000
    //system("lsof -i:10000");

    if(argc<3){
        printf("./a.out port path\n");
        return -1;
    }
    printf("begin search port\n");
    unsigned short port=atoi(argv[1]);
    printf("port=%d\n",port);

    char* path=NULL;
    path=getcwd(NULL,0);
    printf("now path=%s, change dir to %s\n",path,argv[2]);
    if(chdir(argv[2])<0){
        exit(1);
    }
    printf("chdir %s done\n",argv[2]);
    
    //initial socket
    printf("init socket\n");
    int lfd=initListenFd(port);    
    printf("lfd=%d\n",lfd);

    //start server
    epollRun(lfd);
    return 0;
}