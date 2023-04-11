#ifndef SOV_HPP
#define SOV_HPP
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <semaphore.h>
#include <queue>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <memory.h>
#include <map>
#include <vector>
const int NUM = 9;
enum { ROW=9, COL=9, N = 81, NEIGHBOR = 20 };
void solver(char  * problem,int board[81]);
#endif
