#include <stdio.h>
#include <algorithm>
#include <getopt.h>
#include <math.h>
#include "CS149intrin.h"
#include "logger.h"
using namespace std;

#define EXP_MAX 10

Logger CS149Logger;

void usage(const char* progname);
void initValue(float* values, int* exponents, float* output, float* gold, unsigned int N);
void absSerial(float* values, float* output, int N);
void absVector(float* values, float* output, int N);
void clampedExpSerial(float* values, int* exponents, float* output, int N);
void clampedExpVector(float* values, int* exponents, float* output, int N);
float arraySumSerial(float* values, int N);
float arraySumVector(float* values, int N);
bool verifyResult(float* values, int* exponents, float* output, float* gold, int N);

int main(int argc, char * argv[]) {
  int N = 16;
  bool printLog = false;

  // 解析命令行选项 ////////////////////////////////////////////
  int opt;
  static struct option long_options[] = {
    {"size", 1, 0, 's'},
    {"log", 0, 0, 'l'},
    {"help", 0, 0, '?'},
    {0 ,0, 0, 0}
  };

  while ((opt = getopt_long(argc, argv, "s:l?", long_options, NULL)) != EOF) {

    switch (opt) {
      case 's':
        N = atoi(optarg);
        if (N <= 0) {
          printf("错误: 工作负载大小被设置为 %d (<0)。\n", N);
          return -1;
        }
        break;
      case 'l':
        printLog = true;
        break;
      case '?':
      default:
        usage(argv[0]);
        return 1;
    }
  }


  float* values = new float[N+VECTOR_WIDTH];
  int* exponents = new int[N+VECTOR_WIDTH];
  float* output = new float[N+VECTOR_WIDTH];
  float* gold = new float[N+VECTOR_WIDTH];
  initValue(values, exponents, output, gold, N);

  clampedExpSerial(values, exponents, gold, N);
  clampedExpVector(values, exponents, output, N);

  //absSerial(values, gold, N);
  //absVector(values, output, N);

  printf("\e[1;31mCLAMPED EXPONENT（截断指数）\e[0m (必做) \n");
  bool clampedCorrect = verifyResult(values, exponents, output, gold, N);
  if (printLog) CS149Logger.printLog();
  CS149Logger.printStats();

  printf("************************ 结果验证 *************************\n");
  if (!clampedCorrect) {
    printf("@@@ 测试失败!!!\n");
  } else {
    printf("测试通过!!!\n");
  }

  printf("\n\e[1;31mARRAY SUM（数组求和）\e[0m (加分题) \n");
  if (N % VECTOR_WIDTH == 0) {
    float sumGold = arraySumSerial(values, N);
    float sumOutput = arraySumVector(values, N);
    float epsilon = 0.1;
    bool sumCorrect = abs(sumGold - sumOutput) < epsilon * 2;
    if (!sumCorrect) {
      printf("期望值 %f, 实际得到 %f\n.", sumGold, sumOutput);
      printf("@@@ 测试失败!!!\n");
    } else {
      printf("测试通过!!!\n");
    }
  } else {
    printf("此题要求 N %% VECTOR_WIDTH == 0 (VECTOR_WIDTH 为 %d)\n", VECTOR_WIDTH);
  }

  delete [] values;
  delete [] exponents;
  delete [] output;
  delete [] gold;

  return 0;
}

void usage(const char* progname) {
  printf("用法: %s [选项]\n", progname);
  printf("程序选项:\n");
  printf("  -s  --size <N>     使用大小为 N 的工作负载 (默认 = 16)\n");
  printf("  -l  --log          输出 vector 单元执行日志\n");
  printf("  -?  --help         显示本帮助信息\n");
}

void initValue(float* values, int* exponents, float* output, float* gold, unsigned int N) {

  for (unsigned int i=0; i<N+VECTOR_WIDTH; i++)
  {
    // 随机输入值
    values[i] = -1.f + 4.f * static_cast<float>(rand()) / RAND_MAX;
    exponents[i] = rand() % EXP_MAX;
    output[i] = 0.f;
    gold[i] = 0.f;
  }

}

bool verifyResult(float* values, int* exponents, float* output, float* gold, int N) {
  int incorrect = -1;
  float epsilon = 0.00001;
  for (int i=0; i<N+VECTOR_WIDTH; i++) {
    if ( abs(output[i] - gold[i]) > epsilon ) {
      incorrect = i;
      break;
    }
  }

  if (incorrect != -1) {
    if (incorrect >= N)
      printf("你写入了越界的值!\n");
    printf("在 value[%d] 处计算错误!\n", incorrect);
    printf("value  = ");
    for (int i=0; i<N; i++) {
      printf("% f ", values[i]);
    } printf("\n");

    printf("exp    = ");
    for (int i=0; i<N; i++) {
      printf("% 9d ", exponents[i]);
    } printf("\n");

    printf("output = ");
    for (int i=0; i<N; i++) {
      printf("% f ", output[i]);
    } printf("\n");

    printf("gold   = ");
    for (int i=0; i<N; i++) {
      printf("% f ", gold[i]);
    } printf("\n");
    return false;
  }
  printf("计算结果与参考答案完全一致!\n");
  return true;
}

// 计算输入数组 values 中所有元素的绝对值，
// 并把结果存入 output
void absSerial(float* values, float* output, int N) {
  for (int i=0; i<N; i++) {
    float x = values[i];
    if (x < 0) {
      output[i] = -x;
    } else {
      output[i] = x;
    }
  }
}


// 上面 absSerial() 的实现版本，但使用 CS149 intrinsics 进行了向量化
void absVector(float* values, float* output, int N) {
  __cs149_vec_float x;
  __cs149_vec_float result;
  __cs149_vec_float zero = _cs149_vset_float(0.f);
  __cs149_mask maskAll, maskIsNegative, maskIsNotNegative;

//  注意: 请仔细观察这段循环的下标方式。当 (N % VECTOR_WIDTH) != 0 时，
//  这段示例代码并不保证能正确工作。
//  这是为什么？
  for (int i=0; i<N; i+=VECTOR_WIDTH) {

    // 全 1
    maskAll = _cs149_init_ones();

    // 全 0
    maskIsNegative = _cs149_init_ones(0);

    // 从连续的内存地址中加载一个由值组成的 vector
    _cs149_vload_float(x, values+i, maskAll);               // x = values[i];

    // 根据谓词条件设置 mask
    _cs149_vlt_float(maskIsNegative, x, zero, maskAll);     // if (x < 0) {

    // 使用 mask 执行指令（"if" 分支）
    _cs149_vsub_float(result, zero, x, maskIsNegative);      //   output[i] = -x;

    // 对 maskIsNegative 取反，生成 "else" 分支用的 mask
    maskIsNotNegative = _cs149_mask_not(maskIsNegative);     // } else {

    // 执行指令（"else" 分支）
    _cs149_vload_float(result, values+i, maskIsNotNegative); //   output[i] = x; }

    // 把结果写回内存
    _cs149_vstore_float(output+i, result, maskAll);
  }
}


// 接收一个 values 数组和一个 exponents 数组
//
// 对每个元素，计算 values[i]^exponents[i]，并把值 clamp（截断）到
// 9.999。结果存入 output。
void clampedExpSerial(float* values, int* exponents, float* output, int N) {
  for (int i=0; i<N; i++) {
    float x = values[i];
    int y = exponents[i];
    if (y == 0) {
      output[i] = 1.f;
    } else {
      float result = x;
      int count = y - 1;
      while (count > 0) {
        result *= x;
        count--;
      }
      if (result > 9.999999f) {
        result = 9.999999f;
      }
      output[i] = result;
    }
  }
}

void clampedExpVector(float* values, int* exponents, float* output, int N) {

  //
  // CS149 学生 TODO: 在这里实现你的
  // clampedExpSerial() 向量化版本。
  //
  // 你的解法应当对任意 N 和 VECTOR_WIDTH 都能工作，
  // 而不只是 VECTOR_WIDTH 恰好整除 N 的情况
  //
  
}

// 返回 values 中所有元素的和
float arraySumSerial(float* values, int N) {
  float sum = 0;
  for (int i=0; i<N; i++) {
    sum += values[i];
  }

  return sum;
}

// 返回 values 中所有元素的和
// 你可以假设 N 是 VECTOR_WIDTH 的倍数
// 你可以假设 VECTOR_WIDTH 是 2 的幂
float arraySumVector(float* values, int N) {
  
  //
  // CS149 学生 TODO: 在这里实现 arraySumSerial 的向量化版本
  //
  
  for (int i=0; i<N; i+=VECTOR_WIDTH) {

  }

  return 0.0;
}

