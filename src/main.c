#include <nes.h>
#include "neslib.h"
#include "../graphics/test_map/map.h"
#include "../graphics/test_map/map_pallete.h"

#include "../graphics/loading_scr/loading_scr.h"
#include "../graphics/loading_scr/loading_scr_pallete.h"


#define FADE_SLOWNESS 15
#define PLAYER_SPEED 1

//Debug functions
#include <stdio.h>

// Writes a string directly to Mesen's Output / Log console
void debug_print(const char *str) {
    while (*str) {
        *(volatile unsigned char*)0x4018 = *str++;
    }
}


const unsigned char Player_front[]={

	  0,   0, 0x0c, 2,
	  8,   0, 0x0d, 2,
	  0,   8, 0x1c, 2,
	  8,   8, 0x1d, 2,
	0x80

};


void fade_from_black(void) {
    unsigned char i;
    unsigned char j;
    for (i = 0; i <= 4; i++) {
        pal_bright(i);
        for (j = 0; j < FADE_SLOWNESS; j++) {
            ppu_wait_nmi(); // Wait 1 frame between brightness steps
        }
    }
}

void fade_to_black(void) {
    signed char i;
    unsigned char j;
    for (i = 4; i >= 0; i--) {
        pal_bright(i);
        for (j = 0; j < FADE_SLOWNESS; j++) {
            ppu_wait_nmi(); // Wait 1 frame between brightness steps
        }
    }
}

void show_loading_scr(void) {
    ppu_off();
    bank_bg(1);
    pal_bg(loading_scr_palette);
    vram_adr(NAMETABLE_A);
    vram_write(loading_scr, 1024);
    ppu_on_all();
}

void show_game_scr(void) {
    ppu_off();
    bank_bg(0);
    pal_bg(map_palette);
    vram_adr(NAMETABLE_A);
    vram_write(map, 1024);
    ppu_on_all();
    scroll(0, 0);
    pal_bright(4);
}


unsigned char is_solid_tile(unsigned char x, unsigned char y) {
    unsigned char tile_x = x >> 3; //divide x by 8 to get column
    unsigned char tile_y = y >> 3; //divide Y by 8 to get row
    unsigned int map_index;
    unsigned char tile_id;

    // convert to map array index (32 columns per row)
    map_index = ((unsigned int)tile_y * 32) + tile_x;

    //read tile ID
    tile_id = map[map_index];

    //is tile walkable?
    if (tile_id == 0xFD || tile_id == 0xFE || tile_id == 0xFF) {
        return 0;
    }

    return 1;
}

void move_player(unsigned char pad, unsigned char *player_x, unsigned char *player_y) {
    unsigned char new_x = *player_x;
    unsigned char new_y = *player_y;

    // Define sprite bounding box offsets (e.g., a 16x16 sprite)
    // Adjust these if your sprite feet or body collision box is smaller!
    #define BOX_LEFT   2
    #define BOX_RIGHT  13
    #define BOX_TOP    2
    #define BOX_BOTTOM 16

    // --- HORIZONTAL MOVEMENT ---
    if (pad & PAD_LEFT) {
        new_x = *player_x - PLAYER_SPEED;
        // Check top-left and bottom-left corners of player box at new position
        if (is_solid_tile(new_x + BOX_LEFT, *player_y + BOX_TOP) ||
            is_solid_tile(new_x + BOX_LEFT, *player_y + BOX_BOTTOM)) {
            debug_print("Blocked by wall!\n");
            // Hit a wall! Don't update player_x
        } else {
            *player_x = new_x; // Walkable!
        }
    } else if (pad & PAD_RIGHT) {
        new_x = *player_x + PLAYER_SPEED;
        // Check top-right and bottom-right corners
        if (is_solid_tile(new_x + BOX_RIGHT, *player_y + BOX_TOP) ||
            is_solid_tile(new_x + BOX_RIGHT, *player_y + BOX_BOTTOM)) {
            // Hit a wall!
            debug_print("Blocked by wall!\n");
        } else {
            *player_x = new_x;
        }
    }

    // --- VERTICAL MOVEMENT ---
    if (pad & PAD_UP) {
        new_y = *player_y - PLAYER_SPEED;
        // Check top-left and top-right corners
        if (is_solid_tile(*player_x + BOX_LEFT, new_y + BOX_TOP) ||
            is_solid_tile(*player_x + BOX_RIGHT, new_y + BOX_TOP)) {
            // Hit a wall!
            debug_print("Blocked by wall!\n");
        } else {
            *player_y = new_y;
        }
    } else if (pad & PAD_DOWN) {
        new_y = *player_y + PLAYER_SPEED;
        // Check bottom-left and bottom-right corners
        if (is_solid_tile(*player_x + BOX_LEFT, new_y + BOX_BOTTOM) ||
            is_solid_tile(*player_x + BOX_RIGHT, new_y + BOX_BOTTOM)) {
            debug_print("Blocked by wall!\n");
            // Hit a wall!
        } else {
            *player_y = new_y;
        }
    }
}

void main(void) {
    //declarations
    unsigned char oam_id = 0;
    unsigned char pad;

    unsigned char player_x = 54;
    unsigned char player_y = 210;  


    ppu_off();
    oam_clear();
    bank_spr(1);
    pal_spr(loading_scr_palette);

    show_loading_scr();
    fade_from_black();
    delay(300);
    fade_to_black();

    delay(30);
    show_game_scr();

    debug_print("Game started!\n");

    while (1) {
        ppu_wait_nmi();

        oam_id = 0; // Reset OAM buffer index
    
        
        pad = pad_poll(0); // Poll Controller 1

        move_player(pad, &player_x, &player_y);

        // Keep player inside screen boundaries
        if (player_x < 8)   player_x = 8;
        else if (player_x > 240) player_x = 240; // 256 minus sprite width
        if (player_y < 2)   player_y = 2;
        else if (player_y > 220) player_y = 220; // 240 minus sprite height
        

        // Draw the metasprite defined in NEXXT
        oam_id = oam_meta_spr(player_x, player_y, oam_id, Player_front);
        

        // Hide unused sprite slots to prevent black box glitches!
        oam_hide_rest(oam_id);
    }
}