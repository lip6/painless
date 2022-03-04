#include "slime/utils/Mersenne.h"

namespace SLIME {

	void mersenne_init_with_seed(mersenne_randgen* randgen, int seed){
		unsigned int s = ((unsigned int) (seed << 1)) + 1;
		randgen->mt[0] = s & 0xffffffffUL;
		for(randgen->mti = 1; randgen->mti < Mersenne_N; randgen->mti++) {
			randgen->mt[randgen->mti] = (1812433253UL * (randgen->mt[randgen->mti - 1] ^ (randgen->mt[randgen->mti - 1] >> 30)) + randgen->mti);
			randgen->mt[randgen->mti] &= 0xffffffffUL;
		}
	}

	int mersenne_next31(mersenne_randgen* randgen){
		unsigned int y;
		static unsigned int mag01[2] = {0x0UL, Mersenne_MATRIX_A};
		if(randgen->mti >= Mersenne_N) {
			int kk;
			for(kk = 0; kk < Mersenne_N - Mersenne_M; kk++) {
			y = (randgen->mt[kk] & Mersenne_UPPER_MASK) | (randgen->mt[kk + 1] & Mersenne_LOWER_MASK);
			randgen->mt[kk] = randgen->mt[kk + Mersenne_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
			}
			for(; kk < Mersenne_N - 1; kk++) {
			y = (randgen->mt[kk] & Mersenne_UPPER_MASK) | (randgen->mt[kk + 1] & Mersenne_LOWER_MASK);
			randgen->mt[kk] = randgen->mt[kk + (Mersenne_M - Mersenne_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
			}
			y = (randgen->mt[Mersenne_N - 1] & Mersenne_UPPER_MASK) | (randgen->mt[0] & Mersenne_LOWER_MASK);
			randgen->mt[Mersenne_N - 1] = randgen->mt[Mersenne_M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
			randgen->mti = 0;
		}
		y = randgen->mt[randgen->mti++];
		y ^= (y >> 11);
		y ^= (y << 7) & 0x9d2c5680UL;
		y ^= (y << 15) & 0xefc60000UL;
		y ^= (y >> 18);
		return  (int) (y>>1);
	}

	int mersenne_next(mersenne_randgen* randgen, int bound){
		unsigned int value;
		do {
			value = mersenne_next31(randgen);
		} while(value + (unsigned int) bound >= 0x80000000UL);
		return (int) (value % bound);
	}
}