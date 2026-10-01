#include "crafting.h"
#include "../world/beta_blocks.h"
#include <stddef.h>

typedef struct Ingredient { int id,damage; } Ingredient;
typedef struct Recipe {
    int width,height,shapeless;
    InventorySlot result;
    Ingredient ingredients[9];
} Recipe;
static const Recipe recipes[] = {
#include "beta_recipes.def"
};

int crafting_recipe_count(void) { return (int)(sizeof(recipes)/sizeof(recipes[0])); }

static int matches(Ingredient required, const InventorySlot *item)
{
    if (!required.id) return item->id<=0 || item->count<=0;
    return item->count>0 && item->id==required.id &&
           (required.damage<0 || item->damage==required.damage);
}

static int shaped(const Recipe *r,const InventorySlot *grid,int width,int ox,int oy,int mirror)
{
    int x,y;
    for (y=0;y<width;++y) for (x=0;x<width;++x) {
        Ingredient need={0,0};
        int rx=x-ox,ry=y-oy;
        if (rx>=0 && rx<r->width && ry>=0 && ry<r->height)
            need=r->ingredients[ry*r->width+(mirror ? r->width-rx-1 : rx)];
        if (!matches(need,&grid[y*width+x])) return 0;
    }
    return 1;
}

int crafting_match(const InventorySlot *grid,int width,InventorySlot *result)
{
    int i,x,y,mirror;
    if (!grid || !result || (width!=2 && width!=3)) return 0;
    inventory_clear_slot(result);
    for (i=0;i<crafting_recipe_count();++i) {
        const Recipe *r=&recipes[i];
        if (r->shapeless) {
            unsigned used=0;
            int j,k,count=0,ok=1;
            for (j=0;j<width*width;++j) if (grid[j].id>0 && grid[j].count>0) {
                for (k=0;k<r->width;++k)
                    if (!(used&(1u<<k)) && matches(r->ingredients[k],&grid[j])) break;
                if (k==r->width) { ok=0; break; }
                used|=1u<<k; ++count;
            }
            if (ok && count==r->width) { *result=r->result; return 1; }
        } else if (r->width<=width && r->height<=width) {
            for (y=0;y<=width-r->height;++y) for (x=0;x<=width-r->width;++x)
                for (mirror=0;mirror<2;++mirror)
                    if (shaped(r,grid,width,x,y,mirror)) { *result=r->result; return 1; }
        }
    }
    return 0;
}

int crafting_take(InventorySlot *grid,int width,InventorySlot *cursor,InventorySlot inventory[RECRAFT_INVENTORY_SLOTS])
{
    InventorySlot result;
    int i;
    if (!cursor || !crafting_match(grid,width,&result)) return 0;
    if (cursor->id>0 && cursor->count>0 &&
        (!inventory_same(cursor,&result) || cursor->count+result.count>inventory_stack_limit(result.id))) return 0;
    if (cursor->count<=0 || cursor->id<=0) *cursor=result;
    else cursor->count+=result.count;
    for (i=0;i<width*width;++i) if (grid[i].id>0 && grid[i].count>0) {
        int milk=grid[i].id==335;
        if (--grid[i].count==0) inventory_clear_slot(&grid[i]);
        /* Beta SlotCrafting returns containers for milk used by cake. */
        if (milk) {
            InventorySlot bucket={325,1,0};
            if (grid[i].count<=0) grid[i]=bucket;
            else if (inventory) inventory_add_stack(inventory,RECRAFT_INVENTORY_SLOTS,bucket);
        }
    }
    return 1;
}

int furnace_recipe(int input,InventorySlot *result)
{
    static const int data[][3]={{15,265,0},{14,266,0},{56,264,0},{12,20,0},
        {319,320,0},{349,350,0},{4,1,0},{337,336,0},{81,351,2},{17,263,1}};
    size_t i;
    if (!result) return 0;
    inventory_clear_slot(result);
    for (i=0;i<sizeof(data)/sizeof(data[0]);++i) if (data[i][0]==input) {
        *result=(InventorySlot){data[i][1],1,data[i][2]}; return 1;
    }
    return 0;
}

int furnace_fuel_ticks(int item)
{
    if (item>0 && item<BETA_BLOCK_COUNT && beta_material_wood((unsigned)item)) return 300;
    switch (item) {
    case 6: case 280: return 100;
    case 263: return 1600;
    case 327: return 20000;
    default: return 0;
    }
}
