#ifndef RECRAFT_INVENTORY_H
#define RECRAFT_INVENTORY_H

#define RECRAFT_INVENTORY_SLOTS 36

typedef struct InventorySlot {
    int id, count, damage;
} InventorySlot;

typedef struct ItemDrop {
    int active,id,count;
    float x,y,z,vy,age;
} ItemDrop;

void inventory_init(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int creative);
int inventory_add(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int id, int count);
int inventory_take(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int slot, int creative);
void inventory_swap(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int a, int b);

#endif
