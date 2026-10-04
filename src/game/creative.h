#ifndef RECRAFT_CREATIVE_H
#define RECRAFT_CREATIVE_H
#include "inventory.h"
/* Read-only catalogue of Beta blocks/items; granting is local Creative only. */
int creative_count(void);
int creative_get(int index, InventorySlot *out);
int creative_give(InventorySlot *slots, int hotbar, int index, int creative);
int beta_item_tile(int id, int damage);
const char *beta_item_name(int id);
const char *beta_item_translation_key(int id,int damage);
const char *beta_item_variant_name(int id,int damage);
#endif
