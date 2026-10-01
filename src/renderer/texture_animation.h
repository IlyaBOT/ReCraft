#ifndef RECRAFT_TEXTURE_ANIMATION_H
#define RECRAFT_TEXTURE_ANIMATION_H
#include <stdint.h>
typedef struct TextureAnimation {
    float water[4][256],lava[4][256];
    uint8_t portal[32][1024],water_pixels[1024],lava_pixels[1024];
    uint32_t random;
} TextureAnimation;
void texture_animation_init(TextureAnimation *fx);
void texture_animation_step(TextureAnimation *fx);
#endif
