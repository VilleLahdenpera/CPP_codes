

#include <CL/cl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <lodepng.h>
#include <lodepng.cpp>
#include <vector>
#include <omp.h>
#include <windows.h>


#define MAX_SOURCE_SIZE (0x010000)
#define NUM_THREADS 5
#define MAXDISP 64 
#define MINDISP 0
double PCFreq = 0.0;
__int64 CounterStart = 0;
using namespace std;
#include <chrono>
using namespace std::chrono;
vector<double> times;




// Functions to count execution times
void StartCounter() {
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		std::cout << "QueryPerformanceFrequency failed!\n";
	PCFreq = double(li.QuadPart) / 1000.0;
	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}
double GetCounter() {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}




unsigned char* ZNCC(unsigned char* image1, unsigned char* image2, int w, int h, int wsy, int wsx, int min_d, int max_d) {
	StartCounter();
	uint8_t* dispmap = (uint8_t*)malloc(w*h);
	int d;
	int best_d;
	float avg1;
	float avg2;
	float wv1;
	float wv2;
	float sd1;
	float sd2;
	float cur_z;
	float best_z;
	float hwsx = wsx / 2;
	float hwsy = wsy / 2;
	float wsa = hwsx * hwsy;


	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {

			best_d = max_d;
			best_z = -1;

			for (d = min_d; d <= max_d; d++) {

				// Count average
				avg1 = 0;
				avg2 = 0;

				for (int a = -hwsy; a < hwsy; a++) {

					for (int b = -hwsx; b < hwsx; b++) {

						if (!(i + a >= 0) || !(i + a < h) || !(j + a >= 0) || !(j + b < w) || !(j + b - d >= 0) || !(j + b - d < w)) {
							continue;
						}

						// Sum all pixels in window size
						avg1 += image1[(i + a) * w + (j + b)];
						avg2 += image2[(i + a) * w + (j + b - d)];

					}
				}
				avg1 = avg1 / wsa;
				avg2 = avg2 / wsa;


				cur_z = 0;
				sd1 = 0;
				sd2 = 0;


				// Count ZNCC 
				for (int a = -hwsy; a < hwsy; a++) {

					for (int b = -hwsx; b < hwsx; b++) {

						if (!(i + a >= 0) || !(i + a < h) || !(j + a >= 0) || !(j + b < w) || !(j + b - d >= 0) || !(j + b - d < w)) {
							continue;
						}

						wv1 = image1[(i + a) * w + (j + b)] - avg1;
						wv2 = image2[(i + a) * w + (j + b - d)] - avg2;

						cur_z += wv1 * wv2;

						sd1 += wv1 * wv1;
						sd2 += wv2 * wv2;
					}
				}

				cur_z = cur_z / ((sqrt(sd1)) * (sqrt(sd2)));

				if (cur_z > best_z) {
					best_z = cur_z;
					best_d = d;
				}

			}
			dispmap[w * i + j] = (uint8_t)abs(best_d);
		}
	}

	times.push_back(GetCounter());
	return dispmap;
}





unsigned char* DownandGray(unsigned char* image, int w, int h) {
	StartCounter();
	uint8_t* img_down = (uint8_t*)malloc(w*h);
	int ii;
	int jj;

	for (int i = 0; i < h / 4; i++) {
		for (int j = 0; j < w / 4; j++) {
			ii = (4 * i - 1 * (i > 0));
			jj = (4 * j - 1 * (j > 0));

			img_down[i * (w / 4) + j] = 0.2126 * image[ii * (4 * w) + 4 * jj] + 0.7152 * image[ii * (4 * w) + 4 * jj + 1] + 0.0722 * image[ii * (4 * w) + 4 * jj + 2];
		}
	}

	times.push_back(GetCounter());
	return img_down;
}


unsigned Encode(unsigned char* image, int w, int h, char filename[], LodePNGColorType color) {
	unsigned error1 = lodepng_encode_file(filename, image, w, h, color, 8);
	if (error1) std::cout << "encoder error " << error1 << ": " << lodepng_error_text(error1) << std::endl;
	return error1;
}




unsigned char* CrossChecking(unsigned char* image1, unsigned char* image2, int w, int h, int thres) {
	StartCounter();
	uint8_t* depthmap = (uint8_t*)malloc(w*h);
	float diff;

	for (int i = 0; i < w*h; i++) {
		diff = abs((int) image1[i] - image2[i]);
		if (diff > thres) {
			depthmap[i] = 0;
		}
		else {
			depthmap[i] = image1[i];
		}
	}

	times.push_back(GetCounter());
	return depthmap;
}



unsigned char* OcclusionFill(unsigned char* depthmap, int w, int h, int n) {
	StartCounter();
	uint8_t* dmap = (uint8_t*)malloc(w*h);
	bool s;
	uint8_t max = 0;
	uint8_t min = UCHAR_MAX;

	// Occlusion filling
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {

			dmap[i * w + j] = depthmap[i * w + j];

			if (depthmap[i * w + j] == 0) {

				s = false;

				for (int a = 1; (a <= n / 2) && (!s); a++) {
					for (int jj = -a; (jj <= a) && (!s); jj++) {
						for (int ii = -a; (ii <= a) && (!s); ii++) {

							if (!(i + ii >= 0) || !(i + ii < h) || !(j + jj >= 0) || !(j + jj < w) || (ii == 0 && jj == 0)) {
								continue;
							}

							if (depthmap[(i + ii) * w + (j + jj)] != 0) {
								dmap[i * w + j] = depthmap[(i + ii) * w + (j + jj)];
								s = true;
								break;
							}
						}
					}
				}
			}
		}
	}

	// Normalization
	for (int i = 0; i < w*h; i++) {
		if (dmap[i] > max) {
			max = dmap[i];
		}
		if (dmap[i] < min) {
			min = dmap[i];
		}
	}
	for (int i = 0; i < w*h; i++) {
		dmap[i] = (uint8_t)(255 * (dmap[i] - min) / (max - min));
	}

	times.push_back(GetCounter());
	return dmap;
}




int main(int argc, char* argv[])
{
	std::cout << "C++ implementation running...\n";

	// Allocating for image data
	uint8_t* image1;
	uint8_t* image2;
	uint8_t* img_down1;
	uint8_t* img_down2;
	uint8_t* dispmap1;
	uint8_t* dispmap2;
	uint8_t* depthmap;
	uint8_t* depthmap2;
	uint32_t w;
	uint32_t h;


	// Load images im0 and im1
	std::cout << "Loading images...\n";
	lodepng_decode_file(&image1, &w, &h, "im0.png", LCT_RGBA, 8);
	lodepng_decode_file(&image2, &w, &h, "im1.png", LCT_RGBA, 8);
	std::cout << "Images loaded\n";


	// Images properties for processing
	int w1 = w / 4;
	int h1 = h / 4;
	int wsy = 21;
	int wsx = 15;
	int min_d = 0;
	int max_d = 64;
	int thres = 2;
	int neigh = 256;



	// Downscale to 1/16 and transform to grayscale
	std::cout << "Downscaling and Grayscaling...\n";
	img_down1 = DownandGray(image1, w, h);
	Encode(img_down1, w1, h1, "im2.png", LCT_GREY);
	img_down2 = DownandGray(image2, w, h);
	Encode(img_down2, w1, h1, "im3.png", LCT_GREY);
	std::cout << "Downscales and Grayscales done\n";
	std::cout << "GrayScaled and Downscaled images saved into files im2.png and im3.png\n";



	// Count ZNCC 
	std::cout << "Counting ZNCC...\n";
	dispmap1 = ZNCC(img_down1, img_down2, w1, h1, wsy, wsx, min_d, max_d);
	Encode(dispmap1, w1, h1, "im4.png", LCT_GREY);
	dispmap2 = ZNCC(img_down2, img_down1, w1, h1, wsy, wsx, -max_d, min_d);
	Encode(dispmap2, w1, h1, "im5.png", LCT_GREY);
	std::cout << "ZNCC counted\n";
	std::cout << "ZNCC images saved into file im4.png an im5.png\n";



	// Cross checking images
	depthmap = CrossChecking(dispmap1, dispmap2, w1, h1, thres);
	std::cout << "Cross checking done\n";
	Encode(depthmap, w1, h1, "im6.png", LCT_GREY);
	std::cout << "Cross Checked image saved into file im6.png\n";



	// Occlusion fill 
	depthmap2 = OcclusionFill(depthmap, w1, h1, neigh);
	std::cout << "Occlusion filling done\n";
	Encode(depthmap2, w1, h1, "im7.png", LCT_GREY);
	std::cout << "Post-prosessed image saved into file im7.png\n";



	std::cout << "C++ implementation done\n";
	std::cout << "Execution time of Downscaling and Grayscaling: ";
	std::cout << times.at(0)+times.at(1) << "\n";
	std::cout << "Execution time of ZNCC: ";
	std::cout << times.at(2)+times.at(3) << "\n";
	std::cout << "Execution time of Cross Checking: ";
	std::cout << times.at(4) << "\n";
	std::cout << "Execution time of Occlusion Filling: ";
	std::cout << times.at(5) << "\n";
	std::cout << "Execution time of Whole Process: ";
	std::cout << times.at(0)+times.at(1)+times.at(2)+ times.at(3)+times.at(4)+times.at(5) << "\n";

	return 0;
}