#include <stdio.h>
#include <algorithm>
#include <cstdlib>
#include "CycleTimer.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]);
#ifdef USE_ISPC_OBJ
extern "C"
{
	void mandelbrot_ispc(float x0, float y0,
		float x1, float y1,
		int width, int height,
		int maxIterations,
		int *output);

	void mandelbrot_ispc_withtasks(float x0, float y0,
		float x1, float y1,
		int width, int height,
		int maxIterations,
		int *output,
		const int taskNum);
}
#else
#include "mandelbrot_ispc.h"
using namespace ispc;
#endif
extern void mandelbrotThread(
    int numThreads,
    float x0, float y0, float x1, float y1,
    int width, int height,
    int maxIterations,
    int output[]);

extern void writePPMImage(
    int* data,
    int width, int height,
    const char *filename,
    int maxIterations);

bool verifyResult (int *gold, int *result, int width, int height) {
    int i, j;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if (gold[i * width + j] != result[i * width + j]) {
                printf ("Mismatch : [%d][%d], Expected : %d, Actual : %d\n",
                            i, j, gold[i * width + j], result[i * width + j]);
                return 0;
            }
        }
    }

    return 1;
}

void
scaleAndShift(float& x0, float& x1, float& y0, float& y1,
              float scale,
              float shiftX, float shiftY)
{

    x0 *= scale;
    x1 *= scale;
    y0 *= scale;
    y1 *= scale;
    x0 += shiftX;
    x1 += shiftX;
    y0 += shiftY;
    y1 += shiftY;

}

void usage(const char* progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Program Options:\n");
    printf("  -t  --tasks        Run ISPC code implementation with tasks\n");
    printf("  -v  --view <INT>   Use specified view settings\n");
    printf("  -?  --help         This message\n");
}


int main(int argc, char** argv) {

    const unsigned int width = 1280;
    const unsigned int height = 960;
    const int maxIterations = 256;

    float x0 = -2;
    float x1 = 1;
    float y0 = -1;
    float y1 = 1;

    bool useTasks = false;
	int taskNum = 2;

    // parse commandline options ////////////////////////////////////////////

	for (int i = 1; i < argc; i++) {
		if (i < argc - 1)
		{
			if (strcmp(argv[i], "-v") == 0)
			{
				int viewIndex = atoi(argv[i + 1]);
				// change view settings
				if (viewIndex == 2) {
					float scaleValue = .015f;
					float shiftX = -.986f;
					float shiftY = .30f;
					scaleAndShift(x0, x1, y0, y1, scaleValue, shiftX, shiftY);
				}
				else if (viewIndex > 1) {
					fprintf(stderr, "Invalid view index\n");
					return 1;
				}
			}
			else if (strcmp(argv[i], "-t") == 0)
			{
				printf("use task\n");
				useTasks = true;
				if (i < argc - 1) {
					taskNum = atoi(argv[i + 1]);
					if (taskNum == 0) // not number
						taskNum = 2;
					if (height % taskNum != 0) {
						printf("height %% taskNum != 0\n");
						exit(1);
					}
				}
			}
		}
		else
		{
			if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "?") == 0)
			{
				usage(argv[0]);
				return 1;
			}
		}
	}
    // end parsing of commandline options

    int *output_serial = new int[width*height];
    int *output_ispc = new int[width*height];
    int *output_ispc_tasks = new int[width*height];

    for (unsigned int i = 0; i < width * height; ++i)
        output_serial[i] = 0;

    //
    // Run the serial implementation. Teport the minimum time of three
    // runs for robust timing.
    //
    double minSerial = 1e30;
    for (int i = 0; i < 5; ++i) {
        double startTime = CycleTimer::currentSeconds();
        mandelbrotSerial(x0, y0, x1, y1, width, height, 0, height, maxIterations, output_serial);
        double endTime = CycleTimer::currentSeconds();
        minSerial = std::min(minSerial, endTime - startTime);
    }

    printf("[mandelbrot serial]:\t\t[%.3f] ms\n", minSerial * 1000);
    writePPMImage(output_serial, width, height, "mandelbrot-serial.ppm", maxIterations);

    // Clear out the buffer
    for (unsigned int i = 0; i < width * height; ++i)
        output_ispc[i] = 0;

    //
    // Compute the image using the ispc implementation
    //
    double minISPC = 1e30;
    for (int i = 0; i < 5; ++i) {
        double startTime = CycleTimer::currentSeconds();
        mandelbrot_ispc(x0, y0, x1, y1, width, height, maxIterations, output_ispc);
        double endTime = CycleTimer::currentSeconds();
        minISPC = std::min(minISPC, endTime - startTime);
    }

    printf("[mandelbrot ispc]:\t\t[%.3f] ms\n", minISPC * 1000);
    writePPMImage(output_ispc, width, height, "mandelbrot-ispc.ppm", maxIterations);


    if (! verifyResult (output_serial, output_ispc, width, height)) {
        printf ("Error : ISPC output differs from sequential output\n");

        delete[] output_serial;
        delete[] output_ispc;
        delete[] output_ispc_tasks;

        return 1;
    }

    // Clear out the buffer
    for (unsigned int i = 0; i < width * height; ++i) {
        output_ispc_tasks[i] = 0;
    }

    double minTaskISPC = 1e30;
    if (useTasks) {
        //
        // Tasking version of the ISPC code
        //
        for (int i = 0; i < 3; ++i) {
            double startTime = CycleTimer::currentSeconds();
            mandelbrot_ispc_withtasks(x0, y0, x1, y1, width, height, maxIterations, output_ispc_tasks, taskNum);
            double endTime = CycleTimer::currentSeconds();
            minTaskISPC = std::min(minTaskISPC, endTime - startTime);
        }

        printf("[mandelbrot multicore ispc]:\t[%.3f] ms\n", minTaskISPC * 1000);
        writePPMImage(output_ispc_tasks, width, height, "mandelbrot-task-ispc.ppm", maxIterations);

        if (! verifyResult (output_serial, output_ispc_tasks, width, height)) {
            printf ("Error : ISPC output differs from sequential output\n");
            return 1;
        }
    }

    printf("\t\t\t\t(%.2fx speedup from ISPC)\n", minSerial/minISPC);
    if (useTasks) {
        printf("\t\t\t\t(%.2fx speedup from task ISPC)\n", minSerial/minTaskISPC);
    }

    delete[] output_serial;
    delete[] output_ispc;
    delete[] output_ispc_tasks;

	system("pause");

    return 0;
}
