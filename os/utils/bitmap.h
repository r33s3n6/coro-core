#ifndef UTILS_BITMAP_H
#define UTILS_BITMAP_H
#include <ccore/types.h>

// size: number of elements in uint64[]
inline static uint32 bitmap_find_first_zero(const uint64 *bitmap, uint32 size) {
    uint32 i;
    for (i = 0; i < size; i++) {
        if (bitmap[i] != ~(0uL)) 
            break;
    }
    if (i == size)
        return -1;

    uint32 j;
    for (j = 0; j < 64; j++) {
        if (!(bitmap[i] & (1uL << j)))
            break;
    }
    return i * 64 + j;
}

inline static void bitmap_set(uint64 *bitmap, uint32 index) {
    bitmap[index / 64] |= (1uL << (index % 64));
}

inline static void bitmap_clear(uint64 *bitmap, uint32 index) {
    bitmap[index / 64] &= ~(1uL << (index % 64));
}

inline static bool bitmap_get(const uint64 *bitmap, uint32 index) {
    return !!(bitmap[index / 64] & (1uL << (index % 64)));
}


#endif