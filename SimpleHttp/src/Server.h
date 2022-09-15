#pragma once

int initListenFd(unsigned short port);

int epollRun(int lfd);

void* acceptClient(void* arg);

void* recvHttpReuqest(void* arg);

int parseRequestLine(const char* line, int cfd);

int sendFile(const char* filename,int cfd);

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);

int sendDir(const char* dirname,int cfd);

const char* getFileType(const char* name);

void urldecode(char* url);
void urlencode(char* url);