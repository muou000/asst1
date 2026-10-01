#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "CycleTimer.h"

using namespace std;

typedef struct {
  // Control work assignments
  int start, end;

  // Shared by all functions
  double *data;
  double *clusterCentroids;
  int *clusterAssignments;
  double *currCost;
  int M, N, K;
} WorkerArgs;


/**
 * 检查算法是否已经收敛。
 *
 * @param prevCost 指向 K 维数组的指针，其中存放上一次迭代中
 *    各 cluster 的代价。
 * @param currCost 指向 K 维数组的指针，其中存放当前迭代中
 *    各 cluster 的代价。
 * @param epsilon 预先定义的超参数，用于判断算法何时收敛。
 * @param K cluster 的数量。
 *
 * 注意：不要修改这个函数！！！
 */
static bool stoppingConditionMet(double *prevCost, double *currCost,
                                 double epsilon, int K) {
  for (int k = 0; k < K; k++) {
    if (abs(prevCost[k] - currCost[k]) > epsilon)
      return false;
  }
  return true;
}

/**
 * 计算两个 nDim 维点之间的 L2 距离。
 *
 * @param x 指向表示第一个数据点的数组开头的指针。
 * @param y 指向表示第二个数据点的数组开头的指针。
 * @param nDim 每个数据点的维度（元素个数）
 *     （x 和 y 必须相同）。
 */
double dist(double *x, double *y, int nDim) {
  double accum = 0.0;
  for (int i = 0; i < nDim; i++) {
    accum += pow((x[i] - y[i]), 2);
  }
  return sqrt(accum);
}

/**
 * 将每个数据点分配给“距离它最近”的 cluster centroid。
 */
void computeAssignments(WorkerArgs *const args) {
  double *minDist = new double[args->M];
  
  // 初始化数组
  for (int m =0; m < args->M; m++) {
    minDist[m] = 1e30;
    args->clusterAssignments[m] = -1;
  }

  // 将数据点分配给最近的 centroid
  for (int k = args->start; k < args->end; k++) {
    for (int m = 0; m < args->M; m++) {
      double d = dist(&args->data[m * args->N],
                      &args->clusterCentroids[k * args->N], args->N);
      if (d < minDist[m]) {
        minDist[m] = d;
        args->clusterAssignments[m] = k;
      }
    }
  }

  delete[] minDist;
}

/**
 * 根据各数据点的 cluster assignment，为每个 cluster 计算新的
 * centroid 位置。
 */
void computeCentroids(WorkerArgs *const args) {
  int *counts = new int[args->K];

  // 全部清零
  for (int k = 0; k < args->K; k++) {
    counts[k] = 0;
    for (int n = 0; n < args->N; n++) {
      args->clusterCentroids[k * args->N + n] = 0.0;
    }
  }


  // 累加分配到各 cluster 的数据点的贡献
  for (int m = 0; m < args->M; m++) {
    int k = args->clusterAssignments[m];
    for (int n = 0; n < args->N; n++) {
      args->clusterCentroids[k * args->N + n] +=
          args->data[m * args->N + n];
    }
    counts[k]++;
  }

  // 计算均值
  for (int k = 0; k < args->K; k++) {
    counts[k] = max(counts[k], 1); // 防止除以 0
    for (int n = 0; n < args->N; n++) {
      args->clusterCentroids[k * args->N + n] /= counts[k];
    }
  }

  delete[] counts;
}

/**
 * 计算每个 cluster 的代价。用于检查算法是否已经收敛。
 */
void computeCost(WorkerArgs *const args) {
  double *accum = new double[args->K];

  // 全部清零
  for (int k = 0; k < args->K; k++) {
    accum[k] = 0.0;
  }

  // 对所有分配到该 centroid 的数据点求代价之和
  for (int m = 0; m < args->M; m++) {
    int k = args->clusterAssignments[m];
    accum[k] += dist(&args->data[m * args->N],
                     &args->clusterCentroids[k * args->N], args->N);
  }

  // 更新代价
  for (int k = args->start; k < args->end; k++) {
    args->currCost[k] = accum[k];
  }

  delete[] accum;
}

/**
 * 计算 K-Means 算法，使用 std::thread 对工作并行化。
 *
 * @param data 指向长度为 M*N 的数组的指针，表示待聚类的 M 个不同的
 *     N 维数据点。数据按“数据点优先”（data point major）的格式存放，
 *     即 data[i*N] 是数组中第 i 个数据点的起始位置。第 i 个数据点的
 *     N 个值，就是 data[i*N] 到 data[(i+1) * N] 范围内的那 N 个值。
 * @param clusterCentroids 指向长度为 K*N 的数组的指针，表示 K 个不同的
 *     N 维 cluster centroid。数据的存放方式与上面对 data 的说明相同。
 * @param clusterAssignments 指向长度为 M 的数组的指针，表示每个数据点的
 *     cluster assignment，其中 clusterAssignments[i] = j 表示数据点 i
 *     距离第 j 个 cluster centroid 最近。
 * @param M 待聚类的数据点个数。
 * @param N 数据点的维度。
 * @param K cluster centroid 的个数。
 * @param epsilon 当对所有 i（i = 0, 1, ..., K-1）都满足
 *     |currCost[i] - prevCost[i]| < epsilon 时，认为算法已经收敛。
 */
void kMeansThread(double *data, double *clusterCentroids, int *clusterAssignments,
               int M, int N, int K, double epsilon) {

  // 用于跟踪收敛情况
  double *prevCost = new double[K];
  double *currCost = new double[K];

  // WorkerArgs 结构体用于向各函数传入输入，
  // 并从函数带回输出。
  WorkerArgs args;
  args.data = data;
  args.clusterCentroids = clusterCentroids;
  args.clusterAssignments = clusterAssignments;
  args.currCost = currCost;
  args.M = M;
  args.N = N;
  args.K = K;

  // 初始化用于跟踪代价的数组
  for (int k = 0; k < K; k++) {
    prevCost[k] = 1e30;
    currCost[k] = 0.0;
  }

  /* K-Means 算法主循环 */
  int iter = 0;
  while (!stoppingConditionMet(prevCost, currCost, epsilon, K)) {
    // 更新代价数组（用于检查收敛条件）
    for (int k = 0; k < K; k++) {
      prevCost[k] = currCost[k];
    }

    // 设置 args 结构体
    args.start = 0;
    args.end = K;

    computeAssignments(&args);
    computeCentroids(&args);
    computeCost(&args);

    iter++;
  }

  delete[] currCost;
  delete[] prevCost;
}
