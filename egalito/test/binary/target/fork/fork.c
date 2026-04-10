#include <stdio.h>
#include <unistd.h>

int main() {
    int stat = fork();
    if(stat == 0) {
        puts("[child] Hello, World!");
    }
    else {
        puts("[parent] Hello, World!");
    }
    return 0;
}
