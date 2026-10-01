#include <stdio.h>
#include <cstdlib>
#include <thread>

#include "CycleTimer.h"

typedef struct {
    float x0, x1;
    float y0, y1;
    unsigned int width;
    unsigned int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
} WorkerArgs;


extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);


//
// workerThreadStart --
//
// thread 的入口函数。
void workerThreadStart(WorkerArgs * const args) {

    // TODO FOR CS149 学生：在这里实现 worker thread 的函数体。每个
    // thread 都应调用 mandelbrotSerial() 来计算输出图像的一部分。
    // 例如，在一个使用两个 thread 的程序中，thread 0 可以计算图像的
    // 上半部分，thread 1 计算图像的下半部分。

    printf("来自 thread %d 的 Hello world\n", args->threadId);
}

//
// MandelbrotThread --
//
// Mandelbrot 集图像生成的多 thread 实现。
// 各个执行流（thread）通过创建 std::thread 来产生。
void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations, int output[])
{
    static constexpr int MAX_THREADS = 32;

    if (numThreads > MAX_THREADS)
    {
        fprintf(stderr, "错误: 允许的最大 thread 数为 %d\n", MAX_THREADS);
        exit(1);
    }

    // 创建一些尚不代表任何 thread 的 thread 对象。
    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    for (int i=0; i<numThreads; i++) {
        // TODO FOR CS149 学生：你可能需要、也可能不需要修改这里每个
        // thread 的参数。下面的代码为每个 thread 复制了相同的参数
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = width;
        args[i].height = height;
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;

        args[i].threadId = i;
    }

    // 创建（spawn）各个 worker thread。注意这里只创建了 numThreads-1 个
    // std::thread，应用程序的主 thread 本身也充当一个 worker。
    for (int i=1; i<numThreads; i++) {
        workers[i] = std::thread(workerThreadStart, &args[i]);
    }

    workerThreadStart(&args[0]);

    // join（等待完成）各个 worker thread
    for (int i=1; i<numThreads; i++) {
        workers[i].join();
    }
}

