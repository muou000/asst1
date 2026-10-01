/*

  注：这段代码修改自 Intel 提供的示例代码。为遵守 Intel 的开源
  许可协议，其版权声明保留在下方。

  -----------------------------------------------------------------

  Copyright (c) 2010-2011, Intel Corporation
  版权所有。

  在满足以下条件的前提下，允许以源代码和二进制形式重新分发和使用
  本软件，无论是否对其进行修改：

    * 以源代码形式重新分发时，必须保留上述版权声明、本条件清单
      以及下面的免责声明。

    * 以二进制形式重新分发时，必须在随分发包提供的文档和/或其他
      材料中重现上述版权声明、本条件清单以及下面的免责声明。

    * 未经事先书面许可，不得使用 Intel Corporation 的名称或其
      贡献者的名称来认可或推广由本软件派生的产品。

   本软件由版权持有者和贡献者"按原样"（AS IS）提供，不作出任何
   明示或暗示的担保，包括但不限于对适销性和特定用途适用性的暗示
   担保。在任何情况下，版权持有者或贡献者均不对任何直接的、间接
   的、偶然的、特殊的、惩罚性的或后果性的损害（包括但不限于采购
   替代商品或服务；使用、数据或利润的损失；或业务中断）承担责任，
   无论该责任是如何引起的，也无论其责任理论是基于合同、严格责任
   还是侵权行为（包括过失或其他原因），即使已被告知发生此类损害
   的可能性。
*/


static inline int mandel(float c_re, float c_im, int count)
{
    float z_re = c_re, z_im = c_im;
    int i;
    for (i = 0; i < count; ++i) {

        if (z_re * z_re + z_im * z_im > 4.f)
            break;

        float new_re = z_re*z_re - z_im*z_im;
        float new_im = 2.f * z_re * z_im;
        z_re = c_re + new_re;
        z_im = c_im + new_im;
    }

    return i;
}

//
// MandelbrotSerial --
//
// 计算一幅可视化 Mandelbrot 集合的图像。结果数组中保存的是：与某个
// 像素对应的复数在能被判定不属于该集合（rejected）之前所需的迭代次数。
//
// * x0, y0, x1, y1 描述复平面坐标到图像视口（viewport）的映射关系。
// * width, height 描述输出图像的尺寸。
// * startRow, totalRows 描述要计算图像的哪一部分。
void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int totalRows,
    int maxIterations,
    int output[])
{
    float dx = (x1 - x0) / width;
    float dy = (y1 - y0) / height;

    int endRow = startRow + totalRows;

    for (int j = startRow; j < endRow; j++) {
        for (int i = 0; i < width; ++i) {
            float x = x0 + i * dx;
            float y = y0 + j * dy;

            int index = (j * width + i);
            output[index] = mandel(x, y, maxIterations);
        }
    }
}

