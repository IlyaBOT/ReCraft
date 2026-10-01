#include "creative.h"
#include <stddef.h>

/* Numeric registry and items.png coordinates from Beta 1.7.3 Item.java.
 * This table is runtime data; no Java or reference tree is required. */
typedef struct ItemIcon { int id,tile; const char *name; } ItemIcon;
static const ItemIcon icons[]={
    {256, 82, "Shovel Iron"},
    {257, 98, "Pickaxe Iron"},
    {258, 114, "Hatchet Iron"},
    {259, 5, "Flint And Steel"},
    {260, 10, "Apple"},
    {261, 21, "Bow"},
    {262, 37, "Arrow"},
    {263, 7, "Coal"},
    {264, 55, "Diamond"},
    {265, 23, "Ingot Iron"},
    {266, 39, "Ingot Gold"},
    {267, 66, "Sword Iron"},
    {268, 64, "Sword Wood"},
    {269, 80, "Shovel Wood"},
    {270, 96, "Pickaxe Wood"},
    {271, 112, "Hatchet Wood"},
    {272, 65, "Sword Stone"},
    {273, 81, "Shovel Stone"},
    {274, 97, "Pickaxe Stone"},
    {275, 113, "Hatchet Stone"},
    {276, 67, "Sword Diamond"},
    {277, 83, "Shovel Diamond"},
    {278, 99, "Pickaxe Diamond"},
    {279, 115, "Hatchet Diamond"},
    {280, 53, "Stick"},
    {281, 71, "Bowl"},
    {282, 72, "Mushroom Stew"},
    {283, 68, "Sword Gold"},
    {284, 84, "Shovel Gold"},
    {285, 100, "Pickaxe Gold"},
    {286, 116, "Hatchet Gold"},
    {287, 8, "String"},
    {288, 24, "Feather"},
    {289, 40, "Sulphur"},
    {290, 128, "Hoe Wood"},
    {291, 129, "Hoe Stone"},
    {292, 130, "Hoe Iron"},
    {293, 131, "Hoe Diamond"},
    {294, 132, "Hoe Gold"},
    {295, 9, "Seeds"},
    {296, 25, "Wheat"},
    {297, 41, "Bread"},
    {298, 0, "Helmet Cloth"},
    {299, 16, "Chestplate Cloth"},
    {300, 32, "Leggings Cloth"},
    {301, 48, "Boots Cloth"},
    {302, 1, "Helmet Chain"},
    {303, 17, "Chestplate Chain"},
    {304, 33, "Leggings Chain"},
    {305, 49, "Boots Chain"},
    {306, 2, "Helmet Iron"},
    {307, 18, "Chestplate Iron"},
    {308, 34, "Leggings Iron"},
    {309, 50, "Boots Iron"},
    {310, 3, "Helmet Diamond"},
    {311, 19, "Chestplate Diamond"},
    {312, 35, "Leggings Diamond"},
    {313, 51, "Boots Diamond"},
    {314, 4, "Helmet Gold"},
    {315, 20, "Chestplate Gold"},
    {316, 36, "Leggings Gold"},
    {317, 52, "Boots Gold"},
    {318, 6, "Flint"},
    {319, 87, "Porkchop Raw"},
    {320, 88, "Porkchop Cooked"},
    {321, 26, "Painting"},
    {322, 11, "Apple Gold"},
    {323, 42, "Sign"},
    {324, 43, "Door Wood"},
    {325, 74, "Bucket"},
    {326, 75, "Bucket Water"},
    {327, 76, "Bucket Lava"},
    {328, 135, "Minecart"},
    {329, 104, "Saddle"},
    {330, 44, "Door Iron"},
    {331, 56, "Redstone"},
    {332, 14, "Snowball"},
    {333, 136, "Boat"},
    {334, 103, "Leather"},
    {335, 77, "Milk"},
    {336, 22, "Brick"},
    {337, 57, "Clay"},
    {338, 27, "Reeds"},
    {339, 58, "Paper"},
    {340, 59, "Book"},
    {341, 30, "Slimeball"},
    {342, 151, "Minecart Chest"},
    {343, 167, "Minecart Furnace"},
    {344, 12, "Egg"},
    {345, 54, "Compass"},
    {346, 69, "Fishing Rod"},
    {347, 70, "Clock"},
    {348, 73, "Yellow Dust"},
    {349, 89, "Fish Raw"},
    {350, 90, "Fish Cooked"},
    {351, 78, "Dye Powder"},
    {352, 28, "Bone"},
    {353, 13, "Sugar"},
    {354, 29, "Cake"},
    {355, 45, "Bed"},
    {356, 86, "Diode"},
    {357, 92, "Cookie"},
    {358, 60, "Map"},
    {359, 93, "Shears"},
    {2256, 240, "Music Disc 13"},
    {2257, 241, "Music Disc Cat"},
};

int beta_item_tile(int id,int damage)
{
    size_t i;
    for (i=0;i<sizeof(icons)/sizeof(icons[0]);++i) if (icons[i].id==id) {
        if (id==351) return icons[i].tile+(damage&7)*16+((damage>>3)&1);
        return icons[i].tile;
    }
    return -1;
}
const char *beta_item_name(int id)
{
    switch(id) {
    case 58: return "Crafting Table";
    case 54: return "Chest";
    case 61: case 62: return "Furnace";
    case 75: case 76: return "Redstone Torch";
    case 81: return "Cactus";
    case 90: return "Nether Portal";
    default: break;
    }
    size_t i;
    for (i=0;i<sizeof(icons)/sizeof(icons[0]);++i)
        if (icons[i].id==id) return icons[i].name;
    return NULL;
}

static int variants(int id)
{
    if (id==35 || id==351) return 16;
    if (id==6 || id==17 || id==18 || id==31) return 3;
    if (id==43 || id==44) return 4;
    if (id==263) return 2;
    return 1;
}

int creative_get(int index,InventorySlot *out)
{
    int id,n;
    if (!out || index<0) return 0;
    for (id=1;id<=2257;++id) {
        if (id==97) id=256;
        if (id==360) id=2256;
        n=variants(id);
        if (index<n) {
            out->id=id; out->damage=index; out->count=inventory_stack_limit(id);
            return 1;
        }
        index-=n;
    }
    return 0;
}

int creative_count(void)
{
    InventorySlot slot;
    int n=0;
    while (creative_get(n,&slot)) ++n;
    return n;
}

int creative_give(InventorySlot *slots,int hotbar,int index,int creative)
{
    InventorySlot item;
    if (!creative || !slots || hotbar<0 || hotbar>=9 || !creative_get(index,&item)) return 0;
    slots[hotbar]=item;
    return 1;
}
