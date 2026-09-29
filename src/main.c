#include <nes.h>
#include "neslib.h"

#include "entities.h"

#include "../graphics/test_map/map_palette.h"
#include "../graphics/stephenson/map_left3.h"
#include "../graphics/stephenson/map_right3.h"

#include "../graphics/loading_scr/loading_scr_new.h"
#include "../graphics/loading_scr/loading_scr_palette.h"

#include "../graphics/sprites/sprite_palette.h"


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

#define INTERACT_RANGE       20  // pixels - how close the player must be to talk to an NPC
#define DIALOGUE_TOP_ROW      24 // first tile row the box occupies (screen is rows 0-29)
#define DIALOGUE_ROWS          4 // rows tall

// MATCH THESE TO CHR TILES WHEN MADE
#define DIALOGUE_BG_TILE     0x03  // blank tile for the box interior
#define DIALOGUE_BORDER_TILE 0x01  // top/bottom border tile (fine to match BG for a plain box)
#define FONT_TILE_BASE       0xDC  // CHR tile ID for 'A'

// --- Dialogue system state ---
unsigned char current_npc = 0xFF;       // which NPC we're talking to (0xFF = none)
unsigned char dialogue_needs_open = 0;  // set for one frame to trigger drawing the box
unsigned char dialogue_needs_close = 0; // set for one frame to trigger erasing the box
unsigned char dialogue_row_buf[32];     // scratch buffer for one nametable row

//AI generated jargon
const char* const dialogue_text[] = {
    "...",                     // text_id 0 - reserved/unused
    "      HI, WELCOME TO CAMPUS!",   // text_id 1 (matches npc_list[0].text_id)
    "THIS IS THE STEPHENSON BUILDING.", // text_id 2
    "WE HOPE TO SEE YOU IN SEPTEMBER!"
};
#define NUM_DIALOGUE_LINES (sizeof(dialogue_text) / sizeof(dialogue_text[0]))


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

const unsigned char npc[]={

	0,   0, 0x0a, 3,
    8,   0, 0x0b, 3,
	0,   8, 0x1a, 3,
	8,   8, 0x1b, 3,
	0x80

};



// Matches the label exported in loading_jingle.s
extern const unsigned char loading_jingle[];


void load_full_map(void) {
    // 1. Write the Left Screen perfectly into Nametable A
    vram_adr(NAMETABLE_A); 
    vram_write(map_left3, 1024);

    // 2. Write the Right Screen perfectly into Nametable B
    vram_adr(NAMETABLE_B); 
    vram_write(map_right3, 1024);
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
    set_chr_bank(0); // Set CHR bank 0 for the loading screen graphics
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
        tile_id = map_left3[map_index];
    } else {
        // Player is on the Right Screen
        map_index = ((unsigned int)tile_y * 32) + (tile_x - 32);
        tile_id = map_right3[map_index];
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
    unsigned int player_screen_x;

    // --- SAFETY NET: Prevent Unsigned Underflow ---
    if (*player_x >= *cam_x) {
        player_screen_x = *player_x - *cam_x;
    } else {
        player_screen_x = 0; 
        *cam_x = *player_x; 
    }

    // --- SCROLL RIGHT ---
    if (player_screen_x > MARGIN_RIGHT) {
        *cam_x = *player_x - MARGIN_RIGHT;
        if (*cam_x > (MAP_MAX_X - SCREEN_W)) {
            *cam_x = (MAP_MAX_X - SCREEN_W); 
        }
    }

    // --- SCROLL LEFT ---
    if (player_screen_x < MARGIN_LEFT) {
        if (*player_x > MARGIN_LEFT) {
            *cam_x = *player_x - MARGIN_LEFT;
        } else {
            *cam_x = 0; 
        }
    }
}

void init_map_npcs(void) {
    unsigned char i;
    
    // 1. Clear the array first to destroy any garbage RAM data
    for (i = 0; i < MAX_NPCS; i++) {
        npc_list[i].active = 0; 
    }

    //npc 1
    npc_list[0].active = 1;
    // Set the world coordinates (adjust these to fit your map!)
    npc_list[0].x = 150; 
    npc_list[0].y = 20;
    // Assign a valid metasprite array (using your player sprite as a placeholder)
    npc_list[0].sprite = npc; 
    // Assign a dialogue ID 
    npc_list[0].text_id = 1;

    //npc 2
    npc_list[1].active = 1;
    // Set the world coordinates (adjust these to fit your map!)
    npc_list[1].x = 450; 
    npc_list[1].y = 60;
    // Assign a valid metasprite array (using your player sprite as a placeholder)
    npc_list[1].sprite = npc; 
    // Assign a dialogue ID 
    npc_list[1].text_id = 2;

    //npc 3
    npc_list[2].active = 1;
    // Set the world coordinates (adjust these to fit your map!)
    npc_list[2].x = 20; 
    npc_list[2].y = 70;
    // Assign a valid metasprite array (using your player sprite as a placeholder)
    npc_list[2].sprite = npc; 
    // Assign a dialogue ID 
    npc_list[2].text_id = 3;
}

//checks if npc is within interact_range of the player in both x and y directions
unsigned char npc_in_range(unsigned char i, unsigned int player_x, unsigned char player_y) {
    unsigned int dx;
    unsigned char dy;

    if (player_x > npc_list[i].x) dx = player_x - npc_list[i].x;
    else dx = npc_list[i].x - player_x;

    if (player_y > npc_list[i].y) dy = player_y - npc_list[i].y;
    else dy = npc_list[i].y - player_y;

    return (dx < INTERACT_RANGE && dy < INTERACT_RANGE);
}

//writes the dialogue row to the correct nametable based on world column
void draw_dialogue_row(unsigned char screen_row, unsigned char world_col_start, const unsigned char *tiles) {
    unsigned char first_chunk;

    if (world_col_start >= 32) {
        vram_adr(NAMETABLE_B + ((unsigned int)screen_row * 32) + (world_col_start - 32));
        vram_write(tiles, 32);
        return;
    }

    first_chunk = 32 - world_col_start;
    vram_adr(NAMETABLE_A + ((unsigned int)screen_row * 32) + world_col_start);
    vram_write(tiles, first_chunk);

    if (first_chunk < 32) {
        vram_adr(NAMETABLE_B + ((unsigned int)screen_row * 32));
        vram_write(tiles + first_chunk, 32 - first_chunk);
    }
}


//lookup table
unsigned char char_to_tile(char c) {
    if (c == ' ') return DIALOGUE_BG_TILE;
    if (c >= 'A' && c <= 'Z') return FONT_TILE_BASE + (c - 'A');       // A-Z first
    if (c >= '0' && c <= '9') return FONT_TILE_BASE + 26 + (c - '0');  // then 0-9
    if (c == ',') return 0xDA;
    if (c == '.') return 0xD9;
    if (c == '!') return 0xDB;
    if (c == '?') return 0xD7;
    if (c == '"') return 0xD8;
    return DIALOGUE_BG_TILE; // unmapped char - falls back to blank
}

//generate the text in the dialogue row using ascii ordering
void build_text_row(unsigned char *buf, const char *text) {
    unsigned char i = 0;
    while (text[i] != 0 && i < 32) {
        buf[i] = char_to_tile(text[i]);
        i++;
    }
    while (i < 32) {
        buf[i] = DIALOGUE_BG_TILE;
        i++;
    }
}


//does exactly what it says it does
void draw_dialogue_box(unsigned char npc_index, unsigned int cam_x_snapped) {
    unsigned char world_col_start = (unsigned char)(cam_x_snapped >> 3);
    unsigned char i;
    unsigned char text_id = npc_list[npc_index].text_id;
    const char *line = (text_id < NUM_DIALOGUE_LINES) ? dialogue_text[text_id] : dialogue_text[0];

    for (i = 0; i < 32; i++) dialogue_row_buf[i] = DIALOGUE_BORDER_TILE;
    draw_dialogue_row(DIALOGUE_TOP_ROW, world_col_start, dialogue_row_buf);

    build_text_row(dialogue_row_buf, line);
    draw_dialogue_row(DIALOGUE_TOP_ROW + 1, world_col_start, dialogue_row_buf);

    for (i = 0; i < 32; i++) dialogue_row_buf[i] = DIALOGUE_BG_TILE;
    draw_dialogue_row(DIALOGUE_TOP_ROW + 2, world_col_start, dialogue_row_buf);

    for (i = 0; i < 32; i++) dialogue_row_buf[i] = DIALOGUE_BORDER_TILE;
    draw_dialogue_row(DIALOGUE_TOP_ROW + 3, world_col_start, dialogue_row_buf);
}


//take a wild guess
void erase_dialogue_box(unsigned int cam_x_snapped) {
    unsigned char world_col_start = (unsigned char)(cam_x_snapped >> 3);
    unsigned char row, i, world_col;
    unsigned int map_index;

    for (row = DIALOGUE_TOP_ROW; row < DIALOGUE_TOP_ROW + DIALOGUE_ROWS; row++) {
        for (i = 0; i < 32; i++) {
            world_col = world_col_start + i;
            if (world_col < 32) {
                map_index = ((unsigned int)row * 32) + world_col;
                dialogue_row_buf[i] = map_left3[map_index];
            } else {
                map_index = ((unsigned int)row * 32) + (world_col - 32);
                dialogue_row_buf[i] = map_right3[map_index];
            }
        }
        draw_dialogue_row(row, world_col_start, dialogue_row_buf);
    }
}


#define DIALOGUE_PALETTE 0  // which of your 4 BG sub-palettes (0-3) suits text - check your map_palette data and pick the right one

void set_dialogue_attr(unsigned char attr_row) {
    unsigned char buf[8];
    unsigned char i;
    unsigned char value = DIALOGUE_PALETTE | (DIALOGUE_PALETTE << 2) | (DIALOGUE_PALETTE << 4) | (DIALOGUE_PALETTE << 6);
    for (i = 0; i < 8; i++) buf[i] = value;

    vram_adr(NAMETABLE_A + 0x3C0 + (attr_row * 8));
    vram_write(buf, 8);
    vram_adr(NAMETABLE_B + 0x3C0 + (attr_row * 8));
    vram_write(buf, 8);
}

void restore_dialogue_attr(unsigned char attr_row) {
    unsigned int base = 960 + (attr_row * 8);
    vram_adr(NAMETABLE_A + 0x3C0 + (attr_row * 8));
    vram_write(map_left3 + base, 8);
    vram_adr(NAMETABLE_B + 0x3C0 + (attr_row * 8));
    vram_write(map_right3 + base, 8);
}


void main(void) {
    unsigned char oam_id = 0;
    unsigned char pad;
    unsigned int player_x = 100;
    unsigned char player_y = 210; 
    unsigned int cam_x = 0;
    unsigned char draw_x;

    unsigned char i;
    unsigned char draw_npc_x;

    unsigned char player_sprite_state = 0; // 0=front, 1=back, 2=right, 3=left

    ppu_off();
    oam_clear();

    show_loading_scr();
    fade_from_black();
    delay(300);
    fade_to_black();
    delay(30);
    
    set_chr_bank(1); // Set CHR bank 1 for the game graphics
    bank_spr(1);
    pal_spr(spr_palette);

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

    init_map_npcs(); // Set up the NPCs for this map

    while (1) {
    ppu_wait_nmi();

    // Flush any pending box draw/erase right after vblank starts - vram_adr()
    // touches $2006, which needs to happen off-screen or during vblank.
    if (dialogue_needs_open) {
        ppu_off(); // Turn off rendering while we draw the box
        draw_dialogue_box(current_npc, cam_x);
        set_dialogue_attr(DIALOGUE_TOP_ROW / 4);
        scroll(cam_x, 0);
        ppu_on_all(); // Turn rendering back on
        dialogue_needs_open = 0;
    }
    if (dialogue_needs_close) {
        ppu_off(); // Turn off rendering while we erase the box
        erase_dialogue_box(cam_x);
        restore_dialogue_attr(DIALOGUE_TOP_ROW / 4);
        scroll(cam_x, 0);
        ppu_on_all(); // Turn rendering back on
        dialogue_needs_close = 0;
    }

    oam_id = 0;
    pad = pad_poll(0);
    pad_new = pad & ~pad_old; // bits newly pressed this frame
    pad_old = pad;

    if (game_state == STATE_PLAYING) {
        move_player(pad, &player_x, &player_y, &player_sprite_state);

        if (player_y < 2)   player_y = 2;
        else if (player_y > 220) player_y = 220;

        update_camera(&player_x, &cam_x);
        scroll(cam_x, 0);

        if (pad_new & PAD_A) {
            for (i = 0; i < MAX_NPCS; i++) {
                if (npc_list[i].active && npc_in_range(i, player_x, player_y)) {
                    current_npc = i;
                    cam_x = cam_x & ~7; // tile-align so the box draws cleanly
                    dialogue_needs_open = 1;
                    game_state = STATE_DIALOGUE;
                    break;
                }
            }
        }
    } else { // STATE_DIALOGUE
        if (pad_new & PAD_A) {
            dialogue_needs_close = 1;
            game_state = STATE_PLAYING;
            current_npc = 0xFF;
        }
    }

    draw_x = (unsigned char)(player_x - cam_x);
    switch (player_sprite_state) {
        case 0: oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_front); break;
        case 1: oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_back);  break;
        case 2: oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_right); break;
        case 3: oam_id = oam_meta_spr(draw_x, player_y, oam_id, Player_left); break;
    }

    for (i = 0; i < MAX_NPCS; i++) {
        if (npc_list[i].active &&
            npc_list[i].x >= cam_x &&
            npc_list[i].x + 16 <= cam_x + SCREEN_W) {
            draw_npc_x = (unsigned char)(npc_list[i].x - cam_x);
            oam_id = oam_meta_spr(draw_npc_x, npc_list[i].y, oam_id, npc_list[i].sprite);
        }
    }

    oam_hide_rest(oam_id); // still removed per your call
}
}