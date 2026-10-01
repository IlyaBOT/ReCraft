#include "inventory.h"

#include <string.h>

static const int initial_blocks[9]={1,3,2,12,4,17,18,20,50};
typedef struct ItemProperties { int id,stack,damage; } ItemProperties;
static const ItemProperties properties[]={
#include "beta_items.def"
};

void inventory_clear_slot(InventorySlot *slot)
{
    if (slot) { slot->id=-1; slot->count=slot->damage=0; }
}

int inventory_stack_limit(int id)
{
    size_t i;
    for (i=0;i<sizeof(properties)/sizeof(properties[0]);++i)
        if (properties[i].id==id) return properties[i].stack;
    return 64;
}

int inventory_max_damage(int id)
{
    size_t i;
    for (i=0;i<sizeof(properties)/sizeof(properties[0]);++i)
        if (properties[i].id==id) return properties[i].damage;
    return 0;
}

int inventory_same(const InventorySlot *a,const InventorySlot *b)
{
    return a && b && a->id==b->id && a->damage==b->damage;
}

int inventory_add_stack(InventorySlot *slots,int size,InventorySlot item)
{
    int i,total=0,limit=inventory_stack_limit(item.id);
    if (!slots || item.id<=0 || item.count<=0) return 0;
    for (i=0;i<size && item.count>0;++i) if (slots[i].count>0 &&
        inventory_same(&slots[i],&item) && slots[i].count<limit) {
        int n=limit-slots[i].count;
        if (n>item.count) n=item.count;
        slots[i].count+=n; item.count-=n; total+=n;
    }
    for (i=0;i<size && item.count>0;++i) if (slots[i].id<=0 || slots[i].count<=0) {
        int n=item.count>limit ? limit : item.count;
        slots[i]=item; slots[i].count=n; item.count-=n; total+=n;
    }
    return total;
}

int inventory_click(InventorySlot *slot,InventorySlot *cursor,int right,int output)
{
    int amount,space;
    InventorySlot copy;
    if (!slot || !cursor) return 0;
    if (output) {
        if (slot->id<=0 || slot->count<=0) return 0;
        if (cursor->count>0 && cursor->id>0 && (!inventory_same(slot,cursor) ||
            cursor->count+slot->count>inventory_stack_limit(slot->id))) return 0;
        if (cursor->count<=0 || cursor->id<=0) *cursor=*slot;
        else cursor->count+=slot->count;
        inventory_clear_slot(slot); return 1;
    }
    if (cursor->id<=0 || cursor->count<=0) {
        if (slot->id<=0 || slot->count<=0) return 0;
        *cursor=*slot;
        if (right) cursor->count=(slot->count+1)/2;
        slot->count-=cursor->count;
        if (!slot->count) inventory_clear_slot(slot);
        return 1;
    }
    if (slot->id<=0 || slot->count<=0 || inventory_same(slot,cursor)) {
        space=inventory_stack_limit(cursor->id)-(slot->count>0 ? slot->count : 0);
        if (space<=0) return 0;
        amount=right ? 1 : cursor->count;
        if (amount>space) amount=space;
        if (slot->id<=0 || slot->count<=0) { *slot=*cursor; slot->count=0; }
        slot->count+=amount; cursor->count-=amount;
        if (!cursor->count) inventory_clear_slot(cursor);
        return 1;
    }
    if (right) return 0;
    copy=*slot; *slot=*cursor; *cursor=copy;
    return 1;
}

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
    InventorySlot item={id,count,0};
    return inventory_add_stack(slots,RECRAFT_INVENTORY_SLOTS,item);
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
