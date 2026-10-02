#ifndef RECRAFT_CLIMATE_H
#define RECRAFT_CLIMATE_H
#include <stdint.h>
typedef struct BetaNoise2 { uint16_t permutation[512]; double x,z; } BetaNoise2;
typedef struct BetaClimate { BetaNoise2 temperature[4],humidity[4],detail[2]; } BetaClimate;
void beta_climate_init(BetaClimate *climate,uint64_t seed);
void beta_climate_sample(const BetaClimate *climate,int x,int z,double *temperature,double *humidity);
/* Precipitation: 0 dry desert, 1 rain, 2 snow (Beta lookup quantization). */
unsigned beta_climate_precipitation(double temperature,double humidity);
#endif
