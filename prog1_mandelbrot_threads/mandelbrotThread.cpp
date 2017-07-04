#include <stdio.h>
#include <thread>

#include "CycleTimer.h"

extern void mandelbrotSerial(
	float x0, float y0, float x1, float y1,
	int width, int height,
	int startRow, int numRows,
	int maxIterations,
	int output[]);


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


typedef struct {
	float x0, x1;
	float y0, y1;
	unsigned int width;
	unsigned int height;
	int maxIterations;
	int* output;
	int threadId;
	int numThreads;
	float dy;
	int startRow;
} WorkerArgs;


//
// workerThreadStart --
//
// Thread entrypoint.
void* workerThreadStart(WorkerArgs& arg) {

	//WorkerArgs* args = static_cast<WorkerArgs*>(threadArgs);

	// TODO: Implement worker thread here.

	float dx = (arg.x1 - arg.x0) / arg.width;
	float dy = arg.dy;

	for (int j = 0; j < arg.height; j++) {
		for (int i = 0; i < arg.width; ++i) {
			float x = arg.x0 + i * dx;
			float y = arg.y0 + (j + arg.startRow) * dy;

			int index = (j * arg.width + i);
			arg.output[index] = mandel(x, y, arg.maxIterations);
		}
	}

	//printf("Hello world from thread %d\n", arg.threadId);

	return NULL;
}

//
// MandelbrotThread --
//
// Multi-threaded implementation of mandelbrot set image generation.
// Multi-threading performed via pthreads.
void mandelbrotThread(
	int numThreads,
	float x0, float y0, float x1, float y1,
	int width, int height,
	int maxIterations, int output[])
{
	const static int MAX_THREADS = 48;

	if (numThreads > MAX_THREADS)
	{
		fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
		exit(1);
	}

	std::thread workers[MAX_THREADS - 1];
	WorkerArgs args[MAX_THREADS];

	int dh = height / numThreads;
	float dy = (y1 - y0) / height;

	for (int i = 0; i < numThreads; i++) {
		// TODO: Set thread arguments here.
		args[i].threadId = i;
		args[i].width = width;
		args[i].maxIterations = maxIterations;
		args[i].x0 = x0;
		args[i].x1 = x1;
		args[i].numThreads = numThreads;
		args[i].dy = dy;

		args[i].height = dh;
		args[i].output = output + i*dh*width;
		args[i].y0 = y0; args[i].y1 = y1;
		args[i].startRow = i * dh;
		//args[i].y0 = y0 + i * dh * dy;
		//args[i].y1 = y0 + (i + 1) * dh * dy;
		if (i == numThreads - 1) {
			args[i].height = height - dh * (numThreads - 1);
			//args[i].y1 = y1;
		}
		//printf("i=%d, h = %d, startrow = %d\n", i, args[i].height, args[i].startRow);
	}

	// Fire up the worker threads.  Note that numThreads-1 pthreads
	// are created and the main app thread is used as a worker as
	// well.

	for (int i = 0; i < numThreads - 1; i++)
	{
		workers[i] = std::thread(workerThreadStart, args[i + 1]);
	}

	workerThreadStart(args[0]);

	// wait for worker threads to complete
	for (int i = 0; i < numThreads - 1; i++)
		workers[i].join();
}
