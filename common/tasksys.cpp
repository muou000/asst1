/*
  Copyright (c) 2011-2023, Intel Corporation

  SPDX-License-Identifier: BSD-3-Clause
*/

/*
  本文件实现了几个简单的任务系统（task system），提供 ispc 生成的代码
  所使用的三个入口函数，用于处理 ispc 程序中的 'launch' 和 'sync' 语句。
  关于如何在 ispc 程序中使用任务并行，请参阅 ispc 文档中的
  "Task Parallelism: Language Syntax" 一节；关于这里实现的与任务相关的
  入口函数，请参阅 "Task Parallelism: Runtime Requirements" 一节。

  本文件包含多个任务系统实现，分别基于：
    - 微软的 Concurrency Runtime（ISPC_USE_CONCRT）
    - Apple 的 Grand Central Dispatch（ISPC_USE_GCD）
    - 原生 pthreads（ISPC_USE_PTHREADS、ISPC_USE_PTHREADS_FULLY_SUBSCRIBED）
    - TBB（ISPC_USE_TBB_TASK_GROUP、ISPC_USE_TBB_PARALLEL_FOR）
    - OpenMP（ISPC_USE_OMP）
    - HPX（ISPC_USE_HPX）

  任务系统的实现可以在编译时选择：在命令行上定义相应的预处理器符号
  即可（例如：-D ISPC_USE_TBB）。
  并非所有平台与任务系统的组合都有意义。
  如果没有请求任何任务系统，则会为当前平台选择一个合理的默认
  任务系统。可以选择的任务系统如下：

#define ISPC_USE_GCD
#define ISPC_USE_CONCRT
#define ISPC_USE_PTHREADS
#define ISPC_USE_PTHREADS_FULLY_SUBSCRIBED
#define ISPC_USE_OMP
#define ISPC_USE_TBB_TASK_GROUP
#define ISPC_USE_TBB_PARALLEL_FOR

  ISPC_USE_PTHREADS_FULLY_SUBSCRIBED 模型实质上会接管整台机器：为每个
  超线程（hyper-thread）分配一个 pthread，然后用自旋锁（spinlock）和
  atomic 操作来管理任务。该模型适用于 KNC 这类任务可以独占机器的场景，
  但当机器上还需要运行其他任务时就不太合适了。

#define ISPC_USE_CREW
#define ISPC_USE_HPX
  HPX 模型要求先设置好 HPX 运行时环境。这可以手动完成，例如使用
  hpx::init，或者包含 hpx/hpx_main.hpp——后者会把 main() 函数作为入口
  点并搭建运行时系统。thread 的数量可以通过命令行参数 --hpx:threads
  指定，使用 "all" 表示每个处理单元（processing unit）各启动一个 thread。

*/

#if !(defined ISPC_USE_CONCRT || defined ISPC_USE_GCD || defined ISPC_USE_PTHREADS ||                                  \
      defined ISPC_USE_PTHREADS_FULLY_SUBSCRIBED || defined ISPC_USE_TBB_TASK_GROUP ||                                 \
      defined ISPC_USE_TBB_PARALLEL_FOR || defined ISPC_USE_OMP || defined ISPC_USE_HPX)

// 如果编译命令行没有选择任务模型，就挑选一个合理的默认值
#if defined(_WIN32) || defined(_WIN64)
#define ISPC_USE_CONCRT
#elif defined(__linux__) || defined(__FreeBSD__)
#define ISPC_USE_PTHREADS
#elif defined(__APPLE__)
#define ISPC_USE_GCD
//#define ISPC_USE_PTHREADS
#endif
#endif // 编译命令行上未指定任务模型

#if defined(_WIN32) || defined(_WIN64)
#define ISPC_IS_WINDOWS
#elif defined(__linux__) || defined(__FreeBSD__) // 就本文的用途而言两者基本相同
#define ISPC_IS_LINUX
#elif defined(__APPLE__)
#define ISPC_IS_APPLE
#endif

#define DBG(x)

#ifdef ISPC_IS_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif // ISPC_IS_WINDOWS
#ifdef ISPC_USE_CONCRT
#include <concrt.h>
using namespace Concurrency;
#endif // ISPC_USE_CONCRT
#ifdef ISPC_USE_GCD
#include <dispatch/dispatch.h>
#include <pthread.h>
#endif // ISPC_USE_GCD
#ifdef ISPC_USE_PTHREADS
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#endif // ISPC_USE_PTHREADS
#ifdef ISPC_USE_PTHREADS_FULLY_SUBSCRIBED
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
//#include <stdexcept>
#include <stack>
#endif // ISPC_USE_PTHREADS_FULLY_SUBSCRIBED
#ifdef ISPC_USE_TBB_PARALLEL_FOR
#include <tbb/parallel_for.h>
#endif // ISPC_USE_TBB_PARALLEL_FOR
#ifdef ISPC_USE_TBB_TASK_GROUP
#include <tbb/task_group.h>
#endif // ISPC_USE_TBB_TASK_GROUP
#ifdef ISPC_USE_OMP
#include <omp.h>
#endif // ISPC_USE_OMP
#ifdef ISPC_USE_HPX
#include <hpx/include/async.hpp>
#include <hpx/lcos/wait_all.hpp>
#endif // ISPC_USE_HPX
#ifdef ISPC_IS_LINUX
#include <stdlib.h>
#endif // ISPC_IS_LINUX

#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ispc 生成的 'task' 函数的签名（函数指针类型）
typedef void (*TaskFuncType)(void *data, int threadIndex, int threadCount, int taskIndex, int taskCount, int taskIndex0,
                             int taskIndex1, int taskIndex2, int taskCount0, int taskCount1, int taskCount2);

// 用于保存每个任务数据的小结构体
struct TaskInfo {
    TaskFuncType func;
    void *data;
    int taskIndex;
    int taskCount3d[3];
#if defined(ISPC_USE_CONCRT)
    event taskEvent;
#endif
    int taskCount() const { return taskCount3d[0] * taskCount3d[1] * taskCount3d[2]; }
    int taskIndex0() const { return taskIndex % taskCount3d[0]; }
    int taskIndex1() const { return (taskIndex / taskCount3d[0]) % taskCount3d[1]; }
    int taskIndex2() const { return taskIndex / (taskCount3d[0] * taskCount3d[1]); }
    int taskCount0() const { return taskCount3d[0]; }
    int taskCount1() const { return taskCount3d[1]; }
    int taskCount2() const { return taskCount3d[2]; }
    TaskInfo() {}
};

// ispc 要求这些函数具有 C 链接属性（名字不被 C++ name mangling 改写）
extern "C" {
void ISPCLaunch(void **handlePtr, void *f, void *data, int countx, int county, int countz);
void *ISPCAlloc(void **handlePtr, int64_t size, int32_t alignment);
void ISPCSync(void *handle);
}

///////////////////////////////////////////////////////////////////////////
// TaskGroupBase

#define LOG_TASK_QUEUE_CHUNK_SIZE 14
#define MAX_TASK_QUEUE_CHUNKS 128
#define TASK_QUEUE_CHUNK_SIZE (1 << LOG_TASK_QUEUE_CHUNK_SIZE)

#define MAX_LAUNCHED_TASKS (MAX_TASK_QUEUE_CHUNKS * TASK_QUEUE_CHUNK_SIZE)

#define NUM_MEM_BUFFERS 16

class TaskGroup;

/** TaskGroupBase 结构体为"任务组"（task group）提供公共功能；一个任务组
    是指从单个 ispc 函数内部启动的所有任务的集合。当该函数准备返回时，
    它会等待自己任务组中的全部任务完成，然后才真正返回。
 */
class TaskGroupBase {
  public:
    void Reset();

    int AllocTaskInfo(int count);
    TaskInfo *GetTaskInfo(int index);

    void *AllocMemory(int64_t size, int32_t alignment);

  protected:
    TaskGroupBase();
    ~TaskGroupBase();

    int nextTaskInfoIndex;

  private:
    /* 我们按调用函数的需要，以 TASK_QUEUE_CHUNK_SIZE 个 TaskInfo 结构体
       为一块来分配空间。最多保存 MAX_TASK_QUEUE_CHUNKS 块（如果启动的
       任务数量超过这个上限，程序将在运行时直接退出。）
     */
    TaskInfo *taskInfo[MAX_TASK_QUEUE_CHUNKS];

    /* 我们还会分配一些内存块来服务于 ISPCAlloc() 调用。memBuffers[] 数组
       保存指向这些内存的指针。该数组的第一个元素被初始化为指向 mem，
       之后需要的后续元素则通过动态分配来初始化。
     */
    int curMemBuffer, curMemBufferOffset;
    int memBufferSize[NUM_MEM_BUFFERS];
    char *memBuffers[NUM_MEM_BUFFERS];
    char mem[256];
};

inline TaskGroupBase::TaskGroupBase() {
    nextTaskInfoIndex = 0;

    curMemBuffer = 0;
    curMemBufferOffset = 0;
    memBuffers[0] = mem;
    memBufferSize[0] = sizeof(mem) / sizeof(mem[0]);
    for (int i = 1; i < NUM_MEM_BUFFERS; ++i) {
        memBuffers[i] = nullptr;
        memBufferSize[i] = 0;
    }

    for (int i = 0; i < MAX_TASK_QUEUE_CHUNKS; ++i)
        taskInfo[i] = nullptr;
}

inline TaskGroupBase::~TaskGroupBase() {
    // 注意：不要 delete memBuffers[0]，因为它指向的是 "mem" 成员的开头！
    for (int i = 1; i < NUM_MEM_BUFFERS; ++i)
        delete[](memBuffers[i]);
}

inline void TaskGroupBase::Reset() {
    nextTaskInfoIndex = 0;
    curMemBuffer = 0;
    curMemBufferOffset = 0;
}

inline int TaskGroupBase::AllocTaskInfo(int count) {
    int ret = nextTaskInfoIndex;
    nextTaskInfoIndex += count;
    return ret;
}

inline TaskInfo *TaskGroupBase::GetTaskInfo(int index) {
    int chunk = (index >> LOG_TASK_QUEUE_CHUNK_SIZE);
    int offset = index & (TASK_QUEUE_CHUNK_SIZE - 1);

    if (chunk == MAX_TASK_QUEUE_CHUNKS) {
        fprintf(stderr,
                "当前函数总共已启动 %d 个任务——"
                "这个简单的内置任务系统已经无法处理更多任务了。你可以增大 "
                "TASK_QUEUE_CHUNK_SIZE 和 LOG_TASK_QUEUE_CHUNK_SIZE 的值来"
                "绕过这一限制。"
                "抱歉！正在退出。\n",
                index);
        exit(1);
    }

    if (taskInfo[chunk] == nullptr)
        taskInfo[chunk] = new TaskInfo[TASK_QUEUE_CHUNK_SIZE];
    return &taskInfo[chunk][offset];
}

inline void *TaskGroupBase::AllocMemory(int64_t size, int32_t alignment) {
    char *basePtr = memBuffers[curMemBuffer];
    intptr_t iptr = (intptr_t)(basePtr + curMemBufferOffset);
    iptr = (iptr + (alignment - 1)) & ~(alignment - 1);

    int newOffset = int(iptr - (intptr_t)basePtr + size);
    if (newOffset < memBufferSize[curMemBuffer]) {
        curMemBufferOffset = newOffset;
        return (char *)iptr;
    }

    ++curMemBuffer;
    curMemBufferOffset = 0;
    assert(curMemBuffer < NUM_MEM_BUFFERS);

    int allocSize = 1 << (12 + curMemBuffer);
    allocSize = std::max(int(size + alignment), allocSize);
    char *newBuf = new char[allocSize];
    memBufferSize[curMemBuffer] = allocSize;
    memBuffers[curMemBuffer] = newBuf;
    return AllocMemory(size, alignment);
}

///////////////////////////////////////////////////////////////////////////
// atomic 原子操作及相关工具

static inline void lMemFence() {
    // Windows 的 atomic 函数本身已包含内存栅栏（fence）
#if !defined ISPC_IS_WINDOWS
    __sync_synchronize();
#endif
}

static void *lAtomicCompareAndSwapPointer(void **v, void *newValue, void *oldValue) {
#ifdef ISPC_IS_WINDOWS
    return InterlockedCompareExchangePointer(v, newValue, oldValue);
#else
    void *result = __sync_val_compare_and_swap(v, oldValue, newValue);
    lMemFence();
    return result;
#endif // ISPC_IS_WINDOWS
}

static int32_t lAtomicCompareAndSwap32(volatile int32_t *v, int32_t newValue, int32_t oldValue) {
#ifdef ISPC_IS_WINDOWS
    return InterlockedCompareExchange((volatile LONG *)v, newValue, oldValue);
#else
    int32_t result = __sync_val_compare_and_swap(v, oldValue, newValue);
    lMemFence();
    return result;
#endif // ISPC_IS_WINDOWS
}

#ifndef ISPC_USE_GCD
static inline int32_t lAtomicAdd(volatile int32_t *v, int32_t delta) {
#ifdef ISPC_IS_WINDOWS
    return InterlockedExchangeAdd((volatile LONG *)v, delta) + delta;
#else
    return __sync_fetch_and_add(v, delta);
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////

#ifdef ISPC_USE_CONCRT
// 使用 ConcRT 时，我们完全不需要扩展 TaskGroupBase。
class TaskGroup : public TaskGroupBase {
  public:
    void Launch(int baseIndex, int count);
    void Sync();
};
#endif // ISPC_USE_CONCRT

#ifdef ISPC_USE_GCD
/* 使用 Grand Central Dispatch 时，我们为每个任务组关联一个 GCD dispatch
   group。（之后当需要等待组内所有任务完成时，就在这个 dispatch group
   上等待。）
 */
class TaskGroup : public TaskGroupBase {
  public:
    TaskGroup() { gcdGroup = dispatch_group_create(); }

    void Launch(int baseIndex, int count);
    void Sync();

  private:
    dispatch_group_t gcdGroup;
};
#endif // ISPC_USE_GCD

#ifdef ISPC_USE_PTHREADS
static void *lTaskEntry(void *arg);

class TaskGroup : public TaskGroupBase {
  public:
    TaskGroup() {
        numUnfinishedTasks = 0;
        waitingTasks.reserve(128);
        inActiveList = false;
    }

    void Reset() {
        TaskGroupBase::Reset();
        numUnfinishedTasks = 0;
        assert(inActiveList == false);
        lMemFence();
    }

    void Launch(int baseIndex, int count);
    void Sync();

  private:
    friend void *lTaskEntry(void *arg);

    int32_t numUnfinishedTasks;
    int32_t pad[3];
    std::vector<int> waitingTasks;
    bool inActiveList;
};

#endif // ISPC_USE_PTHREADS

#ifdef ISPC_USE_OMP

class TaskGroup : public TaskGroupBase {
  public:
    void Launch(int baseIndex, int count);
    void Sync();
};

#endif // ISPC_USE_OMP

#ifdef ISPC_USE_TBB_PARALLEL_FOR

class TaskGroup : public TaskGroupBase {
  public:
    void Launch(int baseIndex, int count);
    void Sync();
};

#endif // ISPC_USE_TBB_PARALLEL_FOR

#ifdef ISPC_USE_TBB_TASK_GROUP

class TaskGroup : public TaskGroupBase {
  public:
    void Launch(int baseIndex, int count);
    void Sync();

  private:
    tbb::task_group tbbTaskGroup;
};

#endif // ISPC_USE_TBB_TASK_GROUP

#ifdef ISPC_USE_HPX

class TaskGroup : public TaskGroupBase {
  public:
    void Launch(int baseIndex, int count);
    void Sync();

  private:
    std::vector<hpx::future<void>> futures;
};

#endif // ISPC_USE_HPX

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// Grand Central Dispatch

#ifdef ISPC_USE_GCD

/* 基于 Apple Grand Central Dispatch 的 ispc 程序简单任务系统。 */

static dispatch_queue_t gcdQueue;
static volatile int32_t lock = 0;

static void InitTaskSystem() {
    if (gcdQueue != nullptr)
        return;

    while (1) {
        if (lAtomicCompareAndSwap32(&lock, 1, 0) == 0) {
            if (gcdQueue == nullptr) {
                gcdQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
                assert(gcdQueue != nullptr);
                lMemFence();
            }
            lock = 0;
            break;
        }
    }
}

static void lRunTask(void *ti) {
    TaskInfo *taskInfo = (TaskInfo *)ti;
    // FIXME: 这些是伪造的（bogus）值；如果代码依赖它们在不同 thread 中
    // 具有唯一值，可能会引发 bug。
    int threadIndex = 0;
    int threadCount = 1;

    // 真正运行任务
    taskInfo->func(taskInfo->data, threadIndex, threadCount, taskInfo->taskIndex, taskInfo->taskCount(),
                   taskInfo->taskIndex0(), taskInfo->taskIndex1(), taskInfo->taskIndex2(), taskInfo->taskCount0(),
                   taskInfo->taskCount1(), taskInfo->taskCount2());
}

inline void TaskGroup::Launch(int baseIndex, int count) {
    for (int i = 0; i < count; ++i) {
        TaskInfo *ti = GetTaskInfo(baseIndex + i);
        dispatch_group_async_f(gcdGroup, gcdQueue, ti, lRunTask);
    }
}

inline void TaskGroup::Sync() { dispatch_group_wait(gcdGroup, DISPATCH_TIME_FOREVER); }

#endif // ISPC_USE_GCD

///////////////////////////////////////////////////////////////////////////
// Concurrency Runtime

#ifdef ISPC_USE_CONCRT

static void InitTaskSystem() {
    // 无需初始化
}

static void __cdecl lRunTask(LPVOID param) {
    TaskInfo *ti = (TaskInfo *)param;

    // 真正运行任务。
    // FIXME: 与 OS X 上的 GCD 实现一样，这里传给 threadIndex 和
    // threadCount 这两个内建变量的也是伪造的值，这进而会让使用它们的
    // 代码出现 bug。
    int threadIndex = 0;
    int threadCount = 1;
    ti->func(ti->data, threadIndex, threadCount, ti->taskIndex, ti->taskCount(), ti->taskIndex0(), ti->taskIndex1(),
             ti->taskIndex2(), ti->taskCount0(), ti->taskCount1(), ti->taskCount2());

    // 通过 event 通知该任务已完成
    ti->taskEvent.set();
}

inline void TaskGroup::Launch(int baseIndex, int count) {
    for (int i = 0; i < count; ++i)
        CurrentScheduler::ScheduleTask(lRunTask, GetTaskInfo(baseIndex + i));
}

inline void TaskGroup::Sync() {
    for (int i = 0; i < nextTaskInfoIndex; ++i) {
        TaskInfo *ti = GetTaskInfo(i);
        ti->taskEvent.wait();
        ti->taskEvent.reset();
    }
}

#endif // ISPC_USE_CONCRT

///////////////////////////////////////////////////////////////////////////
// pthreads

#ifdef ISPC_USE_PTHREADS

static volatile int32_t lock = 0;

static int nThreads;
static pthread_t *threads = nullptr;

static pthread_mutex_t taskSysMutex;
static std::vector<TaskGroup *> activeTaskGroups;
static sem_t *workerSemaphore;

static void *lTaskEntry(void *arg) {
    int threadIndex = (int)((int64_t)arg);
    int threadCount = nThreads;

    while (1) {
        int err;
        //
        // 在 semaphore 上等待，直到有更多工作到来而被唤醒。
        //
        if ((err = sem_wait(workerSemaphore)) != 0) {
            fprintf(stderr, "sem_wait 出错: %s\n", strerror(err));
            exit(1);
        }

        //
        // 获取 mutex
        //
        if ((err = pthread_mutex_lock(&taskSysMutex)) != 0) {
            fprintf(stderr, "pthread_mutex_lock 出错: %s\n", strerror(err));
            exit(1);
        }

        if (activeTaskGroups.size() == 0) {
            //
            // 任务队列为空，回到开头继续在 semaphore 上等待
            //
            if ((err = pthread_mutex_unlock(&taskSysMutex)) != 0) {
                fprintf(stderr, "pthread_mutex_unlock 出错: %s\n", strerror(err));
                exit(1);
            }
            continue;
        }

        //
        // 取活动列表中最后一个任务组，以及它的等待任务列表中的
        // 最后一个任务。
        //
        TaskGroup *tg = activeTaskGroups.back();
        assert(tg->waitingTasks.size() > 0);
        int taskNumber = tg->waitingTasks.back();
        tg->waitingTasks.pop_back();

        if (tg->waitingTasks.size() == 0) {
            // 我们刚刚取走了这个任务组的最后一个任务，所以把它从
            // 活动列表中移除。
            activeTaskGroups.pop_back();
            tg->inActiveList = false;
        }

        if ((err = pthread_mutex_unlock(&taskSysMutex)) != 0) {
            fprintf(stderr, "pthread_mutex_unlock 出错: %s\n", strerror(err));
            exit(1);
        }

        //
        // 现在真正运行该任务
        //
        DBG(fprintf(stderr, "运行任务 %d，来自组 %p\n", taskNumber, tg));
        TaskInfo *myTask = tg->GetTaskInfo(taskNumber);
        myTask->func(myTask->data, threadIndex, threadCount, myTask->taskIndex, myTask->taskCount(),
                     myTask->taskIndex0(), myTask->taskIndex1(), myTask->taskIndex2(), myTask->taskCount0(),
                     myTask->taskCount1(), myTask->taskCount2());

        //
        // 递减该任务组中的"未完成任务数"计数器。
        //
        lMemFence();
        lAtomicAdd(&tg->numUnfinishedTasks, -1);
    }

    pthread_exit(nullptr);
    return 0;
}

static void InitTaskSystem() {
    if (threads == nullptr) {
        while (1) {
            if (lAtomicCompareAndSwap32(&lock, 1, 0) == 0) {
                if (threads == nullptr) {
                    // 我们启动的 thread 数量比核心数少一个，
                    // 因为主 thread 自己也会从任务队列中领取任务。
                    nThreads = sysconf(_SC_NPROCESSORS_ONLN) - 1;

                    int err;
                    if ((err = pthread_mutex_init(&taskSysMutex, nullptr)) != 0) {
                        fprintf(stderr, "创建 mutex 出错: %s\n", strerror(err));
                        exit(1);
                    }

                    constexpr std::size_t FILENAME_MAX_LEN{1024UL};
                    char name[FILENAME_MAX_LEN];
                    bool success = false;
                    srand(time(nullptr));
                    for (int i = 0; i < 10; i++) {
                        // 某些平台（例如 FreeBSD）要求名字以斜杠开头
                        snprintf(name, FILENAME_MAX_LEN, "/ispc_task.%d.%d", static_cast<int>(getpid()), static_cast<int>(rand()));
                        workerSemaphore = sem_open(name, O_CREAT, S_IRUSR | S_IWUSR, 0);
                        if (workerSemaphore != SEM_FAILED) {
                            success = true;
                            break;
                        }
                        fprintf(stderr, "创建 %s 失败\n", name);
                    }

                    if (!success) {
                        fprintf(stderr, "创建 semaphore 出错 (%s): %s\n", name, strerror(errno));
                        exit(1);
                    }

                    threads = (pthread_t *)malloc(nThreads * sizeof(pthread_t));
                    if (threads == nullptr) {
                        fprintf(stderr, "创建 pthread 出错: %s\n", strerror(err));
                        exit(1);
                    }

                    for (int i = 0; i < nThreads; ++i) {
                        err = pthread_create(&threads[i], nullptr, &lTaskEntry, (void *)((long long)i));
                        if (err != 0) {
                            fprintf(stderr, "创建 pthread %d 出错: %s\n", i, strerror(err));
                            exit(1);
                        }
                    }

                    activeTaskGroups.reserve(64);
                }

                // 确保上面的所有修改都已写入内存之后，再清除锁。
                lMemFence();
                lock = 0;
                break;
            }
        }
    }
}

inline void TaskGroup::Launch(int baseCoord, int count) {
    //
    // 获取 mutex，添加任务
    //
    int err;
    if ((err = pthread_mutex_lock(&taskSysMutex)) != 0) {
        fprintf(stderr, "pthread_mutex_lock 出错: %s\n", strerror(err));
        exit(1);
    }

    // 把对应的一组任务加入该任务组的"等待运行"列表。
    //
    // FIXME: 为此持有一把全局 mutex 有点难看，其实我们只需要保证没有
    // 其他人正在访问这个任务组的 waitingTasks 列表即可。（不过，一次
    // 改用每个 TaskGroup 一把 mutex 的小实验反而显示出更差的性能！）
    for (int i = 0; i < count; ++i)
        waitingTasks.push_back(baseCoord + i);

    // 如果该任务组还不在全局活动列表中，就把它加进去。
    if (inActiveList == false) {
        activeTaskGroups.push_back(this);
        inActiveList = true;
    }

    if ((err = pthread_mutex_unlock(&taskSysMutex)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock 出错: %s\n", strerror(err));
        exit(1);
    }

    //
    // 更新该任务组中剩余待运行任务的数量计数。
    //
    lMemFence();
    lAtomicAdd(&numUnfinishedTasks, count);

    //
    // 对 worker semaphore 执行 post 操作，唤醒那些正睡眠等待任务到来的
    // worker thread
    //
    for (int i = 0; i < count; ++i)
        if ((err = sem_post(workerSemaphore)) != 0) {
            fprintf(stderr, "sem_post 出错: %s\n", strerror(err));
            exit(1);
        }
}

inline void TaskGroup::Sync() {
    DBG(fprintf(stderr, "同步 %p - %d 个未完成\n", tg, numUnfinishedTasks));

    while (numUnfinishedTasks > 0) {
        // 这个组里的任务还没有全部完成。反正我们也没有别的事可做，
        // 就试着来这里帮忙吧……

        DBG(fprintf(stderr, "同步进行中 %p - %d 个未完成\n", tg, numUnfinishedTasks));

        //
        // 获取全局任务系统 mutex，以便领取一个任务来做
        //
        int err;
        if ((err = pthread_mutex_lock(&taskSysMutex)) != 0) {
            fprintf(stderr, "pthread_mutex_lock 出错: %s\n", strerror(err));
            exit(1);
        }

        TaskInfo *myTask = nullptr;
        TaskGroup *runtg = this;
        if (waitingTasks.size() > 0) {
            int taskNumber = waitingTasks.back();
            waitingTasks.pop_back();

            if (waitingTasks.size() == 0) {
                // 这个组里已经没有可开始运行的任务了，
                // 所以把它从活动任务列表中移除。
                activeTaskGroups.erase(std::find(activeTaskGroups.begin(), activeTaskGroups.end(), this));
                inActiveList = false;
            }
            myTask = GetTaskInfo(taskNumber);
            DBG(fprintf(stderr, "在 sync 中运行任务 %d，来自组 %p\n", taskNumber, tg));
        } else {
            // 其他 thread 已经在处理这个组里的全部任务了，所以我们
            // 没法通过自己运行一个任务来帮忙。我们将尝试从另一个组
            // 领一个任务来运行，让自己派上用场。
            if (activeTaskGroups.size() == 0) {
                // 没有剩下的活动任务组了——我们无事可做。
                if ((err = pthread_mutex_unlock(&taskSysMutex)) != 0) {
                    fprintf(stderr, "pthread_mutex_unlock 出错: %s\n", strerror(err));
                    exit(1);
                }
                // FIXME: 我们在这里基本上是在忙等（busy-waiting），在
                // 有超线程（hyper-threading）的世界里这格外浪费。更好的
                // 做法是让这个 thread 在一个条件变量（condition variable）
                // 上睡眠，当该组的最后一个任务完成时再唤醒它。
                usleep(1);
                continue;
            }

            // 从另一个任务组获取一个要运行的任务。
            runtg = activeTaskGroups.back();
            assert(runtg->waitingTasks.size() > 0);

            int taskNumber = runtg->waitingTasks.back();
            runtg->waitingTasks.pop_back();
            if (runtg->waitingTasks.size() == 0) {
                // 这个组里已经没有可开始运行的任务了，所以把它从
                // 活动任务列表中移除。
                activeTaskGroups.pop_back();
                runtg->inActiveList = false;
            }
            myTask = runtg->GetTaskInfo(taskNumber);
            DBG(fprintf(stderr, "在 sync 中运行任务 %d，来自其他组 %p\n", taskNumber, runtg));
        }

        if ((err = pthread_mutex_unlock(&taskSysMutex)) != 0) {
            fprintf(stderr, "pthread_mutex_unlock 出错: %s\n", strerror(err));
            exit(1);
        }

        //
        // 执行 _myTask_ 的工作
        //
        // FIXME: 这里的 thread index/thread count 同样是伪造的值..
        myTask->func(myTask->data, 0, 1, myTask->taskIndex, myTask->taskCount(), myTask->taskIndex0(),
                     myTask->taskIndex1(), myTask->taskIndex2(), myTask->taskCount0(), myTask->taskCount1(),
                     myTask->taskCount2());

        //
        // 递减未完成任务数计数器
        //
        lMemFence();
        lAtomicAdd(&runtg->numUnfinishedTasks, -1);
    }
    DBG(fprintf(stderr, "组 %p 的 sync 完成!n", tg));
}

#endif // ISPC_USE_PTHREADS

///////////////////////////////////////////////////////////////////////////
// OpenMP

#ifdef ISPC_USE_OMP

static void InitTaskSystem() {
    // 无需初始化
}

inline void TaskGroup::Launch(int baseIndex, int count) {
#pragma omp parallel
    {
        const int threadIndex = omp_get_thread_num();
        const int threadCount = omp_get_num_threads();

#pragma omp for schedule(runtime)
        for (int i = 0; i < count; i++) {
            TaskInfo *ti = GetTaskInfo(baseIndex + i);

            // 真正运行任务。
            ti->func(ti->data, threadIndex, threadCount, ti->taskIndex, ti->taskCount(), ti->taskIndex0(),
                     ti->taskIndex1(), ti->taskIndex2(), ti->taskCount0(), ti->taskCount1(), ti->taskCount2());
        }
    }
}

inline void TaskGroup::Sync() {}

#endif // ISPC_USE_OMP

///////////////////////////////////////////////////////////////////////////
// Thread Building Blocks

#ifdef ISPC_USE_TBB_PARALLEL_FOR

static void InitTaskSystem() {
    // 默认无需初始化
    // tbb::task_scheduler_init();
}

inline void TaskGroup::Launch(int baseIndex, int count) {
    tbb::parallel_for(0, count, [=](int i) {
        TaskInfo *ti = GetTaskInfo(baseIndex + i);

        // 真正运行任务。
        // TBB 不公开 task -> thread 的映射关系，所以我们假装它是 1:1 的
        int threadIndex = ti->taskIndex;
        int threadCount = ti->taskCount();

        ti->func(ti->data, threadIndex, threadCount, ti->taskIndex, ti->taskCount(), ti->taskIndex0(), ti->taskIndex1(),
                 ti->taskIndex2(), ti->taskCount0(), ti->taskCount1(), ti->taskCount2());
    });
}

inline void TaskGroup::Sync() {}

#endif // ISPC_USE_TBB_PARALLEL_FOR

#ifdef ISPC_USE_TBB_TASK_GROUP

static void InitTaskSystem() {
    // 默认无需初始化
    // tbb::task_scheduler_init();
}

inline void TaskGroup::Launch(int baseIndex, int count) {
    for (int i = 0; i < count; i++) {
        tbbTaskGroup.run([=]() {
            TaskInfo *ti = GetTaskInfo(baseIndex + i);

            // TBB 不公开 task -> thread 的映射关系，所以我们假装它是 1:1 的
            int threadIndex = ti->taskIndex;
            int threadCount = ti->taskCount();
            ti->func(ti->data, threadIndex, threadCount, ti->taskIndex, ti->taskCount(), ti->taskIndex0(),
                     ti->taskIndex1(), ti->taskIndex2(), ti->taskCount0(), ti->taskCount1(), ti->taskCount2());
        });
    }
}

inline void TaskGroup::Sync() { tbbTaskGroup.wait(); }

#endif // ISPC_USE_TBB_TASK_GROUP

///////////////////////////////////////////////////////////////////////////
// ISPC_USE_HPX

#ifdef ISPC_USE_HPX

static void InitTaskSystem() {}

inline void TaskGroup::Launch(int baseIndex, int count) {
    for (int i = 0; i < count; ++i) {
        TaskInfo *ti = GetTaskInfo(baseIndex + i);
        int threadIndex = i;
        int threadCount = count;
        futures.push_back(hpx::async(ti->func, ti->data, threadIndex, threadCount, ti->taskIndex, ti->taskCount(),
                                     ti->taskIndex0(), ti->taskIndex1(), ti->taskIndex2(), ti->taskCount0(),
                                     ti->taskCount1(), ti->taskCount2()));
    }
}

inline void TaskGroup::Sync() {
    hpx::wait_all(futures);
    futures.clear();
}
#endif
///////////////////////////////////////////////////////////////////////////

#ifndef ISPC_USE_PTHREADS_FULLY_SUBSCRIBED

#define MAX_FREE_TASK_GROUPS 64
static TaskGroup *freeTaskGroups[MAX_FREE_TASK_GROUPS];

static inline TaskGroup *AllocTaskGroup() {
    for (int i = 0; i < MAX_FREE_TASK_GROUPS; ++i) {
        TaskGroup *tg = freeTaskGroups[i];
        if (tg != nullptr) {
            void *ptr = lAtomicCompareAndSwapPointer((void **)(&freeTaskGroups[i]), nullptr, tg);
            if (ptr != nullptr) {
                return (TaskGroup *)ptr;
            }
        }
    }

    return new TaskGroup;
}

static inline void FreeTaskGroup(TaskGroup *tg) {
    tg->Reset();

    for (int i = 0; i < MAX_FREE_TASK_GROUPS; ++i) {
        if (freeTaskGroups[i] == nullptr) {
            void *ptr = lAtomicCompareAndSwapPointer((void **)&freeTaskGroups[i], tg, nullptr);
            if (ptr == nullptr)
                return;
        }
    }

    delete tg;
}

///////////////////////////////////////////////////////////////////////////

void ISPCLaunch(void **taskGroupPtr, void *func, void *data, int count0, int count1, int count2) {
    const int count = count0 * count1 * count2;
    TaskGroup *taskGroup;
    if (*taskGroupPtr == nullptr) {
        InitTaskSystem();
        taskGroup = AllocTaskGroup();
        *taskGroupPtr = taskGroup;
    } else
        taskGroup = (TaskGroup *)(*taskGroupPtr);

    int baseIndex = taskGroup->AllocTaskInfo(count);
    for (int i = 0; i < count; ++i) {
        TaskInfo *ti = taskGroup->GetTaskInfo(baseIndex + i);
        ti->func = (TaskFuncType)func;
        ti->data = data;
        ti->taskIndex = i;
        ti->taskCount3d[0] = count0;
        ti->taskCount3d[1] = count1;
        ti->taskCount3d[2] = count2;
    }
    taskGroup->Launch(baseIndex, count);
}

void ISPCSync(void *h) {
    TaskGroup *taskGroup = (TaskGroup *)h;
    if (taskGroup != nullptr) {
        taskGroup->Sync();
        FreeTaskGroup(taskGroup);
    }
}

void *ISPCAlloc(void **taskGroupPtr, int64_t size, int32_t alignment) {
    TaskGroup *taskGroup;
    if (*taskGroupPtr == nullptr) {
        InitTaskSystem();
        taskGroup = AllocTaskGroup();
        *taskGroupPtr = taskGroup;
    } else
        taskGroup = (TaskGroup *)(*taskGroupPtr);

    return taskGroup->AllocMemory(size, alignment);
}

#else // ISPC_USE_PTHREADS_FULLY_SUBSCRIBED

#define MAX_LIVE_TASKS 1024

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// 用于保存每个任务数据的小结构体
struct Task {
  public:
    TaskFuncType func;
    void *data;
    volatile int32_t taskIndex;
    int taskCount;

    volatile int numDone;
    int liveIndex; // 在活动任务队列中的索引

    inline int noMoreWork() { return taskIndex >= taskCount; }
    /*! 某个 thread 完成了对该任务的工作 --> 递减 num locks */
    // inline void lock() { lAtomicAdd(&locks,1); }
    // inline void unlock() { lAtomicAdd(&locks,-1); }
    inline int nextJob() { return lAtomicAdd(&taskIndex, 1); }
    inline int numJobs() { return taskCount; }
    inline void schedule(int idx) {
        taskIndex = 0;
        numDone = 0;
        liveIndex = idx;
    }
    inline void run(int idx, int threadIdx);
    inline void markOneDone() { lAtomicAdd(&numDone, 1); }
    inline void wait() {
        while (!noMoreWork()) {
            int next = nextJob();
            if (next < numJobs())
                run(next, 0);
        }
        while (numDone != taskCount) {
            usleep(1);
        }
    }
};

///////////////////////////////////////////////////////////////////////////
class TaskSys {
    static int numThreadsRunning;
    struct LiveTask {
        volatile int locks;  /*!< 该任务上的锁数量。初始化为
                                  NUM_THREADS+1，之后每个看到它的 thread
                                  都会将其递减。只有当 'active' 被设为
                                  true 时该值才有效 */
        volatile int active; /*! worker 会在这个标志上自旋，直到它
                                 变为 active */
        Task *task;

        inline void doneWithThis() { lAtomicAdd(&locks, -1); }
        LiveTask() : active(0), locks(-1) {}
    };

  public:
    volatile int nextScheduleIndex; /*! 任务队列中下一个用于插入活动任务
                                        的索引 */

    // inline int inc_begin() { int old = begin; begin = (begin+1)%MAX_TASKS; return old; }
    // inline int inc_end() { int old = end; end = (end+1)%MAX_TASKS; return old; }

    LiveTask taskQueue[MAX_LIVE_TASKS];
    std::stack<Task *> taskMem;

    static TaskSys *global;

    TaskSys() : nextScheduleIndex(0) {
        TaskSys::global = this;
        Task *mem = new Task[MAX_LIVE_TASKS]; //< 实际上可以多于同时活动的任务数
        for (int i = 0; i < MAX_LIVE_TASKS; i++) {
            taskMem.push(mem + i);
        }
        createThreads();
    }

    inline Task *allocOne() {
        pthread_mutex_lock(&mutex);
        if (taskMem.empty()) {
            fprintf(stderr, "活动任务（live task）过多。"
                            "请修改 MAX_LIVE_TASKS 的值并重新编译。\n");
            exit(1);
        }
        Task *task = taskMem.top();
        taskMem.pop();
        pthread_mutex_unlock(&mutex);
        return task;
    }

    static inline void init() {
        if (global)
            return;
        pthread_mutex_lock(&mutex);
        if (global == nullptr)
            global = new TaskSys;
        pthread_mutex_unlock(&mutex);
    }

    void createThreads();
    int nThreads;
    pthread_t *thread;

    void threadFct();

    inline void schedule(Task *t) {
        pthread_mutex_lock(&mutex);
        int liveIndex = nextScheduleIndex;
        nextScheduleIndex = (nextScheduleIndex + 1) % MAX_LIVE_TASKS;
        if (taskQueue[liveIndex].active) {
            fprintf(stderr, "任务队列资源耗尽。"
                            "请修改 MAX_LIVE_TASKS 的值并重新编译。\n");
            exit(1);
        }
        taskQueue[liveIndex].task = t;
        t->schedule(liveIndex);
        taskQueue[liveIndex].locks = numThreadsRunning + 1; // _worker_ thread 数再加上创建者
        taskQueue[liveIndex].active = true;
        pthread_mutex_unlock(&mutex);
    }

    void sync(Task *task) {
        task->wait();
        int liveIndex = task->liveIndex;
        while (taskQueue[liveIndex].locks > 1) {
            usleep(1);
        }
        _mm_free(task->data);
        pthread_mutex_lock(&mutex);
        taskMem.push(task); // 回收任务
        taskQueue[liveIndex].active = false;
        pthread_mutex_unlock(&mutex);
    }
};

void TaskSys::threadFct() {
    int myIndex = 0; // lAtomicAdd(&threadIdx,1);
    while (1) {
        while (!taskQueue[myIndex].active) {
            usleep(4);
            continue;
        }

        Task *mine = taskQueue[myIndex].task;
        while (!mine->noMoreWork()) {
            int job = mine->nextJob();
            if (job >= mine->numJobs())
                break;
            mine->run(job, myIndex);
        }
        taskQueue[myIndex].doneWithThis();
        myIndex = (myIndex + 1) % MAX_LIVE_TASKS;
    }
}

inline void Task::run(int idx, int threadIdx) {
    (*this->func)(data, threadIdx, TaskSys::global->nThreads, idx, taskCount);
    markOneDone();
}

void *_threadFct(void *data) {
    ((TaskSys *)data)->threadFct();
    return nullptr;
}

void TaskSys::createThreads() {
    init();
    int reserved = 4;
    int minid = 2;
    nThreads = sysconf(_SC_NPROCESSORS_ONLN) - reserved;

    thread = (pthread_t *)malloc(nThreads * sizeof(pthread_t));

    numThreadsRunning = 0;
    for (int i = 0; i < nThreads; ++i) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);

        int threadID = minid + i;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(threadID, &cpuset);
        int ret = pthread_attr_setaffinity_np(&attr, sizeof(cpuset), &cpuset);

        int err = pthread_create(&thread[i], &attr, &_threadFct, this);
        ++numThreadsRunning;
        if (err != 0) {
            fprintf(stderr, "创建 pthread %d 出错: %s\n", i, strerror(err));
            exit(1);
        }
    }
}

TaskSys *TaskSys::global = nullptr;
int TaskSys::numThreadsRunning = 0;

///////////////////////////////////////////////////////////////////////////

void ISPCLaunch(void **taskGroupPtr, void *func, void *data, int count) {
    Task *ti = *(Task **)taskGroupPtr;
    ti->func = (TaskFuncType)func;
    ti->data = data;
    ti->taskIndex = 0;
    ti->taskCount = count;
    TaskSys::global->schedule(ti);
}

void ISPCSync(void *h) {
    Task *task = (Task *)h;
    assert(task);
    TaskSys::global->sync(task);
}

void *ISPCAlloc(void **taskGroupPtr, int64_t size, int32_t alignment) {
    TaskSys::init();
    Task *task = TaskSys::global->allocOne();
    *taskGroupPtr = task;
    task->data = _mm_malloc(size, alignment);
    return task->data; //*taskGroupPtr;
}

#endif // ISPC_USE_PTHREADS_FULLY_SUBSCRIBED
