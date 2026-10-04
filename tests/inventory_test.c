#include "../src/game/inventory.h"
#include "../src/game/creative.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    InventorySlot slots[RECRAFT_INVENTORY_SLOTS];
    inventory_init(slots,0);
    {
        InventorySlot tool={270,2,59},block={3,64,4};
        assert(inventory_damage(&tool,1) && tool.count==1 && tool.damage==0);
        assert(!inventory_damage(&tool,59) && tool.damage==59);
        assert(inventory_damage(&tool,1) && tool.id==-1 && tool.count==0);
        assert(!inventory_damage(&tool,1));
        assert(!inventory_damage(&block,100) && block.count==64 && block.damage==4);
    }
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
        int i,j,wool=0,dyes=0,slabs=0,mechanisms=0;
        InventorySlot item;
        assert(!creative_get(-1,&item));
        assert(!creative_get(creative_count(),&item));
        assert(creative_count()==216);
        assert(!strcmp(beta_item_translation_key(278,17),beta_item_translation_key(278,0)));
        assert(strcmp(beta_item_translation_key(351,1),beta_item_translation_key(351,0)));
        for (i=0;i<creative_count();++i) {
            assert(creative_get(i,&item));
            assert(item.id!=8 && item.id!=9 && item.id!=10 && item.id!=11 && item.id!=55 && item.id!=93 && item.id!=94);
            assert(item.id!=34 && item.id!=36 && item.id!=63 && item.id!=68 && item.id!=64 && item.id!=71 && item.id!=26);
            assert(item.id!=95 && item.id!=358 && (item.id<=96 || (item.id>=256 && item.id<=359) || item.id==2256 || item.id==2257));
            if(item.id==5) assert(item.damage==0);
            if(item.id==6 || item.id==17 || item.id==18) assert(item.damage<=2);
            assert(beta_item_translation_key(item.id,item.damage)[0]);
            for(j=0;j<i;++j) { InventorySlot other; assert(creative_get(j,&other)); assert(other.id!=item.id || other.damage!=item.damage); }
            if(item.id==23 || item.id==29 || item.id==33 || item.id==70 || item.id==72 || item.id==77) ++mechanisms;
        assert(item.id>0 && item.count==inventory_stack_limit(item.id));
            if (item.id==35) ++wool;
            if (item.id==44) ++slabs;
            if (item.id==351) ++dyes;
            if (item.id>=256) assert(beta_item_tile(item.id,item.damage)>=0);
        }
        assert(wool==16 && dyes==16 && slabs==4 && mechanisms==6);
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
