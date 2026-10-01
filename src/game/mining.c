#include "mining.h"
typedef struct MiningTool { int id; uint8_t speed2[BETA_BLOCK_COUNT]; uint32_t harvest[4]; } MiningTool;
static const MiningTool tools[]={
#include "beta_mining.def"
};
static const float hardness[BETA_BLOCK_COUNT]={
#include "beta_hardness.def"
};
static const MiningTool *tool(int id)
{
    unsigned i;
    for (i=1;i<sizeof(tools)/sizeof(tools[0]);++i) if (tools[i].id==id) return &tools[i];
    return &tools[0];
}
float mining_hardness(unsigned block) { return block<BETA_BLOCK_COUNT ? hardness[block] : -1; }
int mining_can_harvest(int item,unsigned block)
{ return block<BETA_BLOCK_COUNT && ((tool(item)->harvest[block/32]>>(block%32))&1u); }
float mining_strength(int item,unsigned block,int underwater,int on_ground)
{
    float h=mining_hardness(block),speed;
    if (h<0 || block==0) return 0;
    if (h==0) return 1;
    if (!mining_can_harvest(item,block)) return 1.0f/h/100.0f;
    speed=tool(item)->speed2[block]*0.5f;
    if (underwater) speed/=5;
    if (!on_ground) speed/=5;
    return speed/h/30.0f;
}
InventorySlot mining_drop(BetaBlockState b,int item,uint32_t random)
{
    InventorySlot d={b.id,1,0};
    if (!mining_can_harvest(item,b.id)) { inventory_clear_slot(&d); return d; }
    switch (b.id) {
    case 1: d.id=4; break;
    case 2: case 60: d.id=3; break;
    case 7: case 8: case 9: case 10: case 11: case 20: case 51: case 52:
    case 79: case 90: d.id=-1; d.count=0; break;
    case 13: if (random%10==0) d.id=318; break;
    case 16: d.id=263; break;
    case 17: d.damage=b.metadata&3; break;
    case 18:
        if (item==359) d.damage=b.metadata&3;
        else if (random%20==0) { d.id=6; d.damage=b.metadata&3; }
        else { d.id=-1; d.count=0; }
        break;
    case 21: d.id=351; d.damage=4; d.count=4+(int)(random%5); break;
    case 26: d.id=355; break;
    case 30: d.id=287; break;
    case 31: if (random%8==0) d.id=295; else { d.id=-1; d.count=0; } break;
    case 35: d.damage=b.metadata; break;
    case 43: d.id=44; d.count=2; d.damage=b.metadata&3; break;
    case 44: d.damage=b.metadata&3; break;
    case 55: d.id=331; break;
    case 56: d.id=264; break;
    case 59: d.id=b.metadata==7 ? 296 : 295; break;
    case 62: d.id=61; break;
    case 63: case 68: d.id=323; break;
    case 64: d.id=324; if (b.metadata&8) d.count=0; break;
    case 71: d.id=330; if (b.metadata&8) d.count=0; break;
    case 73: case 74: d.id=331; d.count=4+(int)(random%2); break;
    case 75: d.id=76; break;
    case 78: d.id=332; break;
    case 80: d.id=332; d.count=4; break;
    case 82: d.id=337; d.count=4; break;
    case 83: d.id=338; break;
    case 89: d.id=348; d.count=2+(int)(random%3); break;
    case 92: d.id=-1; d.count=0; break;
    case 93: case 94: d.id=356; break;
    default: break;
    }
    return d;
}
void mining_wear(InventorySlot *item,unsigned block)
{
    int max,wear=1;
    if (!item || item->count<=0) return;
    /* Tools take one point per block in Beta; swords take two. Shears only
     * take damage on leaves, cobwebs and wool. Armor/bows are not mining tools. */
    if (item->id==267 || item->id==268 || item->id==272 || item->id==276 || item->id==283) wear=2;
    else if (item->id==359) { if (block!=18 && block!=30 && block!=35) return; }
    else if (tool(item->id)==&tools[0]) return;
    max=inventory_max_damage(item->id);
    if (max<=0) return;
    item->damage+=wear;
    if (item->damage>max) inventory_clear_slot(item);
}
