#include "../src/game/crafting.h"
#include <assert.h>
#include <stdio.h>
static void clear(InventorySlot *slots,int size)
{ int i; for (i=0;i<size;++i) inventory_clear_slot(&slots[i]); }
int main(void)
{
    assert(furnace_fuel_ticks(63)==300 && furnace_fuel_ticks(65)==0 && furnace_fuel_ticks(68)==300);
    assert(furnace_fuel_ticks(50)==0 && furnace_fuel_ticks(263)==1600 && furnace_fuel_ticks(327)==20000);
    InventorySlot grid[9],result,cursor,inventory[36],slot;
    assert(crafting_recipe_count()==151);
    clear(grid,9); clear(inventory,36); inventory_clear_slot(&cursor);
    grid[3]=(InventorySlot){17,1,2};
    assert(crafting_match(grid,2,&result) && result.id==5 && result.count==4);
    assert(crafting_take(grid,2,&cursor,inventory) && cursor.id==5 && cursor.count==4);
    assert(!crafting_match(grid,2,&result));
    clear(grid,9);
    grid[0]=grid[1]=grid[2]=(InventorySlot){5,2,0};
    grid[4]=grid[7]=(InventorySlot){280,2,0};
    assert(crafting_match(grid,3,&result) && result.id==270 && result.count==1);
    assert(!crafting_match(grid,2,&result));
    inventory_clear_slot(&cursor);
    assert(crafting_take(grid,3,&cursor,inventory) && cursor.id==270);
    assert(!crafting_take(grid,3,&cursor,inventory)); /* tools do not stack */
    clear(grid,9);
    grid[1]=grid[2]=grid[5]=(InventorySlot){265,1,0};
    grid[4]=grid[7]=(InventorySlot){280,1,0};
    assert(crafting_match(grid,3,&result) && result.id==258); /* reflected axe */
    clear(grid,9);
    grid[3]=(InventorySlot){351,1,1}; grid[0]=(InventorySlot){351,1,15};
    assert(crafting_match(grid,2,&result) && result.id==351 && result.damage==9 && result.count==2);
    grid[1]=(InventorySlot){1,1,0};
    assert(!crafting_match(grid,2,&result));
    clear(grid,9);
    grid[0]=grid[1]=grid[2]=(InventorySlot){335,1,0};
    grid[3]=grid[5]=(InventorySlot){353,1,0}; grid[4]=(InventorySlot){344,1,0};
    grid[6]=grid[7]=grid[8]=(InventorySlot){296,1,0};
    inventory_clear_slot(&cursor);
    assert(crafting_take(grid,3,&cursor,inventory) && cursor.id==354);
    assert(grid[0].id==325 && grid[1].id==325 && grid[2].id==325);
    slot=(InventorySlot){35,9,14}; inventory_clear_slot(&cursor);
    assert(inventory_click(&slot,&cursor,1,0) && slot.count==4 && cursor.count==5 && cursor.damage==14);
    assert(inventory_click(&slot,&cursor,1,0) && slot.count==5 && cursor.count==4);
    slot=(InventorySlot){1,1,0};
    assert(!inventory_click(&slot,&cursor,0,1)); /* output cannot accept wool */
    assert(inventory_stack_limit(325)==1 && inventory_stack_limit(332)==16);
    assert(inventory_max_damage(270)==59 && inventory_max_damage(278)==1561);
    assert(furnace_fuel_ticks(263)==1600 && furnace_fuel_ticks(327)==20000);
    assert(furnace_recipe(17,&result) && result.id==263 && result.damage==1);
    puts("Vanilla Beta crafting, metadata, containers and furnace recipes passed");
    return 0;
}
