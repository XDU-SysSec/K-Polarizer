#include <stdio.h>

long leafFunc(long a, long b, long c)
{
    long first = a + 8;
    long second = b + 9;
    long third = c + 15;
    long sum = first + second + third;

    return first * second * third + sum;
}

long nonLeafFunc(long a1, long a2)
{
    long first = a1 * a2;
    long second = a2 + a2;
    long third = leafFunc(first, second, first % second);
    printf("[redzone_output] %ld\n", third ^ 0x8f);
    return third + 20;
}

int main(void) {
    nonLeafFunc(10, 20);
}
