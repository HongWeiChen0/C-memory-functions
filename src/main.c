#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    printf("%f\n", *ptr);

    sf_free(ptr);

    // void *x = sf_malloc(sizeof(double) * 8);
    // void *y = sf_realloc(x, sizeof(int));

    // sf_block *bp = (sf_block *)((char*)y - sizeof(sf_header));
    // printf("%p\n", bp);

    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
