#include "../src/game/inventory.h"
#include "../src/game/creative.h"
#include <assert.h>

int main(void)
{
    InventorySlot slots[RECRAFT_INVENTORY_SLOTS];
    inventory_init(slots,0);
    assert(slots[0].id==-1 && slots[0].count==0);
    assert(inventory_add(slots,1,65)==65);
    assert(slots[0].id==1 && slots[0].count==64);
    assert(slots[1].id==1 && slots[1].count==1);
    assert(inventory_take(slots,1,0));
    assert(slots[1].id==-1 && slots[1].count==0);
    inventory_swap(slots,0,9);
    assert(slots[9].id==1 && slots[0].id==-1);
    inventory_init(slots,1);
    assert(slots[0].id==1 && slots[0].count==64);
    assert(inventory_take(slots,0,1));
    assert(slots[0].count==64);
    {
        int i,wool=0,dyes=0,slabs=0;
        InventorySlot item;
        assert(!creative_get(-1,&item));
        assert(!creative_get(creative_count(),&item));
        for (i=0;i<creative_count();++i) {
            assert(creative_get(i,&item));
            assert(item.id>0 && item.count==64);
            if (item.id==35) ++wool;
            if (item.id==44) ++slabs;
            if (item.id==351) ++dyes;
            if (item.id>=256) assert(beta_item_tile(item.id,item.damage)>=0);
        }
        assert(wool==16 && dyes==16 && slabs==4);
        assert(beta_item_tile(256,0)==82);
        assert(beta_item_tile(2257,0)==241);
        assert(beta_item_tile(351,15)==191);
        assert(!creative_give(slots,0,1,0));
        assert(slots[0].id==1);
        assert(creative_give(slots,8,1,1) && slots[8].id==2);
        assert(inventory_take(slots,8,1) && slots[8].count==64);
    }
    return 0;
}
