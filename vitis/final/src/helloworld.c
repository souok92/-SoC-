#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xtime_l.h"
#include <stdlib.h>
#include "xil_io.h"

// BRAM memory map
#define BRAM_BASEADDR   0xA0000000
#define MATRIX_A_OFFSET 0x00000000
#define MATRIX_B_OFFSET 0x00000400
#define RESULT_OFFSET   0x00000800
#define CTRL_OFFSET     0x00000C00
#define STATUS_OFFSET   0x00000C08
#define CYCLE_OFFSET    0x00000D00

#define MATRIX_SIZE 16
#define TOTAL_SIZE (MATRIX_SIZE * MATRIX_SIZE)

// Time measurement variables
XTime tStart, tEnd;
double elapsedTime;

// Matrix arrays
int matrixA[TOTAL_SIZE];
int matrixB[TOTAL_SIZE];
int matrixC[TOTAL_SIZE];  // Result matrix for SW
int matrixD[TOTAL_SIZE];  // Result matrix for HW

void generateRandomMatrix(int* matrix, int size) {
    for(int i = 0; i < size * size; i++) {
        matrix[i] = rand() % 30;  // Random values 0-30
    }
}

void matrix_multiply_sw(int* A, int* B, int* C, int size) {
    XTime_GetTime(&tStart);

    // matrix multiplication
    for(int i = 0; i < size; i++) {
        for(int j = 0; j < size; j++) {
            int sum = 0;
            for(int k = 0; k < size; k++) {
                sum += A[i*size + k] * B[k*size + j];
            }
            C[i*size + j] = sum;
        }
    }

    XTime_GetTime(&tEnd);
    double executionTime = 1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000);
    printf("SW Matrix Multiplication took %.2f us\n\n", executionTime);
}

void write_matrices_to_bram() {
	//for printing
    int cnt = 0;

    // Write and verify each element of Matrix A
    printf("\nWriting and verifying Matrix A...\n\n");
    for(int i = 0; i < TOTAL_SIZE; i++) {
        u32 addr = BRAM_BASEADDR + MATRIX_A_OFFSET + (i*4);
        Xil_Out32(addr, matrixA[i]);
        int check = Xil_In32(addr);
        printf(" %d", check);
        cnt += 1;
        if (cnt >= MATRIX_SIZE){
        	printf(";\n");
        	cnt = 0;
        }
    }

    // Write and verify each element of Matrix B
    printf("\nWriting and verifying Matrix B...\n\n");
    for(int i = 0; i < TOTAL_SIZE; i++) {
        u32 addr = BRAM_BASEADDR + MATRIX_B_OFFSET + (i*4);
        Xil_Out32(addr, matrixB[i]);
        int check = Xil_In32(addr);
        printf(" %d", check);
        cnt += 1;
        if (cnt >= MATRIX_SIZE){
        	printf(";\n");
        	cnt = 0;
        }
    }

    XTime_GetTime(&tStart);

    // Write each element of Matrix A
    for(int i = 0; i < TOTAL_SIZE; i++) {
    	u32 addr = BRAM_BASEADDR + MATRIX_A_OFFSET + (i*4);
    	Xil_Out32(addr, matrixA[i]);
    };

    // Write each element of Matrix B
	for(int i = 0; i < TOTAL_SIZE; i++) {
		u32 addr = BRAM_BASEADDR + MATRIX_A_OFFSET + (i*4);
		Xil_Out32(addr, matrixA[i]);
	};

    XTime_GetTime(&tEnd);

    printf("\nWriting matrices to BRAM took %.2f us\n\n",
           1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000));
}

void read_result_from_bram() {
	// for printing
	int cnt = 0;
    for(int i = 0; i < TOTAL_SIZE; i++) {
        matrixD[i] = Xil_In32(BRAM_BASEADDR + RESULT_OFFSET + (i*4));
        printf(" %d", matrixD[i]);
        cnt += 1;
        if (cnt >= MATRIX_SIZE){
        	printf(";\n");
        	cnt = 0;
        }
    }

    XTime_GetTime(&tStart);

    for(int i = 0; i < TOTAL_SIZE; i++) {
    	matrixD[i] = Xil_In32(BRAM_BASEADDR + RESULT_OFFSET + (i*4));
    };
    XTime_GetTime(&tEnd);

    printf("\nReading result from BRAM took %.2f us\n\n",
           1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000));
}

void matrix_multiply_hw() {
	// clear
	Xil_Out32(BRAM_BASEADDR + STATUS_OFFSET, 0x00000000);
    Xil_Out32(BRAM_BASEADDR + CTRL_OFFSET,   0x00000000);

    // Start HW multiplication
    XTime_GetTime(&tStart);

    // Write start signal
    Xil_Out32(BRAM_BASEADDR + CTRL_OFFSET, 0x00000001);

    // Wait for done signal
    while(Xil_In32(BRAM_BASEADDR + STATUS_OFFSET) != 0x00000001);

    XTime_GetTime(&tEnd);

    // Reset signal
    Xil_Out32(BRAM_BASEADDR + STATUS_OFFSET, 0x00000000);

    // Print results
    printf("CYCLE : %d\n", Xil_In32(BRAM_BASEADDR + CYCLE_OFFSET));

    printf("HW Matrix Multiplication took %.2f us\n\n",
           1.0 * (tEnd - tStart) / (COUNTS_PER_SECOND/1000000));
}

void compare_results() {
    int mismatch = 0;
    printf("\nComparing PS and PL results:\n\n");

    for(int i = 0; i < TOTAL_SIZE; i++) {
        if(matrixC[i] != matrixD[i]) {
            mismatch = 1;
            printf("Mismatch at index %d: PS=%d, PL=%d\n\n",
                   i, matrixC[i], matrixD[i]);
        }
    }

    if(mismatch == 0) {
        printf("Results match! PS and PL calculations are identical.\n\n");
    }
}

int main()
{
    init_platform();

    char cmd;
    printf("\nMatrix Multiplication Test\n");
    printf("Commands:\r\n");
    printf("1: Enter seed and generate matrices\n");
    printf("2: Run PS (ARM) multiplication\n");
    printf("3: Run PL (FPGA) multiplication\n");
    printf("4: Compare PS and PL results\n");
    printf("5: EXIT\n");

    while(1) {
    	cmd = getchar();

        switch(cmd) {
            case '1':
                generateRandomMatrix(matrixA, MATRIX_SIZE);
                generateRandomMatrix(matrixB, MATRIX_SIZE);
                printf("\nMatrices generated random\n\n");
                break;

            case '2':
                printf("\nRunning PS multiplication...\n\n");
                matrix_multiply_sw(matrixA, matrixB, matrixC, MATRIX_SIZE);
                break;

            case '3':
                printf("\nRunning PL multiplication...\n\n");
                write_matrices_to_bram();
                matrix_multiply_hw();
                read_result_from_bram();
                break;

            case '4':
                compare_results();
                break;

            case '5':
            	printf("\nEXIT\n");
            	return 0;

            default:
                if(cmd != '\n' && cmd != '\r') {
                    printf("\nInvalid command\n\n");
                }
                break;
        }
    }

    cleanup_platform();
    return 0;
}
