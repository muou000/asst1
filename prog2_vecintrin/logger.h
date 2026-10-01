#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdio.h>
#include <vector>
#include <string.h>
using namespace std;

#define MAX_INST_LEN 32

struct __cs149_mask;

struct Log {
  char instruction[MAX_INST_LEN];
  unsigned long long mask; // 支持最大 64 的 vector 宽度
};

struct Statistics {
  unsigned long long utilized_lane;
  unsigned long long total_lane;
  unsigned long long total_instructions;
};

class Logger {
  private:
    vector<Log> log;
    Statistics stats;

  public:
    void addLog(const char * instruction, __cs149_mask mask, int N = 0);
    void printStats();
    void printLog();
};

#endif
