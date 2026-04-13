#include <stdio.h>

int __attribute__((noinline)) fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void __attribute__((noinline)) dummy_work(int count) {
    volatile int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += i;
    }
}

void __attribute__((noinline)) heavy_logic() {
    for (int i = 0; i < 5; i++) {
	// It will take up to a few sec
        printf("Computing fib(30)... iteration %d\n", i);
        fibonacci(30);
    }
}

int main(int argc, char* argv[]) {
    printf("--- Profiling Test Start ---\n");

    heavy_logic();
    dummy_work(1000);

    printf("--- Profiling Test End ---\n");
    return 0;
}
