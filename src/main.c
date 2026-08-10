#include <nes.h>
#include "neslib.h"
#include "../graphics/test_map/map_pallete.h"
#include "../graphics/stephenson/map_left.h"
#include "../graphics/stephenson/map_right.h"

#include "../graphics/loading_scr/loading_scr_new.h"
#include "../graphics/loading_scr/loading_scr_pallete.h"


#define FADE_SLOWNESS 15
#define PLAYER_SPEED 1

#define MARGIN_RIGHT 160  // Scroll right when player passes pixel 160 on screen
#define MARGIN_LEFT  80   // Scroll left when player goes behind pixel 80 on screen
#define MAP_MAX_X    512  // Total width of your 64x32 map in pixels
#define SCREEN_W     256

//Debug functions
#include <stdio.h>

#define STATE_PLAYING   0
#define STATE_DIALOGUE  1

unsigned char game_state = STATE_PLAYING;
unsigned char pad_old = 0; // Remembers what you pressed last frame
unsigned char pad_new = 0; // Only triggers ONCE when pressed

const unsigned char Player_front[]={

	  0,   0, 0x04, 2,
	  8,   0, 0x05, 2,
	  0,   8, 0x14, 2,
	  8,   8, 0x15, 2,
	0x80

};

const unsigned char Player_back[]={

	  0,   0, 0x06, 2,
	  8,   0, 0x07, 2,
	  0,   8, 0x16, 2,
	  8,   8, 0x17, 2,
	0x80

};

const unsigned char Player_right[]={

	  0,   0, 0x08, 2,
	  8,   0, 0x09, 2,
	  0,   8, 0x18, 2,
	  8,   8, 0x19, 2,
	0x80

};

const unsigned char Player_left[]={

	 8,   0, 0x08, 2 | OAM_FLIP_H,
	 0,   0, 0x09, 2 | OAM_FLIP_H,
	 8,   8, 0x18, 2 | OAM_FLIP_H,
	 0,   8, 0x19, 2 | OAM_FLIP_H,
	0x80

};


// Matches the label exported in loading_jingle.s
extern const unsigned char loading_jingle[];


void load_full_map(void) {
    // 1. Write the Left Screen perfectly into Nametable A
    vram_adr(NAMETABLE_A); 
    vram_write(map_left, 1024);

    // 2. Write the Right Screen perfectly into Nametable B
    vram_adr(NAMETABLE_B); 
    vram_write(map_right, 1024);
}

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
    music_play(1); // Play the loading jingle
    ppu_off();
    bank_bg(0);
    pal_bg(loading_scr_palette);
    vram_adr(NAMETABLE_A);
    vram_write(loading_scr_new, 1024);
    ppu_on_all();
}

void show_game_scr(void) {
    music_play(0); // Play the game music
    ppu_off();
    bank_bg(0);
    pal_bg(map_palette);
    vram_adr(NAMETABLE_A);
    //vram_write(map_data, 1024);
    ppu_on_all();
    scroll(0, 0);
    pal_bright(4);
}


// Notice: 'x' MUST be an unsigned int because it spans 512 pixels!
unsigned char is_solid_tile(unsigned int x, unsigned char y) {
    unsigned char tile_x = x >> 3; // Divide by 8 (0 to 63)
    unsigned char tile_y = y >> 3; // Divide Y by 8
    unsigned int map_index;
    unsigned char tile_id;

    if (tile_x < 32) {
        // Player is on the Left Screen
        map_index = ((unsigned int)tile_y * 32) + tile_x;
        tile_id = map_left[map_index];
    } else {
        // Player is on the Right Screen
        map_index = ((unsigned int)tile_y * 32) + (tile_x - 32);
        tile_id = map_right[map_index];
    }

    // Now you can safely check your solid tile IDs!
    if (tile_id == 0x10 || tile_id == 0x11 || tile_id == 0x13 || tile_id == 0x08 || tile_id == 0x09 || tile_id == 0x18 || tile_id == 0x19 || tile_id == 0x0A) return 1;

    return 0; 
}

void move_player(unsigned char pad, unsigned int *player_x, unsigned char *player_y, unsigned char *player_sprite_state) {
    unsigned int new_x = *player_x;
    unsigned char new_y = *player_y;

    #define BOX_LEFT   2
    #define BOX_RIGHT  13
    #define BOX_TOP    2
    #define BOX_BOTTOM 16

    // --- HORIZONTAL MOVEMENT ---
    if (pad & PAD_LEFT) {
        *player_sprite_state = 3; // Facing left
        if (*player_x >= PLAYER_SPEED) { // Safe to subtract
            new_x = *player_x - PLAYER_SPEED;
            if (!is_solid_tile(new_x + BOX_LEFT, *player_y + BOX_TOP) &&
                !is_solid_tile(new_x + BOX_LEFT, *player_y + BOX_BOTTOM)) {
                *player_x = new_x; 
            }
        } else {
            *player_x = 0; // Clamp to edge
        }
    } else if (pad & PAD_RIGHT) {
        *player_sprite_state = 2; // Facing right
        if (*player_x <= (MAP_MAX_X - 16 - PLAYER_SPEED)) { // Safe to add
            new_x = *player_x + PLAYER_SPEED;
            if (!is_solid_tile(new_x + BOX_RIGHT, *player_y + BOX_TOP) &&
                !is_solid_tile(new_x + BOX_RIGHT, *player_y + BOX_BOTTOM)) {
                *player_x = new_x;
            }
        } else {
            *player_x = MAP_MAX_X - 16; // Clamp to edge
        }
    }

    // --- VERTICAL MOVEMENT ---
    if (pad & PAD_UP) {
        *player_sprite_state = 1; // Facing back
        new_y = *player_y - PLAYER_SPEED;
        if (!is_solid_tile(*player_x + BOX_LEFT, new_y + BOX_TOP) &&
            !is_solid_tile(*player_x + BOX_RIGHT, new_y + BOX_TOP)) {
            *player_y = new_y;
        }
    } else if (pad & PAD_DOWN) {
        *player_sprite_state = 0; // Facing front
        new_y = *player_y + PLAYER_SPEED;
        if (!is_solid_tile(*player_x + BOX_LEFT, new_y + BOX_BOTTOM) &&
            !is_solid_tile(*player_x + BOX_RIGHT, new_y + BOX_BOTTOM)) {
            *player_y = new_y;
        }
    }
}

void update_camera(unsigned int *player_x, unsigned int *cam_x) {
    unsigned int player_screen_x = *player_x - *cam_x;

    // --- SCROLL RIGHT ---
    if (player_screen_x > MARGIN_RIGHT) {
        *cam_x = *player_x - MARGIN_RIGHT;
        // Hard clamp! Never let the camera scroll past the map boundary
        if (*cam_x > (MAP_MAX_X - SCREEN_W)) {
            *cam_x = (MAP_MAX_X - SCREEN_W); 
        }
    }

    // --- SCROLL LEFT ---
    if (player_screen_x < MARGIN_LEFT) {
        if (*player_x > MARGIN_LEFT) {
            *cam_x = *player_x - MARGIN_LEFT;
        } else {
            *cam_x = 0; // Hard clamp!
        }
    }
}

void main(void) {
    unsigned char oam_id = 0;
    unsigned char pad;
    unsigned int player_x = 100;
    unsigned char player_y = 210; 
    unsigned int cam_x = 0;
    unsigned char draw_x;

    unsigned char player_sprite_state = 0; // 0=front, 1=back, 2=right, 3=left

    ppu_off();
    oam_clear();
    bank_spr(1);
    pal_spr(map_palette);

    show_loading_scr();
    fade_from_black();
    delay(300);
    fade_to_black();
    delay(30);

    // --- NEW CORRECT RENDER ORDER ---
    ppu_off();               // 1. Turn rendering OFF
    bank_bg(0);              // 2. Set graphics bank
    music_play(0);           // 3. Start music
    load_full_map();         // 4. Stream 2048 bytes to VRAM
    pal_bg(map_palette);     // 5. Load the palette
    scroll(0, 0);            // 6. Reset the scroll position
    ppu_on_all();            // 7. FINALLY turn the screen ON!
    pal_bright(4);           // 8. Restore brightness
    // --------------------------------

    while (1) {
        ppu_wait_nmi();

        oam_id = 0; 
        pad = pad_poll(0); 

        move_player(pad, &player_x, &player_y, &player_sprite_state);

        if (player_y < 2)   player_y = 2;
        else if (player_y > 220) player_y = 220; 

        update_camera(&player_x, &cam_x);
        scroll(cam_x, 0);

        draw_x = (unsigned char)(player_x - cam_x);
        switch (player_sprite_state) {
            case 0:
                oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_front);
                break;
            case 1:
                oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_back);
                break;
            case 2:
                oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_right);
                break;
            case 3:
                oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_left);
                break;
        }
        oam_hide_rest(oam_id);
    }
}