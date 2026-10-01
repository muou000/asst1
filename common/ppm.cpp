#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>



void
writePPMImage(int* data, int width, int height, const char *filename, int maxIterations)
{
    FILE *fp = fopen(filename, "wb");

    // 写入 PPM 文件头
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");

    for (int i = 0; i < width*height; ++i) {

        // 先对该像素的迭代次数做截断（clamp），再把数值缩放到 0-1
        // 范围。然后将结果取一次幂（指数 <1），以提升低迭代次数
        // 像素的亮度。 aka：让画面看起来更酷。

        float mapped = pow( std::min(static_cast<float>(maxIterations),
                                     static_cast<float>(data[i])) / 256.f, .5f);

        // 转换回 0-255 范围（8 位通道）
        unsigned char result = static_cast<unsigned char>(255.f * mapped);
        for (int j = 0; j < 3; ++j)
            fputc(result, fp);
    }
    fclose(fp);
    printf("已写入图像文件 %s\n", filename);
}
