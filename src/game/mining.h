#ifndef RECRAFT_MINING_H
#define RECRAFT_MINING_H
#include "inventory.h"
#include "../world/beta_blocks.h"
float mining_hardness(unsigned block);
int mining_can_harvest(int item,unsigned block);
float mining_strength(int item,unsigned block,int underwater,int on_ground);
/* Return one Beta drop stack. Random draws use the caller's world RNG. */
InventorySlot mining_drop(BetaBlockState block,int item,uint32_t random);
int mining_wear(InventorySlot *item,unsigned block); /* 1 only if a tool broke. */
#endif
