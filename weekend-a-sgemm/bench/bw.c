#include <stdio.h>
#include<time.h>
#include<limits.h>
#include<stdlib.h>

#define CACHE_EXHAUST (1UL << 24) // 64MiB floats

long get_time(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec; 
}

int main (void){
    float *a  = malloc(CACHE_EXHAUST * sizeof(float));
    float *b = malloc(CACHE_EXHAUST * sizeof(float));
    if (!a || !b){
        free(a); free(b);
        return 1;
    }
    for (size_t i = 0; i < CACHE_EXHAUST; i++) a[i] = (float)i;
    for (size_t i = 0; i < CACHE_EXHAUST; i++) b[i] = 1.1;
    long fastest = LONG_MAX;
    
    for (int i = 0; i < 10; i++){
        long now = get_time();
        for (size_t j = 0; j < CACHE_EXHAUST; j++){
            b[j] = a[j];
        }
        long elapsed = get_time() - now;
        if (elapsed < fastest) {
            fastest = elapsed;
        } 
    }
    printf("bandwidth is %.1f GB/s\n", CACHE_EXHAUST * 4 * 2 / fastest);
    printf("%f\n", b[0]);    
    free(a); free(b);
    return 0;
}