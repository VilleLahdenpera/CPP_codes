#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <cmath>


int Matrxi_Mul(float * A, float * B, float *Result, int rows_of_A, int columns_of_A ,int rows_of_B, int columns_of_B, bool Fix_Or_Float16);
float HALFToFloat(uint16_t);
uint16_t floatToHALF(float);
static uint32_t halfToFloatI(uint16_t);
static uint16_t floatToHalfI(uint32_t);



float
HALFToFloat(uint16_t y)
{
    union { float f; uint32_t i; } v;
    v.i = halfToFloatI(y);
    return v.f;
}

uint32_t
static halfToFloatI(uint16_t y)
{
    int s = (y >> 15) & 0x00000001;                           
    int e = (y >> 10) & 0x0000001f;                            
    int f =  y        & 0x000003ff;                            


    if (e == 0) {
        if (f == 0)                                           
            return s << 31;
        else {                                                 
            while (!(f & 0x00000400)) {
                f <<= 1;
                e -=  1;
            }
            e += 1;
            f &= ~0x00000400;
        }
    } else if (e == 31) {
        if (f == 0)                                             
            return (s << 31) | 0x7f800000;
        else                                                    
            return (s << 31) | 0x7f800000 | (f << 13);
    }

    e = e + (127 - 15);
    f = f << 13;

    return ((s << 31) | (e << 23) | f);
}

uint16_t
floatToHALF(float i)
{
    union { float f; uint32_t i; } v;
    v.f = i;
    return floatToHalfI(v.i);
}

uint16_t
static floatToHalfI(uint32_t i)
{
    register int s =  (i >> 16) & 0x00008000;                   
    register int e = ((i >> 23) & 0x000000ff) - (127 - 15);     
    register int f =   i        & 0x007fffff;                   


    if (e <= 0) {
        if (e < -10) {
            if (s)                                             
               return 0x8000;
            else
               return 0;
        }
        f = (f | 0x00800000) >> (1 - e);
        return s | (f >> 13);
    } else if (e == 0xff - (127 - 15)) {
        if (f == 0)                                            
            return s | 0x7c00;
        else {                                                  
            f >>= 13;
            return s | 0x7c00 | f | (f == 0);
        }
    } else {
        if (e > 30)                                             
            return s | 0x7c00;
        return s | (e << 10) | (f >> 13);
    }
}


int main (int argc, char *argv[])  {
	bool Fix_Or_Float16 = true;
	//Allocate matrixes
	int A_rows = 2;
	int A_columns = 3;
	int A_value = 10;
	int B_rows = 3;
	int B_columns = 2;	
	int B_value = 4;
	float* A = (float*)malloc(A_rows * A_columns * sizeof(int));
    float* B = (float*)malloc(B_rows * B_columns * sizeof(int));
    float* Result = (float*)malloc(A_rows * A_rows * sizeof(int));
	//Fill with numbers
	for (int i = 0; i < A_rows; i++) {
		 for (int j = 0; j < A_columns; j++) {
			*(A + i * A_columns + j) = A_value;
        }
	}
	for (int i = 0; i < B_rows; i++) {
		 for (int j = 0; j < B_columns; j++) {
			*(B + i * B_columns + j) = B_value;
        }
	}

	//Do multiplication 
    Matrxi_Mul(A, B, Result, A_rows, A_columns, B_rows, B_columns, Fix_Or_Float16);
}



int Matrxi_Mul(float* A, float* B, float* Result, int rows_of_A, int columns_of_A, int rows_of_B, int columns_of_B, bool Fix_Or_Float16) {
	// Allocate fixed matrixes
	int32_t* A_Fixed = (int*)malloc(rows_of_A * columns_of_A * sizeof(int));
	int32_t* B_Fixed = (int*)malloc(rows_of_B * columns_of_B * sizeof(int));
	//float* A_B_Fixed = (float*)malloc(rows_of_B * columns_of_B * sizeof(int));	
	uint16_t* A_Half = (uint16_t*)malloc(rows_of_A*columns_of_A*sizeof(int));
	uint16_t* B_Half = (uint16_t*)malloc(rows_of_B*columns_of_B*sizeof(int));
	//uint16_t* A_B_Half = (uint16_t*)malloc(rows_of_B*columns_of_B*sizeof(int));
	if (A_Fixed == nullptr || B_Fixed == nullptr || A_Half == nullptr || B_Half == nullptr) {
		printf("Multiplication can not be done due to matrix size!");
		return -1;
	}

	if (Fix_Or_Float16 == true) {
		
		//FIXED FORMAT

	    //Change to fixed point format
		int scaling = pow(2,8);
		printf("A: \n");
		for (int i = 0; i < rows_of_A; i++) {
	     	for (int j = 0; j < columns_of_A; j++) {
			    printf("%d\t",*(A_Fixed + i * columns_of_A + j) = *(A + i * columns_of_A + j) * scaling);
    		}
			printf("\n");
	    }
		printf("B: \n");
		for (int i = 0; i < rows_of_B; i++) {
	     	for (int j = 0; j < columns_of_B; j++) {
	        	printf("%d\t",*(B_Fixed + i * columns_of_B + j) = *(B + i * columns_of_B + j) * scaling);
    		}
			printf("\n");
	    }
	     

		 // Multiply fixed matrixes
		 printf("Result: \n");
		 for (int i = 0; i < rows_of_A; i++) {
			for (int j = 0; j < columns_of_B; j++) {
				for (int k = 0; k < columns_of_A; k++) {
					*(Result+columns_of_B*j+i) += (*(A_Fixed+columns_of_A*i+k) * *(B_Fixed + columns_of_B*k+j) / scaling);
	            }
				printf("%f\t", *(Result+j*columns_of_B+i));
			}
			printf("\n");
		}


	   } else {

			//HALF FORMAT

			//Change to Float16 format
			for (int i = 0; i < rows_of_A; i++) {
				for (int j = 0; j < columns_of_A; j++) {
					printf("%d\t",*(A_Half + i * columns_of_A + j) = floatToHALF(*(A + i * columns_of_A + j)));
	        	}
	        	printf("\n");
			}	
			for (int i = 0; i < rows_of_B; i++) {
				for (int j = 0; j < columns_of_B; j++) {
					printf("%d\t",*(B_Half + i * columns_of_B + j) = floatToHALF(*(B + i * columns_of_B + j)));
	        	}
	        	printf("\n");
			}

			//Multiply and format back to Float32
			for (int i = 0; i < rows_of_A; i++) {
				for (int j = 0; j < columns_of_B; j++) {
					for (int k = 0; k < columns_of_A; k++) {
					    *(Result + columns_of_B * j + i) += HALFToFloat(*(A_Half + columns_of_A * i + k)) * HALFToFloat(*(B_Half + columns_of_B * k + j));
	            	}
					printf("%f\t", *(Result+j*columns_of_B+i));
				}
				printf("\n");
			}	
		}
	return 0;
}






        
