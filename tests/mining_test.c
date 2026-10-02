#include "game/mining.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void)
{
    InventorySlot tool={278,1,1561},drop;
    float s=mining_strength(270,1,0,1);
    assert(fabsf(s-2.0f/1.5f/30)<0.000001f);
    assert(mining_strength(285,1,0,1)>mining_strength(278,1,0,1));
    assert(mining_strength(271,17,0,1)>mining_strength(270,17,0,1));
    assert(mining_strength(269,3,0,1)>mining_strength(271,3,0,1));
    assert(fabsf(mining_strength(270,1,1,0)-s/25)<0.000001f);
    assert(!mining_can_harvest(285,56) && mining_can_harvest(257,56));
    assert(!mining_can_harvest(257,49) && mining_can_harvest(278,49));
    assert(mining_strength(278,7,0,1)==0);
    assert(mining_strength(0,50,0,1)==1);
    drop=mining_drop((BetaBlockState){1,0},270,1); assert(drop.id==4 && drop.count==1);
    drop=mining_drop((BetaBlockState){1,0},0,1); assert(drop.count==0);
    drop=mining_drop((BetaBlockState){17,2},0,1); assert(drop.id==17 && drop.damage==2);
    drop=mining_drop((BetaBlockState){20,0},278,1); assert(drop.count==0);
    mining_wear(&tool,1); assert(tool.count==0);
    tool=(InventorySlot){359,1,0};
    mining_wear(&tool,35); assert(tool.damage==0);
    mining_wear(&tool,18); mining_wear(&tool,30); assert(tool.damage==2);
    puts("Beta mining speed, harvest, drops and durability passed");
    return 0;
}
