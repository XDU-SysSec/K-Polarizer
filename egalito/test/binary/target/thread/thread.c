#include <stdio.h>
#include <pthread.h>

pthread_barrier_t barrier;

void hello(void) {
    puts("will reach a barrier");
    pthread_barrier_wait(&barrier);
    puts("Hello, World!");
    fflush(stdout);
}

int main() {
    pthread_t   t1;
    int status;

    pthread_barrier_init(&barrier, NULL, 2);

    status = pthread_create(&t1, NULL, (void *)hello, NULL);
    hello();

    if(status == 0) {
        puts("joining");
        pthread_join(t1, NULL);
    }
    pthread_barrier_destroy(&barrier);

    return 0;
}
