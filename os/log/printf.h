#ifndef PRINTF_H
#define PRINTF_H

void printf(char *, ...);

#define assert(x, str) if(!(x)) printf("!!!!!!!!!!!!!!!!!!! assertion failed: %s\n", str)

#endif // PRINTF_H
