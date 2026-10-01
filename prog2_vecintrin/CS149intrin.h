// 在这里定义向量单元（vector unit）的宽度
#define VECTOR_WIDTH 4

#ifndef CS149INTRIN_H_
#define CS149INTRIN_H_

#include <cstdlib>
#include <cmath>
#include "logger.h"

//*******************
//* 类型定义 *
//*******************

extern Logger CS149Logger;

template <typename T>
struct __cs149_vec {
  T value[VECTOR_WIDTH];
};

// 使用 __cs149_mask 声明一个 mask
struct __cs149_mask : __cs149_vec<bool> {};

// 使用 __cs149_vec_float 声明一个浮点数 vector 寄存器
#define __cs149_vec_float __cs149_vec<float>

// 使用 __cs149_vec_int 声明一个整数 vector 寄存器
#define __cs149_vec_int   __cs149_vec<int>

//***********************
//* 函数定义 *
//***********************

// 返回一个 mask：前 N 个 lane 被初始化为 1，其余 lane 为 0
__cs149_mask _cs149_init_ones(int first = VECTOR_WIDTH);

// 返回 maska 的取反结果
__cs149_mask _cs149_mask_not(__cs149_mask &maska);

// 返回 (maska | maskb)
__cs149_mask _cs149_mask_or(__cs149_mask &maska, __cs149_mask &maskb);

// 返回 (maska & maskb)
__cs149_mask _cs149_mask_and(__cs149_mask &maska, __cs149_mask &maskb);

// 统计 maska 中 1 的个数
int _cs149_cntbits(__cs149_mask &maska);

// 若 vector lane 处于激活状态，则把寄存器设置为 value
//  否则保留旧值
void _cs149_vset_float(__cs149_vec_float &vecResult, float value, __cs149_mask &mask);
void _cs149_vset_int(__cs149_vec_int &vecResult, int value, __cs149_mask &mask);
// 为方便使用，返回一个所有 lane 都初始化为 value 的 vector 寄存器
__cs149_vec_float _cs149_vset_float(float value);
__cs149_vec_int _cs149_vset_int(int value);

// 若 vector lane 处于激活状态，则把 vector 寄存器 src 中的值复制到 vector 寄存器 dest
// 否则保留旧值
void _cs149_vmove_float(__cs149_vec_float &dest, __cs149_vec_float &src, __cs149_mask &mask);
void _cs149_vmove_int(__cs149_vec_int &dest, __cs149_vec_int &src, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则把数组 src 中的值加载到 vector 寄存器 dest
//  否则保留旧值
void _cs149_vload_float(__cs149_vec_float &dest, float* src, __cs149_mask &mask);
void _cs149_vload_int(__cs149_vec_int &dest, int* src, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则把 vector 寄存器 src 中的值存储到数组 dest
//  否则保留旧值
void _cs149_vstore_float(float* dest, __cs149_vec_float &src, __cs149_mask &mask);
void _cs149_vstore_int(int* dest, __cs149_vec_int &src, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回计算结果 (veca + vecb)
//  否则保留旧值
void _cs149_vadd_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vadd_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回计算结果 (veca - vecb)
//  否则保留旧值
void _cs149_vsub_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vsub_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回计算结果 (veca * vecb)
//  否则保留旧值
void _cs149_vmult_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vmult_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回计算结果 (veca / vecb)
//  否则保留旧值
void _cs149_vdiv_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vdiv_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);


// 若 vector lane 处于激活状态，则返回绝对值计算结果 abs(veca)
//  否则保留旧值
void _cs149_vabs_float(__cs149_vec_float &vecResult, __cs149_vec_float &veca, __cs149_mask &mask);
void _cs149_vabs_int(__cs149_vec_int &vecResult, __cs149_vec_int &veca, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回 (veca > vecb) 的比较结果 mask
//  否则保留旧值
void _cs149_vgt_float(__cs149_mask &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vgt_int(__cs149_mask &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回 (veca < vecb) 的比较结果 mask
//  否则保留旧值
void _cs149_vlt_float(__cs149_mask &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_vlt_int(__cs149_mask &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// 若 vector lane 处于激活状态，则返回 (veca == vecb) 的比较结果 mask
//  否则保留旧值
void _cs149_veq_float(__cs149_mask &vecResult, __cs149_vec_float &veca, __cs149_vec_float &vecb, __cs149_mask &mask);
void _cs149_veq_int(__cs149_mask &vecResult, __cs149_vec_int &veca, __cs149_vec_int &vecb, __cs149_mask &mask);

// 将相邻的元素对相加，即
//  [0 1 2 3] -> [0+1 0+1 2+3 2+3]
void _cs149_hadd_float(__cs149_vec_float &vecResult, __cs149_vec_float &vec);

// 执行偶奇交错重排（even-odd interleaving）：所有偶数下标的元素移到数组的前半部分，
//  奇数下标的元素移到后半部分，即
//  [0 1 2 3 4 5 6 7] -> [0 2 4 6 1 3 5 7]
void _cs149_interleave_float(__cs149_vec_float &vecResult, __cs149_vec_float &vec);

// 添加一条自定义日志，帮助调试
void addUserLog(const char * logStr);

#endif
