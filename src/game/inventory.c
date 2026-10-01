#include "inventory.h"

#include <string.h>

static const int initial_blocks[9]={1,3,2,12,4,17,18,20,50};

void inventory_init(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int creative)
{
    int i;
    memset(slots,0,sizeof(InventorySlot)*RECRAFT_INVENTORY_SLOTS);
    for (i=0;i<RECRAFT_INVENTORY_SLOTS;++i) slots[i].id=-1;
    if (creative) for (i=0;i<9;++i) {
        slots[i].id=initial_blocks[i];
        slots[i].count=64;
    }
}

int inventory_add(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int id, int count)
{
    int i,added=0;
    if (id<=0 || count<=0) return 0;
    for (i=0;i<RECRAFT_INVENTORY_SLOTS && count>0;++i)
        if (slots[i].id==id && slots[i].count>0 && slots[i].count<64) {
            int n=64-slots[i].count;
            if (n>count) n=count;
            slots[i].count+=n; count-=n; added+=n;
        }
    for (i=0;i<RECRAFT_INVENTORY_SLOTS && count>0;++i)
        if (slots[i].id<0 || slots[i].count<=0) {
            int n=count>64 ? 64 : count;
            slots[i].id=id; slots[i].count=n; slots[i].damage=0;
            count-=n; added+=n;
        }
    return added;
}

int inventory_take(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int slot, int creative)
{
    InventorySlot *item;
    if (slot<0 || slot>=RECRAFT_INVENTORY_SLOTS) return 0;
    item=&slots[slot];
    if (item->id<=0 || item->count<=0) return 0;
    if (!creative && --item->count==0) {
        item->id=-1;
        item->damage=0;
    }
    return 1;
}

void inventory_swap(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int a, int b)
{
    InventorySlot copy;
    if (a<0 || b<0 || a>=RECRAFT_INVENTORY_SLOTS ||
        b>=RECRAFT_INVENTORY_SLOTS) return;
    copy=slots[a]; slots[a]=slots[b]; slots[b]=copy;
}
