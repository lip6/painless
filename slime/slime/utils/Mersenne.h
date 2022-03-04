#pragma once

#define Mersenne_N 624
#define Mersenne_M 397
#define Mersenne_MATRIX_A 0x9908b0dfUL
#define Mersenne_UPPER_MASK 0x80000000UL
#define Mersenne_LOWER_MASK 0x7fffffffUL


namespace SLIME
{
    // mersenne twist
    typedef struct mersenne_randgen mersenne_randgen;
    struct mersenne_randgen
    {
    	unsigned mt[624];
    	int 	 mti;
    };

    void    mersenne_init_with_seed(mersenne_randgen* randgen, int seed);
    int     mersenne_next31(mersenne_randgen* randgen);
    int     mersenne_next(mersenne_randgen* randgen, int bound);
} // namespace SLIME

