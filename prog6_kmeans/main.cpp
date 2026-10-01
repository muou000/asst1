#include <algorithm>
#include <iostream>
#include <math.h>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "CycleTimer.h"

#define SEED 7
#define SAMPLE_RATE 1e-2

using namespace std;

// 主要的计算函数
extern void kMeansThread(double *data, double *clusterCentroids,
                      int *clusterAssignments, int M, int N, int K,
                      double epsilon);
extern double dist(double *x, double *y, int nDim);

// 工具函数
extern void logToFile(string filename, double sampleRate, double *data,
                      int *clusterAssignments, double *clusterCentroids, int M,
                      int N, int K);
extern void writeData(string filename, double *data, double *clusterCentroids,
                      int *clusterAssignments, int *M_p, int *N_p, int *K_p,
                      double *epsilon_p);
extern void readData(string filename, double **data, double **clusterCentroids,
                     int **clusterAssignments, int *M_p, int *N_p, int *K_p,
                     double *epsilon_p);

// 生成数据的函数
double randDouble() {
  return static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
}

void initData(double *data, int M, int N) {
  int K = 10;
  double *centers = new double[K * N];

  // 高斯噪声
  double mean = 0.0;
  double stddev = 0.5;
  std::default_random_engine generator;
  std::normal_distribution<double> normal_dist(mean, stddev);

  // 随机生成一些点，作为数据围绕的中心
  for (int k = 0; k < K; k++) {
    for (int n = 0; n < N; n++) {
      centers[k * N + n] = randDouble();
    }
  }

  // 均匀地聚类
  for (int m = 0; m < M; m++) {
    int startingPoint = rand() % K; // 从哪个中心出发
    for (int n = 0; n < N; n++) {
      double noise = normal_dist(generator);
      data[m * N + n] = centers[startingPoint * N + n] + noise;
    }
  }

  delete[] centers;
}

void initCentroids(double *clusterCentroids, int K, int N) {
  // 初始化各 centroid（彼此靠得很近——这样会更有意思一点）
  for (int n = 0; n < N; n++) {
    clusterCentroids[n] = randDouble();
  }
  for (int k = 1; k < K; k++) {
    for (int n = 0; n < N; n++) {
      clusterCentroids[k * N + n] =
          clusterCentroids[n] + (randDouble() - 0.5) * 0.1;
    }
  }
}

int main() {
  srand(SEED);

  int M, N, K;
  double epsilon;

  double *data;
  double *clusterCentroids;
  int *clusterAssignments;

  // 注意：我们将使用 data.dat 中的数据来给你的提交评分，
  // 这些数据由下面这个函数读入
  readData("./data.dat", &data, &clusterCentroids, &clusterAssignments, &M, &N,
           &K, &epsilon);

  // 注意：如果你想自己生成数据（仅供把玩），可以使用下面的代码
  /*
  M = 1e6;
  N = 100;
  K = 3;
  epsilon = 0.1;

  data = new double[M * N];
  clusterCentroids = new double[K * N];
  clusterAssignments = new int[M];

  // 初始化数据
  initData(data, M, N);
  initCentroids(clusterCentroids, K, N);

  // 初始化各数据点的 cluster assignment
  for (int m = 0; m < M; m++) {
    double minDist = 1e30;
    int bestAssignment = -1;
    for (int k = 0; k < K; k++) {
      double d = dist(&data[m * N], &clusterCentroids[k * N], N);
      if (d < minDist) {
        minDist = d;
        bestAssignment = k;
      }
    }
    clusterAssignments[m] = bestAssignment;
  }

  // 取消注释即可生成数据文件
  // writeData("./data.dat", data, clusterCentroids, clusterAssignments, &M, &N,
  //           &K, &epsilon);
  */

  printf("运行 K-means，参数为：M=%d, N=%d, K=%d, epsilon=%f\n", M, N,
         K, epsilon);

  // 记录算法的初始状态
  logToFile("./start.log", SAMPLE_RATE, data, clusterAssignments,
            clusterCentroids, M, N, K);

  double startTime = CycleTimer::currentSeconds();
  kMeansThread(data, clusterCentroids, clusterAssignments, M, N, K, epsilon);
  double endTime = CycleTimer::currentSeconds();
  printf("[总耗时]: %.3f ms\n", (endTime - startTime) * 1000);

  // 记录算法结束时的状态
  logToFile("./end.log", SAMPLE_RATE, data, clusterAssignments,
            clusterCentroids, M, N, K);

  delete[] data;
  delete[] clusterCentroids;
  delete[] clusterAssignments;
  return 0;
}