#pragma once
#define F_SETFL 1
#define O_NONBLOCK 2
int fcntl(int fd,int cmd,...);
