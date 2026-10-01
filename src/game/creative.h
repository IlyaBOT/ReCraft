#ifndef RECRAFT_CREATIVE_H
#define RECRAFT_CREATIVE_H
#include "inventory.h"
/* Read-only catalogue of Beta blocks/items; granting is local Creative only. */
int creative_count(void);
int creative_get(int index, InventorySlot *out);
int creative_give(InventorySlot *slots, int hotbar, int index, int creative);
int beta_item_tile(int id, int damage);
const char *beta_item_name(int id);
#endif
