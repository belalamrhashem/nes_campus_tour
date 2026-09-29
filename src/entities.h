// Define the blueprint for an NPC

typedef struct {
    unsigned char active;   // 1 if they are on this map, 0 if the slot is empty
    unsigned int x;         // World X coordinate (16-bit for scrolling!)
    unsigned char y;        // Y coordinate
    const unsigned char* sprite; // Pointer to their metasprite data
    unsigned char text_id;  // Which dialogue text they should say
} NPC;

// Create a strict limit. (The NES only has 2KB of RAM, so don't make this 100!)
#define MAX_NPCS 4
NPC npc_list[MAX_NPCS];