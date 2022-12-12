#ifndef TYPES_H
#define TYPES_H

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;


typedef char  int8;
typedef short int16;
typedef int   int32;
typedef long  int64;

// typedef long unsigned int ssize_t;
// typedef long unsigned int size_t;
// typedef int pid_t;
#include <sys/types.h> // ssize_t, size_t, pid_t

#endif // TYPES_H