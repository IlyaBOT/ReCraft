#ifndef RECRAFT_CRAFTING_H
#define RECRAFT_CRAFTING_H
#include "inventory.h"

/* Grid is row-major, width 2 or 3. Metadata -1 is a recipe wildcard only. */
int crafting_match(const InventorySlot *grid, int width, InventorySlot *result);
int crafting_take(InventorySlot *grid, int width, InventorySlot *cursor,
                  InventorySlot inventory[RECRAFT_INVENTORY_SLOTS]);
int crafting_recipe_count(void);
int furnace_recipe(int input, InventorySlot *result);
int furnace_fuel_ticks(int item);
#endif
