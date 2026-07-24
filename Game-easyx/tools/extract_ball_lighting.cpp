// ============================================================
// 球体光照数据提取工具
// 用法: 编译运行后生成 ball_lighting.h
// 纯 C++ 标准库，无任何外部依赖
// ============================================================
#include <cstdio>
#include <cmath>
#include <cstring>

// 输出二维数组的亮度数据 (0-255)
static void writeBrightness(const char* name, int radius, FILE* f) {
    int size = radius * 2 + 2;  // 同 SDL2 纹理尺寸
    int cx = radius + 1;
    int cy = radius + 1;

    // 光源方向 (左上 Blinn-Phong)
    double lx = -0.6, ly = -0.6, lz = 0.8;
    double lLen = sqrt(lx*lx + ly*ly + lz*lz);
    lx /= lLen; ly /= lLen; lz /= lLen;

    fprintf(f, "// %s 球体 Blinn-Phong 亮度数据 (R=%d, size=%d)\n", name, radius, size);
    fprintf(f, "static const unsigned char %s[%d * %d] = {\n", name, size, size);

    for (int y = 0; y < size; y++) {
        fprintf(f, "    ");
        for (int x = 0; x < size; x++) {
            double dx = (double)(x - cx) / radius;
            double dy = (double)(y - cy) / radius;
            double distSq = dx * dx + dy * dy;

            int bright = 0;
            if (distSq <= 1.0) {
                double dist = sqrt(distSq);
                double nx, ny;
                if (dist > 0.001) { nx = dx / dist; ny = dy / dist; }
                else              { nx = 0; ny = 0; }
                double nz = sqrt(1.0 - distSq);

                // Lambertian 漫反射
                double NdotL = nx*lx + ny*ly + nz*lz;
                if (NdotL < 0.0) NdotL = 0.0;
                double ambient = 0.18;
                double diffuse = ambient + (1.0 - ambient) * NdotL;

                // Blinn-Phong 镜面高光
                double hx = lx, hy = ly, hz = lz + 1.0;
                double hLen = sqrt(hx*hx + hy*hy + hz*hz);
                double NdotH = (nx*hx + ny*hy + nz*hz) / hLen;
                if (NdotH < 0.0) NdotH = 0.0;
                double specular = pow(NdotH, 32.0) * 0.7;

                // 边缘暗化 (Fresnel-like)
                double edge = 1.0 - distSq * 0.3;

                double val = diffuse * edge + specular;
                if (val > 1.0) val = 1.0;
                bright = (int)(val * 255);
                if (bright > 255) bright = 255;
            }
            fprintf(f, "%3d", bright);
            if (x < size - 1) fprintf(f, ",");
        }
        fprintf(f, "%s\n", (y < size - 1) ? "," : "");
    }
    fprintf(f, "};\n\n");
}

int main() {
    const char* path = "../Game/ball_lighting.h";
    FILE* f = fopen(path, "w");
    if (!f) {
        printf("ERROR: Cannot open %s for writing\n", path);
        return 1;
    }

    fprintf(f, "// ============================================================\n");
    fprintf(f, "// 预计算球体光照数据 (自动生成, 勿手动编辑)\n");
    fprintf(f, "// 工具: tools/extract_ball_lighting.cpp\n");
    fprintf(f, "// ============================================================\n");
    fprintf(f, "#pragma once\n\n");

    // M1 关节球 R=50
    writeBrightness("BALL1_BRIGHT", 50, f);

    // M2 末端球 R=40
    writeBrightness("BALL2_BRIGHT", 40, f);

    fclose(f);
    printf("Generated: %s\n", path);
    return 0;
}
