#include <stdio.h>
#include <algorithm>

#include "CycleTimer.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

extern void saxpySerial(int N, float a, float* X, float* Y, float* result);

#ifdef USE_ISPC_OBJ
extern "C"
{
	void saxpy_ispc(int N, float a, float* X, float* Y, float* result);
	void saxpy_ispc_withtasks(int N, float a, float* X, float* Y, float* result);
}
#else
#include "saxpy_ispc.h"
using namespace ispc;
#endif
// return GB/s
static float
toBW(int bytes, float sec) {
    return static_cast<float>(bytes) / (1024.f * 1024.f * 1024.f) / sec;
}

static float
toGFLOPS(int ops, float sec) {
    return static_cast<float>(ops) / 1e9f / sec;
}

int main() {

    const unsigned int N = 24 * 1000 * 1000; // 20 M element vectors (~80 MB)
    const unsigned int TOTAL_BYTES = 4 * N * sizeof(float);
    const unsigned int TOTAL_FLOPS = 2 * N;

    float scale = 2.f;

    float* arrayX = new float[N];
    float* arrayY = new float[N];
    float* result = new float[N];

    // initialize array values
    for (unsigned int i=0; i<N; i++)
    {
        arrayX[i] = (float)i;
        arrayY[i] = (float)i;
        result[i] = 0.f;
    }

    //
    // Run the serial implementation. Repeat three times for robust
    // timing.
    //
    double minSerial = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime =CycleTimer::currentSeconds();
        saxpySerial(N, scale, arrayX, arrayY, result);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

 printf("[saxpy serial]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minSerial * 1000,
           toBW(TOTAL_BYTES, minSerial),
           toGFLOPS(TOTAL_FLOPS, minSerial));

    // Clear out the buffer
    for (unsigned int i = 0; i < N; ++i)
        result[i] = 0.f;

    //
    // Run the ISPC (single core) implementation
    //
    double minISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpy_ispc(N, scale, arrayX, arrayY, result);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
    }

    printf("[saxpy ispc]:\t\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minISPC * 1000,
           toBW(TOTAL_BYTES, (float)minISPC),
           toGFLOPS(TOTAL_FLOPS, (float)minISPC));

    // Clear out the buffer
    for (unsigned int i = 0; i < N; ++i)
        result[i] = 0.f;

    //
    // Run the ISPC (multi-core) implementation
    //
    double minTaskISPC = 1e30;
    for (int i = 0; i < 3; ++i) {
        double startTime = CycleTimer::currentSeconds();
        saxpy_ispc_withtasks(N, scale, arrayX, arrayY, result);
        double endTime = CycleTimer::currentSeconds();
        minTaskISPC = std::min(minTaskISPC, endTime - startTime);
    }

    printf("[saxpy task ispc]:\t[%.3f] ms\t[%.3f] GB/s\t[%.3f] GFLOPS\n",
           minTaskISPC * 1000,
           toBW(TOTAL_BYTES, (float)minTaskISPC),
           toGFLOPS(TOTAL_FLOPS, (float)minTaskISPC));

    printf("\t\t\t\t(%.2fx speedup from use of tasks)\n", minISPC/minTaskISPC);
    printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    //printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);

    delete[] arrayX;
    delete[] arrayY;
    delete[] result;

	system("pause");

    return 0;
}
