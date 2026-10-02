#ifndef RECRAFT_INVENTORY_H
#define RECRAFT_INVENTORY_H

#define RECRAFT_INVENTORY_SLOTS 36

typedef struct InventorySlot {
    int id, count, damage;
} InventorySlot;

typedef struct ItemDrop {
    int active,id,count,damage;
    float x,y,z,vx,vy,vz,age;
    int health,pickup_delay;
} ItemDrop;

void inventory_init(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int creative);
int inventory_add(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int id, int count);
int inventory_take(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int slot, int creative);
void inventory_swap(InventorySlot slots[RECRAFT_INVENTORY_SLOTS], int a, int b);
void inventory_clear_slot(InventorySlot *slot);
int inventory_stack_limit(int id);
int inventory_max_damage(int id);
/* Beta ItemStack: one item breaks only after damage exceeds its maximum.
 * Returns 1 on break, 0 for wear only or a non-damageable/empty stack. */
int inventory_damage(InventorySlot *slot,int amount);
int inventory_same(const InventorySlot *a, const InventorySlot *b);
int inventory_add_stack(InventorySlot *slots, int size, InventorySlot item);
/* Left: pick up/merge/swap. Right: split or place one. Output slots only take. */
int inventory_click(InventorySlot *slot, InventorySlot *cursor, int right, int output);

#endif
