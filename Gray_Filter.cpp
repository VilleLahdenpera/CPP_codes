

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
#include <windows.h>


#define MAX_SOURCE_SIZE (0x050000)

using namespace std;
#include <chrono>
using namespace std::chrono;
double PCFreq = 0.0;
__int64 CounterStart = 0;
cl_image_format format = { CL_RGBA, CL_UNSIGNED_INT8 };
cl_image_desc desc;





void StartCounter() {
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		cout << "QueryPerformanceFrequency failed!\n";

	PCFreq = double(li.QuadPart) / 1000.0;

	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}

double GetCounter() {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}


int main(int argc, char* argv[])
{
	

	std::cout << "C++ implementation running...\n";


	// Load code from .cl file
	FILE* fp;
	char fileName[] = "Gray_Filter.cl";
	char* source_str;
	size_t source_size;
	fopen_s(&fp, fileName, "r");
	if (!fp) {
		fprintf(stderr, "Failed to load kernel.\n");
		exit(1);
	}
	source_str = (char*)calloc(MAX_SOURCE_SIZE, 1);
	source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);




	uint8_t* image1;
	uint32_t w;
	uint32_t h;
	
	// Load image im0 
	cout << "Loading image\n";
	StartCounter();
	lodepng_decode_file(&image1, &w, &h, "im0.png", LCT_RGBA, 8);
	double loading_time = GetCounter();


	uint8_t* image2 = (uint8_t*)malloc(w*h);

	// Device management
	size_t globalSize, localSize;
	cl_int err;
	cl_uint ret_num_platforms;
	cl_uint global_mem_size_ret;
	cl_uint local_mem_size_ret;
	cl_device_local_mem_type local_mem_type_ret;
	cl_uint max_compute_units;
	cl_uint max_clock_frequency;
	cl_uint max_constant_buffer_size;
	cl_uint max_work_group_size;
	cl_uint max_work_item_size;
	cl_platform_id cpPlatform;        // OpenCL platform
	cl_device_id device_id;           // device ID
	cl_context context;               // context
	cl_command_queue queue;           // command queue
	cl_program program;               // program
	cl_kernel kernel;                 // kernel





	// Size of image
	uint32_t im_size = w * h;

	// Number items in local group
	const size_t localWorkSize = 16;      // Local work size
	const size_t globalWorkSize = im_size;   // Global work size

		// Bind to platform
	err = clGetPlatformIDs(1, &cpPlatform, &ret_num_platforms);
	printf("Platform err: %d\n", err);

	// Get ID for the device
	err = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
	printf("Platform id: %d, device id: %d\n\n", cpPlatform, device_id);

	// Get device info
	err = clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, NULL, NULL, &global_mem_size_ret);
	err = clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, NULL, NULL, &local_mem_size_ret);
	err = clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_TYPE, NULL, NULL, &local_mem_type_ret);
	err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, NULL, NULL, &max_compute_units);
	err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_CLOCK_FREQUENCY, NULL, NULL, &max_clock_frequency);
	err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, NULL, NULL, &max_constant_buffer_size);
	err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, NULL, NULL, &max_work_group_size);
	err = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, NULL, NULL, &max_work_item_size);
	std::cout << "___Device info___ " << "\n";
	std::cout << "Memory size: " << local_mem_size_ret << "\n";
	std::cout << "Memory type: " << local_mem_type_ret << "\n";
	std::cout << "Max compute units: " << max_compute_units << "\n";
	std::cout << "Max clock frequency: " << max_clock_frequency << "\n";
	std::cout << "Max constant buffer size: " << max_constant_buffer_size << "\n";
	std::cout << "Max work group size: " << max_work_group_size << "\n";
	std::cout << "Max work item size: " << max_work_item_size << "\n";
	std::cout << "\n";

	// Create a context 
	context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);

	// Create a command queue
	queue = clCreateCommandQueue(context, device_id, 0, &err);

	// Create the compute program 
	program = clCreateProgramWithSource(context, 1, (const char**)&source_str, (const size_t*)&source_size, &err);

	// Build the program executable
	err = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

	if (err == CL_BUILD_PROGRAM_FAILURE) {
		// Determine the size of the log
		size_t log_size;
		clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL,
			&log_size);
		// Allocate memory for the log
		char* log = (char*)malloc(log_size);
		// Get the log
		clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log,
			NULL);
		// Print the log
		printf("%s\n", log);
	}


	// Create the compute kernel 
	kernel = clCreateKernel(program, "grayscale_filter", &err);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Fail to create kernel!\n");
		abort();
	}
	std::cout << "Kernel created\n";


	// Create the input and output
	cl_mem clmemImage = clCreateBuffer(context, CL_MEM_READ_ONLY, im_size, 0, &err);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Fail to create buffer for the image1!\n");
		abort();
	}
	cl_mem clmemImage2 = clCreateBuffer(context, CL_MEM_READ_ONLY, im_size, 0, &err);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Fail to create buffer for the image2!\n");
		abort();
	}
	

	// Create 2D image
	desc.image_type = CL_MEM_OBJECT_IMAGE2D;
	desc.image_width = w;
	desc.image_height = h;
	desc.image_depth = 8;
	desc.image_row_pitch = w * 4;
	cl_mem clmemOrigImage = clCreateImage2D(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, &format, desc.image_width, desc.image_height, desc.image_row_pitch, image1, &err);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Fail to create Image!\n");
		abort();
	}


	// Set the arguments to kernel
	err = clSetKernelArg(kernel, 0, sizeof(clmemOrigImage), &clmemOrigImage);
	err |= clSetKernelArg(kernel, 1, sizeof(clmemImage), &clmemImage);
	err |= clSetKernelArg(kernel, 2, sizeof(w), &w);
	err |= clSetKernelArg(kernel, 3, sizeof(h), &h);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Failed to set kernel arguments!\n");
		abort();
	}

	
	// Execute the kernel over the entire range of the data set 
	cout << "Starting kernel...\n";
	StartCounter();
	err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, (const size_t*)&globalWorkSize, (const size_t*)&localWorkSize, 0, NULL, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Failed to execute kernel on the device!\n");
		abort();
	}
	std::cout << "Kernel running...\n";


	// Wait for the command queue to get serviced before reading back results
	clFinish(queue);


	// Read the results from the device
	err = clEnqueueReadBuffer(queue, clmemImage, CL_TRUE, 0, im_size, image2, 0, NULL, NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Failed to send the data to host!\n");
		abort();
	}
	std::cout << "Image filtering done\n";
	double filtering_time = GetCounter();



	//Encode the image
	cout << "Saving image\n";
	StartCounter();
	unsigned error1 = lodepng_encode_file("im1.png", image2, w, h, LCT_GREY, 8);
	if (error1) std::cout << "encoder error " << error1 << ": " << lodepng_error_text(error1) << std::endl;
	std::cout << "Image saved into file im1.png\n";
	double saving_time = GetCounter();


	// Release OpenCL resources
	clReleaseMemObject(clmemImage2);
	clReleaseMemObject(clmemImage);
	clReleaseMemObject(clmemOrigImage);
	clReleaseProgram(program);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);


	cout << "\nExecution time of image loading: ";
	cout << loading_time << "\n";
	cout << "\nExecution time of filtering: ";
	cout << filtering_time << "\n";
	cout << "\nExecution time of image saving: ";
	cout << saving_time << "\n";
	cout << "\nExecution time of whole process: ";
	cout << saving_time + loading_time + filtering_time << "\n";

	return 0;
}