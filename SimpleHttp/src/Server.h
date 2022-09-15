#pragma once

int initListenFd(unsigned short port);

int epollRun(int lfd);

int acceptClient(int lfd, int epfd);

int recvHttpReuqest(int cfd, int epfd);

int parseRequestLine(const char* line, int cfd);

int sendFile(const char* filename,int cfd);

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);

int sendDir(const char* dirname,int cfd);

const char* getFileType(const char* name);