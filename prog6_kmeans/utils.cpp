#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string>

using namespace std;

void logToFile(string filename, double sampleRate, double *data,
               int *clusterAssignments, double *clusterCentroids, int M, int N,
               int K) {
  ofstream logFile;
  logFile.open(filename);

  // 写入文件头
  logFile << M << "," << N << "," << K << endl;

  // 记录数据点
  for (int m = 0; m < M; m++) {
    if (static_cast<double>(rand()) / static_cast<double>(RAND_MAX) <
        sampleRate) {
      logFile << "Example " << m << ", cluster " << clusterAssignments[m]
              << ": ";
      for (int n = 0; n < N; n++) {
        logFile << data[m * N + n] << " ";
      }
      logFile << "\n";
    }
  }

  // 记录各 centroid
  for (int k = 0; k < K; k++) {
    logFile << "Centroid " << k << ": ";
    for (int n = 0; n < N; n++) {
      logFile << clusterCentroids[k * N + n] << " ";
    }
    logFile << "\n";
  }

  logFile.close();
}

void writeData(string filename, double *data, double *clusterCentroids,
               int *clusterAssignments, int *M_p, int *N_p, int *K_p,
               double *epsilon_p) {
  int M = *M_p;
  int N = *N_p;
  int K = *K_p;

  ofstream dataFile(filename, ios::out | ios::binary);
  dataFile.write((char *)M_p, sizeof(int));
  dataFile.write((char *)N_p, sizeof(int));
  dataFile.write((char *)K_p, sizeof(int));
  dataFile.write((char *)epsilon_p, sizeof(double));
  dataFile.write((char *)data, sizeof(double) * M * N);
  dataFile.write((char *)clusterCentroids, sizeof(double) * K * N);
  dataFile.write((char *)clusterAssignments, sizeof(int) * M);
  dataFile.close();
}

void readData(string filename, double **data, double **clusterCentroids,
              int **clusterAssignments, int *M_p, int *N_p, int *K_p,
              double *epsilon_p) {
  cout << "正在读取 data.dat..." << endl;

  ifstream dataFile(filename, ios::in | ios::binary);
  if (dataFile.fail()) {
      cout << "无法打开文件！请确认 data.dat 存在……程序退出。" << endl;
      exit(EXIT_FAILURE);
  }

  dataFile.read((char *)M_p, sizeof(int));
  dataFile.read((char *)N_p, sizeof(int));
  dataFile.read((char *)K_p, sizeof(int));
  dataFile.read((char *)epsilon_p, sizeof(double));

  int M = *M_p;
  int N = *N_p;
  int K = *K_p;

  *data = new double[M * N];
  *clusterCentroids = new double[K * N];
  *clusterAssignments = new int[M];

  dataFile.read((char *)*data, sizeof(double) * M * N);
  dataFile.read((char *)*clusterCentroids, sizeof(double) * K * N);
  dataFile.read((char *)*clusterAssignments, sizeof(int) * M);
  dataFile.close();
}