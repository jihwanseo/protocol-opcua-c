#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHAR_SIZE   128

#define COLOR_GREEN        "\x1b[32m"
#define COLOR_PURPLE      "\x1b[35m"
#define COLOR_RESET         "\x1b[0m"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                  METHOD CALLBACK FUNCTIONS                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 *  No Argument method
 **/
void test_method_shutdown(int inpSize, void **input, int outSize, void **output)
{
    printf(COLOR_GREEN "\n[shutdown() method called]\n" COLOR_RESET);
}

void test_method_print(int inpSize, void **input, int outSize, void **output)
{
    int *inp = (int*) input[0];
    printf(COLOR_GREEN "\n[print() method called]" COLOR_RESET);
    printf(COLOR_PURPLE " %d\n" COLOR_RESET, *inp );
}

void test_method_version(int inpSize, void **input, int outSize, void **output)
{
    char *version = (char*) malloc(sizeof(char) * CHAR_SIZE);
    strcpy(version, "09131759");
    output[0] = version;
    printf(COLOR_GREEN "\n[version() method called] :: %s\n" COLOR_RESET, version);
}

void test_method_sqrt(int inpSize, void **input, int outSize, void **output)
{
    double *inp = (double *) input[0];
    double *sq_root = (double *) malloc(sizeof(double));
    *sq_root = sqrt(*inp);
    output[0] = (void *) sq_root;
    printf(COLOR_GREEN "\n[sqrt(%.2f) method called] :: %.2f\n" COLOR_RESET, *inp, *sq_root);
}

void test_method_increment_int32Array(int inpSize, void **input, int outSize, void **output)
{
    int32_t *inputArray = (int32_t *) input[0];
    int *delta = (int *) input[1];

    printf(COLOR_GREEN "\n[incrementInt32Array({%d %d %d %d %d}, %d) method called] :: ",
            inputArray[0], inputArray[1], inputArray[2], inputArray[3], inputArray[4], *delta);

    int32_t *outputArray = (int32_t *) malloc(sizeof(int32_t) * 5);
    for (int i = 0; i < 5; i++)
    {
        outputArray[i] = inputArray[i] + *delta;
        printf("%d ", outputArray[i]);
    }

    printf("\n"COLOR_RESET);
    output[0] = (void *) outputArray;
}
