#include <stdio.h>
// #include <unistd.h>
// #include <sys/stat.h>
// #include <sys/types.h>

int main(int argc, char *argv[]) {
    // struct stat s;
    // fstat(1 ,&s);
    printf("%s: Hello world!\n", argv[0]);
    return 0;
}