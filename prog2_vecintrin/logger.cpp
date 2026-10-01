#include "logger.h"
#include "CS149intrin.h"

void Logger::addLog(const char * instruction, __cs149_mask mask, int N) {
  Log newLog;
  strcpy(newLog.instruction, instruction);
  newLog.mask = 0;
  for (int i=0; i<N; i++) {
    if (mask.value[i]) {
      newLog.mask |= (((unsigned long long)1)<<i);
      stats.utilized_lane++;
    }
  }
  stats.total_lane += N;
  stats.total_instructions += (N>0);
  log.push_back(newLog);
}

void Logger::printStats() {
  printf("****************** 输出 Vector 单元统计信息 *******************\n");
  printf("Vector 宽度:               %d\n", VECTOR_WIDTH);
  printf("Vector 指令总数:           %lld\n", stats.total_instructions);
  printf("Vector 利用率:             %.1f%%\n", (double)stats.utilized_lane/stats.total_lane*100);
  printf("已用 Vector lane 数:       %lld\n", stats.utilized_lane);
  printf("Vector lane 总数:          %lld\n", stats.total_lane);
}



void Logger::printLog() {
  printf("***************** 输出 Vector 单元执行日志 *****************\n");
  printf(" 指令        | Vector lane 占用情况 ('*' 表示激活, '_' 表示未激活)\n");
  printf("------------- --------------------------------------------------------\n");
  for (int i=0; i<log.size(); i++) {
    printf("%12s | ", log[i].instruction);
    for (int j=0; j<VECTOR_WIDTH; j++) {
      if (log[i].mask & (((unsigned long long)1)<<j)) {
        printf("*");
      } else {
        printf("_");
      }
    }
    printf("\n");
  }
}

