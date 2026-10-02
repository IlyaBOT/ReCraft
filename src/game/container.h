#ifndef RECRAFT_CONTAINER_H
#define RECRAFT_CONTAINER_H
#include "inventory.h"
typedef enum ContainerKind { CONTAINER_PLAYER, CONTAINER_WORKBENCH, CONTAINER_CHEST,
    CONTAINER_FURNACE, CONTAINER_CREATIVE } ContainerKind;
typedef struct ContainerSession {
    ContainerKind kind;
    int x,y,z,size;
    int entity_id; /* Minecart storage uses the existing chest interface. */
    InventorySlot grid[9],cursor;
    int server,window_id,transaction,pending,burn,fuel,cook;
    InventorySlot result,contents[54];
} ContainerSession;
#endif
