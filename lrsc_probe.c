// lrsc_probe.c - C906 reservation set sniffer
// riscv64-linux-gnu-gcc -pthread -static -o lrsc_probe lrsc_probe.c

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

#define RUNS 10000000
#define CACHELINE 64

typedef struct {
    atomic_int target;
    char pad1[28];
    volatile int victim;  // +32 bytes from target
    char pad2[28];
} __attribute__((aligned(CACHELINE))) probe_t;

probe_t data = {0};
volatile int go = 0, stop = 0;
atomic_ulong fails = 0, stores = 0;

void* hammer_lrsc(void* _) {
    unsigned long f = 0;
    while (!go);
    
    for (int i = 0; i < RUNS && !stop; i++) {
        int old, new, ok;
        do {
            __asm__ volatile("lr.w %0, (%1)" : "=r"(old) : "r"(&data.target) : "memory");
            new = old + 1;
            __asm__ volatile("sc.w %0, %2, (%1)" : "=r"(ok) : "r"(&data.target), "r"(new) : "memory");
            if (ok) f++;
        } while (ok);
    }
    atomic_fetch_add(&fails, f);
    return NULL;
}

void* hammer_store(void* _) {
    unsigned long s = 0;
    while (!go);
    
    for (int i = 0; i < RUNS && !stop; i++) {
        data.victim = i;
        s++;
    }
    atomic_fetch_add(&stores, s);
    return NULL;
}

int main() {
    pthread_t t1, t2;
    
    printf("target: %p\n", &data.target);
    printf("victim: %p (+%ld bytes)\n", &data.victim, (char*)&data.victim - (char*)&data.target);
    
    pthread_create(&t1, NULL, hammer_lrsc, NULL);
    pthread_create(&t2, NULL, hammer_store, NULL);
    
    sleep(1);
    go = 1;
    
    pthread_join(t1, NULL);
    stop = 1;
    pthread_join(t2, NULL);
    
    unsigned long f = atomic_load(&fails);
    unsigned long s = atomic_load(&stores);
    
    printf("\nSC fails: %lu / %d (%.4f%%)\n", f, RUNS, 100.0 * f / RUNS);
    printf("stores:   %lu\n", s);
    printf("final:    %d\n\n", atomic_load(&data.target));
    
    if (f < RUNS * 0.01) 
        printf("→ Word-sized reservation (~4B)\n");
    else if (f > RUNS * 0.10)
        printf("→ Cache-line reservation (~64B)\n");
    else
        printf("→ Inconclusive\n");
    
    return 0;
}