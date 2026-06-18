#include "knob.h"
#include "i18n.h"
#include "device_license.h"
#include "arcmind_logo.h"
#include "dead.h"
#include "infect.h"
#include "driver/ledc.h"
#include "esp_random.h"
#include "esp32-hal-cpu.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LIFE_MIN -999
#define LIFE_MAX 999
#define DEFAULT_LIFE_TOTAL 40
#define DEFAULT_BRIGHTNESS_PERCENT 30
#define MULTIPLAYER_COUNT 4
#define PLAYER_PALETTE_COUNT 12
#define PLAYER_COLOR_COLS 4
#define POISON_MAX 10
#define KNOB_EVENT_QUEUE_SIZE 32

#define BACKLIGHT_PIN 47
#define BACKLIGHT_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_TIMER LEDC_TIMER_0
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_FREQ 5000
#define BACKLIGHT_LEDC_RES LEDC_TIMER_10_BIT
#define BACKLIGHT_DUTY_MAX 1023

typedef struct
{
    knob_event_t event;
    int cont;
} knob_input_event_t;

static int life_total = DEFAULT_LIFE_TOTAL;
static int brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;

// ---------- screens ----------
static lv_obj_t *screen_unregistered = NULL;
static lv_obj_t *screen_intro = NULL;
static lv_obj_t *screen_main = NULL;
static lv_obj_t *screen_settings = NULL;
static lv_obj_t *screen_multiplayer = NULL;
static lv_obj_t *screen_multiplayer_menu = NULL;
static lv_obj_t *screen_multiplayer_name = NULL;
static lv_obj_t *screen_multiplayer_color = NULL;
static lv_obj_t *screen_multiplayer_cmd_select = NULL;
static lv_obj_t *screen_multiplayer_cmd_damage = NULL;
static lv_obj_t *screen_multiplayer_all_damage = NULL;
static lv_obj_t *screen_before_menu     = NULL;
static lv_obj_t *screen_before_settings = NULL;

// ---------- main UI ----------
static lv_obj_t *intro_img = NULL;
static lv_obj_t *intro_label_name = NULL;
static lv_obj_t *life_container = NULL;
static lv_obj_t *life_hitbox = NULL;
static lv_obj_t *menu_overlay = NULL;
static lv_obj_t *menu_panel = NULL;
static lv_obj_t *menu_players_row = NULL;
static lv_obj_t *menu_players_name = NULL;
static lv_obj_t *menu_players_value = NULL;
static lv_obj_t *label_main_battery = NULL;
static lv_obj_t *arc_main_battery = NULL;
static lv_obj_t *label_main_battery_charge = NULL;
static lv_obj_t *label_main_battery_settings = NULL;
static lv_obj_t *label_multiplayer_battery = NULL;
static lv_obj_t *arc_multiplayer_battery = NULL;
static lv_obj_t *label_multiplayer_battery_charge = NULL;
static lv_obj_t *label_multiplayer_battery_settings = NULL;
static lv_obj_t *mp_battery_panel = NULL;
static lv_obj_t *menu_brightness_bar = NULL;
static lv_obj_t *menu_brightness_label = NULL;

// Commander damage — main screen (you vs P2, P3, P4)
static int cmd_main_damage[3] = {0, 0, 0};
static int cmd_main_selected = -1;
static int main_poison_selected = -1;

/* 1-player inner ring + center UI (matches multiplayer ring design) */
#define SP_RING_SLOTS       4
static const int16_t sp_slot_center_deg[SP_RING_SLOTS] = {248, 113, 158, 203};
static lv_obj_t *main_bg_panel = NULL;
static lv_obj_t *main_center_panel = NULL;
static lv_obj_t *main_battery_panel = NULL;
static lv_obj_t *sp_ring_seg[SP_RING_SLOTS];
static lv_obj_t *btn_sp_ring_hit[SP_RING_SLOTS];
static lv_obj_t *canvas_cmd_sp[3];
static uint8_t  *canvas_cmd_sp_buf[3];
static lv_obj_t *canvas_poison_sp = NULL;
static uint8_t  *canvas_poison_sp_buf = NULL;
static lv_obj_t *canvas_main_life = NULL;
static uint8_t  *canvas_main_life_buf = NULL;
static lv_obj_t *canvas_main_name = NULL;
static uint8_t  *canvas_main_name_buf = NULL;
static lv_obj_t *img_main_skull = NULL;
#define MAIN_LIFE_W       128
#define MAIN_LIFE_H       52
#define MAIN_LIFE_FONT_H  36
#define MAIN_LIFE_BUF_SIZE (MAIN_LIFE_W * MAIN_LIFE_H * 3)
#define MAIN_NAME_W       120
#define MAIN_NAME_H       26
#define MAIN_NAME_FONT_H  22
#define MAIN_NAME_BUF_SIZE (MAIN_NAME_W * MAIN_NAME_H * 3)
#define MAIN_CENTER_Y     16
#define MAIN_DELTA_X      82
#define SP_POISON_W       32
#define SP_POISON_H       44
#define SP_POISON_W_HALF  (SP_POISON_W / 2)
#define SP_POISON_H_HALF  (SP_POISON_H / 2)
#define SP_POISON_BUF_SIZE (SP_POISON_W * SP_POISON_H * 3)

// Commander damage — multiplayer screen (inline circles)
static lv_obj_t *cmd_mp_circle[MULTIPLAYER_COUNT][3];
static lv_obj_t *cmd_mp_label[MULTIPLAYER_COUNT][3];
static int cmd_mp_selected_victim = -1;
static int cmd_mp_selected_attacker = -1;

// 7-segment digits
static lv_obj_t *digit_hundreds[7];
static lv_obj_t *digit_tens[7];
static lv_obj_t *digit_ones[7];
static lv_obj_t *digit_sign[7];
static lv_obj_t *digit_sign_plus_vert = NULL;

static lv_obj_t *digit_box_sign = NULL;
static lv_obj_t *digit_box_hundreds = NULL;
static lv_obj_t *digit_box_tens = NULL;
static lv_obj_t *digit_box_ones = NULL;

// ---------- settings UI ----------
static lv_obj_t *label_settings_title = NULL;
static lv_obj_t *label_settings_battery = NULL;
static lv_obj_t *label_settings_battery_detail = NULL;
static lv_obj_t *btn_settings_back = NULL;
static lv_obj_t *btn_menu_reset = NULL;
static lv_obj_t *btn_menu_back = NULL;
static lv_obj_t *btn_menu_settings = NULL;
static lv_obj_t *btn_menu_select_first = NULL;
static lv_obj_t *btn_mp_menu_rename = NULL;
static lv_obj_t *btn_mp_menu_pick_color = NULL;
static lv_obj_t *btn_mp_menu_back = NULL;
static lv_obj_t *btn_mp_color_back = NULL;
static lv_obj_t *mp_menu_panel = NULL;
static lv_obj_t *label_multiplayer_color_title = NULL;
static lv_obj_t *mp_color_swatch[PLAYER_PALETTE_COUNT];
static lv_obj_t *mp_color_taken_mark[PLAYER_PALETTE_COUNT];
static lv_obj_t *btn_mp_name_save = NULL;
static lv_obj_t *btn_mp_name_back = NULL;
static lv_obj_t *btn_mp_cmd_select_back = NULL;
static lv_obj_t *btn_mp_cmd_damage_back = NULL;
static lv_obj_t *btn_mp_all_damage_apply = NULL;
static lv_obj_t *btn_mp_all_damage_back = NULL;

// ---------- multiplayer UI ----------
static lv_obj_t *multiplayer_quadrants[MULTIPLAYER_COUNT];
static lv_obj_t *multiplayer_round_segments[MULTIPLAYER_COUNT];
static lv_obj_t *round_wedge_hit[MULTIPLAYER_COUNT];
#define ROUND_WEDGE_BTN_SIZE 168
static const lv_coord_t ROUND_WEDGE_BTN_XY[MULTIPLAYER_COUNT][2] = {
    { 96, 192}, /* P0 bottom */
    {  0,  96}, /* P1 left */
    { 96,   0}, /* P2 top */
    {192,  96}, /* P3 right */
};
static lv_obj_t *label_multiplayer_life[MULTIPLAYER_COUNT];
static lv_obj_t *label_multiplayer_name[MULTIPLAYER_COUNT];
static lv_obj_t *label_multiplayer_menu_title = NULL;
static lv_obj_t *label_multiplayer_name_title = NULL;
static lv_obj_t *textarea_multiplayer_name = NULL;
static lv_obj_t *keyboard_multiplayer_name = NULL;
static lv_obj_t *label_multiplayer_cmd_select_title = NULL;
static lv_obj_t *button_multiplayer_cmd_target[MULTIPLAYER_COUNT - 1];
static lv_obj_t *label_multiplayer_cmd_target[MULTIPLAYER_COUNT - 1];
static lv_obj_t *label_multiplayer_cmd_damage_title = NULL;
static lv_obj_t *label_multiplayer_cmd_damage_value = NULL;
static lv_obj_t *label_multiplayer_cmd_damage_hint = NULL;
static lv_obj_t *label_multiplayer_all_damage_title = NULL;
static lv_obj_t *label_multiplayer_all_damage_value = NULL;
static lv_obj_t *label_multiplayer_all_damage_hint = NULL;

static lv_timer_t *intro_timer = NULL;
static int last_knob_cont = 0;
static bool knob_initialized = false;
static knob_input_event_t knob_event_queue[KNOB_EVENT_QUEUE_SIZE];
static volatile uint8_t knob_event_head = 0;
static volatile uint8_t knob_event_tail = 0;

static int battery_percent = -1;
static float battery_voltage = 0.0f;
static float battery_voltage_prev = 0.0f;
static uint32_t battery_sample_tick = 0;
static bool battery_sample_valid = false;
static bool battery_is_charging = false;
static int battery_percent_smoothed = -1;
static int multiplayer_life[MULTIPLAYER_COUNT] = {40, 40, 40, 40};
static int multiplayer_selected = 0;
static int multiplayer_menu_player = 0;
static int multiplayer_cmd_source = 0;
static int multiplayer_cmd_target = -1;
static int multiplayer_cmd_delta = 0;
static int multiplayer_cmd_target_choices[MULTIPLAYER_COUNT - 1] = {-1, -1, -1};
static int multiplayer_cmd_damage_totals[MULTIPLAYER_COUNT][MULTIPLAYER_COUNT] = {{0}};
static int multiplayer_all_damage_value = 0;
static bool multiplayer_swipe_tracking = false;
static lv_point_t multiplayer_swipe_start = {0, 0};
static char multiplayer_names[MULTIPLAYER_COUNT][16] = {"P1", "P2", "P3", "P4"};
static uint8_t multiplayer_color[MULTIPLAYER_COUNT] = {0, 1, 2, 3};
static int multiplayer_poison[MULTIPLAYER_COUNT];
static int mp_poison_selected = -1;

// ---------- life delta display ----------
static lv_obj_t *label_life_delta = NULL;
static int life_delta_acc = 0;
static int mp_delta_acc[MULTIPLAYER_COUNT] = {0, 0, 0, 0};
static lv_timer_t *life_delta_hide_timer = NULL;
static lv_timer_t *mp_delta_hide_timer = NULL;

// ---------- auto-dim ----------
#define AUTO_DIM_TIMEOUT_MS 30000
#define AUTO_DIM_BRIGHTNESS 5
#define UNDIM_GRACE_MS 150
#define CPU_FREQ_ACTIVE 240
#define CPU_FREQ_IDLE 80
static bool auto_dim_enabled = false;
static bool dimmed = false;
static bool settings_dirty = false;
static uint32_t last_activity_tick = 0;
static uint32_t undim_tick = 0;
static lv_timer_t *auto_dim_timer = NULL;

// ---------- settings fields ----------
static bool mirror_enabled = false;
static bool round_table_enabled = false;
static int  game_player_count = 4; /* 1, 2, or 4 */
static int  menu_focus = 0; /* 0=players, 1=brightness */
#define SETTINGS_FIELD_COUNT 6
#define SETTINGS_TIMER_FIELD_IDX 3
#define SETTINGS_TIMER_FIELD_VISIBLE 0
static int settings_selected_field = 0; // 0=brightness, 1=auto_dim, 2=mirror, 3=timer_dur, 4=table, 5=language
static lv_obj_t *settings_row[SETTINGS_FIELD_COUNT] = {NULL};
static lv_obj_t *settings_row_name[SETTINGS_FIELD_COUNT] = {NULL};
static lv_obj_t *settings_row_value[SETTINGS_FIELD_COUNT] = {NULL};
static const i18n_id_t settings_field_ids[SETTINGS_FIELD_COUNT] = {
    I18N_BRIGHTNESS, I18N_AUTO_DIM, I18N_MIRROR, I18N_TIMER, I18N_TABLE, I18N_LANGUAGE
};

// ---------- multiplayer timer duration ----------
static const uint16_t MP_TIMER_OPTIONS[] = {0, 30, 45, 60, 90, 120, 180};
#define MP_TIMER_OPTIONS_COUNT ((int)(sizeof(MP_TIMER_OPTIONS) / sizeof(MP_TIMER_OPTIONS[0])))
static int mp_timer_duration_idx = 0; // index into MP_TIMER_OPTIONS; 0 = disabled

// ---------- skull overlay (dead player indicator) ----------
static lv_obj_t *img_skull[MULTIPLAYER_COUNT];

// ---------- poison counter badges ----------
static lv_obj_t *btn_mp_ring_hit[MULTIPLAYER_COUNT][4];

// ---------- multiplayer turn timer ----------
typedef enum {
    MTIMER_OFF = 0,
    MTIMER_IDLE,
    MTIMER_SELECTING,
    MTIMER_SPINNING,
    MTIMER_WAITING,
    MTIMER_RUNNING,
    MTIMER_EXPIRED,
} mp_timer_state_t;

static mp_timer_state_t mp_timer_state         = MTIMER_OFF;
static int              mp_timer_remaining_ms  = 0;
static int              mp_timer_current_player = -1;
static int              mp_timer_select_player  = 0;
static bool             mp_timer_select_moved   = false;
static bool             mp_timer_blink_visible  = true;
static int              mp_spin_step            = 0;
static int              mp_spin_target          = -1;
static lv_obj_t        *arc_mp_timer            = NULL;
static lv_obj_t        *btn_mp_timer            = NULL;
static lv_obj_t        *label_mp_timer_btn      = NULL;
/* no separate outline objects — highlight is done via quadrant bg opacity */
static lv_timer_t      *mp_timer_tick           = NULL;
static lv_timer_t      *mp_timer_blink          = NULL;
static lv_timer_t      *mp_timer_spin_tmr       = NULL;

// ---------- mirror canvas (P0, P1 top players) ----------
// lv_img_set_angle() on a canvas uses per-scanline transform — no LVGL heap alloc.
// Style transform_angle on labels fails silently when the 48 KB LVGL heap is too
// small to allocate the required off-screen layer buffer.
#define CMP_LIFE_W 96
#define CMP_LIFE_H 44
#define CMP_NAME_W 64
#define CMP_NAME_H 18
/* LV_IMG_CF_TRUE_COLOR_ALPHA = 3 B/px (RGB565 + alpha) at LV_COLOR_DEPTH 16 */
#define CMP_LIFE_BUF_SIZE (CMP_LIFE_W * CMP_LIFE_H * 3)
#define CMP_NAME_BUF_SIZE (CMP_NAME_W * CMP_NAME_H * 3)
static lv_obj_t *canvas_mp_life[MULTIPLAYER_COUNT] = {NULL};
static lv_obj_t *canvas_mp_name[MULTIPLAYER_COUNT] = {NULL};
static uint8_t  *canvas_mp_life_buf[MULTIPLAYER_COUNT] = {NULL};
static uint8_t  *canvas_mp_name_buf[MULTIPLAYER_COUNT] = {NULL};

/* Single central delta canvas — shown at screen center, rotated to face the active player */
#define CMP_DELTA_W 100
#define CMP_DELTA_H 46
#define CMP_DELTA_BUF_SIZE (CMP_DELTA_W * CMP_DELTA_H * 3)
static lv_obj_t *canvas_mp_delta_ctr = NULL;
static uint8_t  *canvas_mp_delta_ctr_buf = NULL;
static int       mp_delta_player = -1;

#define CMP_CMD_W 30
#define CMP_CMD_H 30
#define CMP_CMD_BUF_SIZE (CMP_CMD_W * CMP_CMD_H * 3)
#define MP_RING_HIT_SIZE  36
#define MP_RING_HIT_HALF  (MP_RING_HIT_SIZE / 2)
#define MP_SLOT_INFECT    0
#define MP_RING_DIAM      336
#define MP_RING_WIDTH     34
#define MP_RING_MID_R     151
#define MP_RING_INNER_R   134
#define MP_RING_OUTER_R   168
/* Round table: outer ring at screen edge (same band as rect), content inboard. */
#define MP_RING_ROUND_DIAM    336
#define MP_RING_ROUND_WIDTH   30
#define MP_RING_ROUND_MID_R   151
#define MP_RING_ROUND_INNER_R 136
#define MP_RING_ROUND_OUTER_R 166
#define ROUND_HUB_R           30
#define ROUND_LIFE_R          88
#define ROUND_NAME_R          112
#define ROUND_SLICE_HALF_DEG  9
/* Round X-wedges (0=top CW): P0 bottom, P1 left, P2 top, P3 right. */
static const int16_t ROUND_WEDGE_START[MULTIPLAYER_COUNT] = {135, 225, 315, 45};
static const int16_t ROUND_WEDGE_END[MULTIPLAYER_COUNT]   = {225, 315, 45, 135};
static const int16_t ROUND_SPOKE_DEG[MULTIPLAYER_COUNT]   = {180, 270, 0, 90};
static const int16_t ROUND_BIN_OFF[4] = {11, 33, 55, 77};
static const int8_t  ROUND_SLOT_BIN[4] = {3, 0, 1, 2}; /* poison=bin3 (last toward wedge end) */
#define CMP_POISON_W      44
#define CMP_POISON_H      24
#define CMP_POISON_W_HALF (CMP_POISON_W / 2)
#define CMP_POISON_H_HALF (CMP_POISON_H / 2)
#define CMP_POISON_BUF_SIZE (CMP_POISON_W * CMP_POISON_H * 3)
#define MP_RING_ROTATION  270
#define SP_SLICE_HALF_DEG 22
#define SP_RING_DIAM      MP_RING_DIAM
#define SP_RING_WIDTH     MP_RING_WIDTH
#define SP_RING_MID_R     MP_RING_MID_R
/* attacker index mapping: mp_attackers[victim][slot] — mirrors the build-time array */
static const int cmp_mp_attackers[MULTIPLAYER_COUNT][3] = {
    {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}
};
static lv_obj_t *canvas_cmd_mp[MULTIPLAYER_COUNT][3];
static uint8_t  *canvas_cmd_mp_buf[MULTIPLAYER_COUNT][3];
static lv_obj_t *canvas_poison_mp[MULTIPLAYER_COUNT];
static uint8_t  *canvas_poison_mp_buf[MULTIPLAYER_COUNT];
static lv_obj_t *mp_ring_seg[MULTIPLAYER_COUNT][4];

typedef struct { lv_coord_t x; lv_coord_t y; } mp_slot_pos_t;
typedef struct { uint16_t start; uint16_t end; } mp_slot_arc_t;

/* Ring slice centers (deg, 0=top CW): slots 1-3=cmd from center line, slot0=poison last toward corner. */
#define MP2P_RING_SLOTS 2
#define MP2P_SLICE_HALF_DEG 24
#define MP2P_RING_WIDTH     38
#define MP2P_RING_HIT_SIZE  44
#define MP2P_RING_HIT_HALF  (MP2P_RING_HIT_SIZE / 2)
#define MP2P_SLICE_VALUE_FONT (&lv_font_montserrat_22)
#define MP2P_SLICE_VALUE_H    22
/* 2p: P1 top, P2 bottom — two large adjacent slices on each half's outer edge. */
static const int16_t rect_2p_slot_center_deg[2][MP2P_RING_SLOTS] = {
    { 22, 338}, /* top: poison last (CW), cmd inward */
    {202, 158}, /* bottom */
};
static const int16_t rect_slot_center_deg[MULTIPLAYER_COUNT][4] = {
    {281, 349, 326, 304},  /* P0 top-left */
    { 79,  11,  34,  56},  /* P1 top-right */
    {101, 169, 146, 124},  /* P2 bottom-right */
    {259, 191, 214, 236},  /* P3 bottom-left */
};
/* Rect ring slice centers — see rect_slot_center_deg above. */

static bool mp_point_in_ring_band(lv_coord_t x, lv_coord_t y);
static int16_t mp_point_to_deg(lv_coord_t x, lv_coord_t y);
static bool mp_is_2p(void);
static bool mp_2p_deg_in_half(int16_t deg, int player);
static bool mp_2p_deg_for_player(int16_t deg, int player);
static void refresh_menu_player_count_ui(void);
static void refresh_menu_focus_ui(void);
static void change_game_player_count(int dir);
static void apply_game_player_count(void);

static bool mp_deg_in_wedge(int16_t deg, int player)
{
    int16_t s, e;

    if (player < 0 || player >= MULTIPLAYER_COUNT) return false;
    s = ROUND_WEDGE_START[player];
    e = ROUND_WEDGE_END[player];
    if (s < e) return deg >= s && deg < e;
    return deg >= s || deg < e;
}

static int mp_round_player_from_deg(int16_t deg)
{
    int p;

    if (mp_is_2p()) {
        if (mp_2p_deg_for_player(deg, 1)) return 1;
        if (mp_2p_deg_for_player(deg, 2)) return 2;
        return -1;
    }
    for (p = 0; p < MULTIPLAYER_COUNT; p++) {
        if (mp_deg_in_wedge(deg, p)) return p;
    }
    return -1;
}

static int mp_round_player_from_point(lv_coord_t x, lv_coord_t y)
{
    int32_t dx = (int32_t)x - 180;
    int32_t dy = (int32_t)y - 180;
    uint32_t r2 = (uint32_t)(dx * dx + dy * dy);

    if (r2 < (uint32_t)(ROUND_HUB_R * ROUND_HUB_R)) return -1;
    if (mp_point_in_ring_band(x, y)) return -1;
    if (mp_is_2p()) {
        if (y < 180) return 1;
        if (y >= 180) return 2;
        return -1;
    }
    return mp_round_player_from_deg(mp_point_to_deg(x, y));
}

static int16_t round_slot_center(int player, int slot)
{
    int16_t ws, deg;

    if (player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot > 3) return 0;
    ws = ROUND_WEDGE_START[player];
    deg = (int16_t)(ws + ROUND_BIN_OFF[ROUND_SLOT_BIN[slot]]);
    while (deg >= 360) deg = (int16_t)(deg - 360);
    return deg;
}

static void round_content_offset(int player, bool name, int16_t *ox, int16_t *oy)
{
    double rad;
    lv_coord_t r;

    if (ox == NULL || oy == NULL || player < 0 || player >= MULTIPLAYER_COUNT) return;
    r = name ? ROUND_NAME_R : ROUND_LIFE_R;
    rad = (double)ROUND_SPOKE_DEG[player] * 3.141592653589793 / 180.0;
    *ox = (int16_t)((double)r * sin(rad) + 0.5);
    *oy = (int16_t)(-(double)r * cos(rad) + 0.5);
}

static lv_coord_t mp_ring_diam(void)
{
    return round_table_enabled ? MP_RING_ROUND_DIAM : MP_RING_DIAM;
}

static lv_coord_t mp_ring_width(void)
{
    if (mp_is_2p()) return MP2P_RING_WIDTH;
    return round_table_enabled ? MP_RING_ROUND_WIDTH : MP_RING_WIDTH;
}

static lv_coord_t mp_ring_mid_r(void)
{
    return round_table_enabled ? MP_RING_ROUND_MID_R : MP_RING_MID_R;
}

static lv_coord_t mp_ring_inner_r(void)
{
    return round_table_enabled ? MP_RING_ROUND_INNER_R : MP_RING_INNER_R;
}

static lv_coord_t mp_ring_outer_r(void)
{
    return round_table_enabled ? MP_RING_ROUND_OUTER_R : MP_RING_OUTER_R;
}

static bool mp_is_2p(void)
{
    return game_player_count == 2;
}

static int mp_ring_slots(void)
{
    return mp_is_2p() ? MP2P_RING_SLOTS : 4;
}

static bool mp_player_active(int player)
{
    if (player < 0 || player >= MULTIPLAYER_COUNT) return false;
    if (game_player_count == 4) return true;
    if (game_player_count == 2) return player == 1 || player == 2;
    return false;
}

static int mp2p_opponent(int player)
{
    return (player == 1) ? 2 : 1;
}

static int mp_cmd_attacker_for_slot(int victim, int slot)
{
    if (mp_is_2p() && slot == 1) return mp2p_opponent(victim);
    if (slot < 1 || slot > 3) return 0;
    return cmp_mp_attackers[victim][slot - 1];
}

static bool mp_use_canvas_life_name(int player)
{
    if (mp_is_2p()) return player == 1;
    if (round_table_enabled) return true;
    return player < 2;
}

static bool mp_2p_top_player(int player)
{
    return player == 1;
}

static bool mp_2p_deg_for_player(int16_t deg, int player)
{
    if (mp_2p_top_player(player)) return deg >= 270 || deg < 90;
    return deg >= 90 && deg < 270;
}

static void mp_slot_center_deg(int player, int slot, int16_t *deg_out)
{
    if (deg_out == NULL || player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot >= mp_ring_slots()) return;
    if (mp_is_2p()) {
        int idx = (player == 1) ? 0 : 1;
        *deg_out = rect_2p_slot_center_deg[idx][slot];
        return;
    }
    if (slot > 3) return;
    *deg_out = round_table_enabled ? round_slot_center(player, slot)
                                   : rect_slot_center_deg[player][slot];
}

static void mp_slot_arc_from_center(int16_t center, mp_slot_arc_t *out)
{
    int16_t s, e, half;

    if (out == NULL) return;
    if (mp_is_2p()) half = MP2P_SLICE_HALF_DEG;
    else half = round_table_enabled ? ROUND_SLICE_HALF_DEG : 10;
    s = (int16_t)(center - half);
    e = (int16_t)(center + half);
    if (s < 0) s = (int16_t)(s + 360);
    if (e > 360) e = 360;
    out->start = (uint16_t)s;
    out->end = (uint16_t)e;
}

static void get_mp_counter_slot_pos(int player, int slot, mp_slot_pos_t *out)
{
    int16_t deg;
    double rad;
    lv_coord_t half;
    if (out == NULL || player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot >= mp_ring_slots()) return;
    mp_slot_center_deg(player, slot, &deg);
    rad = (double)deg * 3.141592653589793 / 180.0;
    if (slot == MP_SLOT_INFECT) {
        out->x = (lv_coord_t)(180.0 + (double)mp_ring_mid_r() * sin(rad) - (double)CMP_POISON_W_HALF + 0.5);
        out->y = (lv_coord_t)(180.0 - (double)mp_ring_mid_r() * cos(rad) - (double)CMP_POISON_H_HALF + 0.5);
        return;
    }
    half = 15;
    out->x = (lv_coord_t)(180.0 + (double)mp_ring_mid_r() * sin(rad) - (double)half + 0.5);
    out->y = (lv_coord_t)(180.0 - (double)mp_ring_mid_r() * cos(rad) - (double)half + 0.5);
}

static void get_mp_counter_slot_arc(int player, int slot, mp_slot_arc_t *out)
{
    int16_t deg;
    if (out == NULL || player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot >= mp_ring_slots()) return;
    mp_slot_center_deg(player, slot, &deg);
    mp_slot_arc_from_center(deg, out);
}

static int16_t mp_point_to_deg(lv_coord_t x, lv_coord_t y)
{
    int32_t dx = (int32_t)x - 180;
    int32_t dy = 180 - (int32_t)y;
    double rad = atan2((double)dx, (double)dy);
    int deg = (int)(rad * 180.0 / 3.141592653589793);
    if (deg < 0) deg += 360;
    return (int16_t)deg;
}

static bool mp_deg_in_arc(int16_t deg, uint16_t start, uint16_t end)
{
    if (start <= end) return deg >= (int16_t)start && deg <= (int16_t)end;
    return deg >= (int16_t)start || deg <= (int16_t)end;
}

static bool mp_point_in_ring_band(lv_coord_t x, lv_coord_t y)
{
    int32_t dx = (int32_t)x - 180;
    int32_t dy = (int32_t)y - 180;
    uint32_t r2 = (uint32_t)(dx * dx + dy * dy);
    return r2 >= (uint32_t)(mp_ring_inner_r() * mp_ring_inner_r()) &&
           r2 <= (uint32_t)(mp_ring_outer_r() * mp_ring_outer_r());
}

static int16_t mp_angular_dist(int16_t a, int16_t b)
{
    int16_t d = (int16_t)(a - b);
    if (d < 0) d = (int16_t)(-d);
    if (d > 180) d = (int16_t)(360 - d);
    return d;
}

static bool mp_resolve_ring_click(lv_coord_t x, lv_coord_t y, int *player, int *slot)
{
    int16_t deg;
    int p, s;
    int best_p = -1;
    int best_s = -1;
    int16_t best_dist = 360;

    if (player == NULL || slot == NULL) return false;
    if (!mp_point_in_ring_band(x, y)) return false;

    deg = mp_point_to_deg(x, y);
    if (round_table_enabled) {
        int rp = mp_round_player_from_deg(deg);
        if (rp < 0) return false;
        for (s = 0; s < mp_ring_slots(); s++) {
            int16_t center, dist;
            mp_slot_center_deg(rp, s, &center);
            dist = mp_angular_dist(deg, center);
            if (dist < best_dist) {
                best_dist = dist;
                best_p = rp;
                best_s = s;
            }
        }
    } else {
        for (p = 0; p < MULTIPLAYER_COUNT; p++) {
            if (!mp_player_active(p)) continue;
            for (s = 0; s < mp_ring_slots(); s++) {
                int16_t center, dist;
                mp_slot_center_deg(p, s, &center);
                dist = mp_angular_dist(deg, center);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_p = p;
                    best_s = s;
                }
            }
        }
    }
    if (best_p < 0 || best_dist > (round_table_enabled ? 10 : 14)) return false;
    *player = best_p;
    *slot = best_s;
    return true;
}

static void mp_ring_hit_pos(int player, int slot, mp_slot_pos_t *out)
{
    mp_slot_pos_t pos;
    lv_coord_t vhalf;

    if (out == NULL || player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot >= mp_ring_slots()) return;
    get_mp_counter_slot_pos(player, slot, &pos);
    if (slot == MP_SLOT_INFECT) {
        out->x = pos.x + CMP_POISON_W_HALF - (mp_is_2p() ? MP2P_RING_HIT_HALF : MP_RING_HIT_HALF);
        out->y = pos.y + CMP_POISON_H_HALF - (mp_is_2p() ? MP2P_RING_HIT_HALF : MP_RING_HIT_HALF);
        return;
    }
    vhalf = 15;
    out->x = pos.x + vhalf - (mp_is_2p() ? MP2P_RING_HIT_HALF : MP_RING_HIT_HALF);
    out->y = pos.y + vhalf - (mp_is_2p() ? MP2P_RING_HIT_HALF : MP_RING_HIT_HALF);
}

static void layout_mp_ring_segment(int player, int slot)
{
    mp_slot_arc_t arc_ang;
    lv_obj_t *seg;

    if (player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot >= mp_ring_slots()) return;
    seg = mp_ring_seg[player][slot];
    if (seg == NULL) return;
    get_mp_counter_slot_arc(player, slot, &arc_ang);
    lv_arc_set_bg_angles(seg, arc_ang.start, arc_ang.end);
}

static lv_color_t get_player_active_color(int index);
static void mp_handle_ring_slice(int player, int slot);
static void event_mp_ring_slice(lv_event_t *e);

static int16_t mp_facing_angle(int player)
{
    if (player < 0 || player >= MULTIPLAYER_COUNT) return 0;
    if (mp_is_2p()) {
        if (player == 1 && mirror_enabled) return 1800;
        return 0;
    }
    if (round_table_enabled) {
        if (!mirror_enabled) return 0;
        static const int16_t round_ang[MULTIPLAYER_COUNT] = {0, 900, 1800, 2700};
        return round_ang[player];
    }
    if (player < 2 && mirror_enabled) return 1800;
    return 0;
}

static int16_t mp_slot_label_angle(int player)
{
    return mp_facing_angle(player);
}

static void layout_mp_counter_slots(int player)
{
    mp_slot_pos_t pos;
    int a, s;

    if (player < 0 || player >= MULTIPLAYER_COUNT) return;
    for (s = 0; s < mp_ring_slots(); s++) {
        if (mp_ring_seg[player][s] != NULL) {
            lv_obj_set_size(mp_ring_seg[player][s], mp_ring_diam(), mp_ring_diam());
            lv_obj_set_style_arc_width(mp_ring_seg[player][s], mp_ring_width(), LV_PART_MAIN);
        }
        layout_mp_ring_segment(player, s);
    }
    for (s = 0; s < 4; s++) {
        mp_ring_hit_pos(player, s, &pos);
        if (btn_mp_ring_hit[player][s] != NULL) {
            if (s < mp_ring_slots()) {
                lv_coord_t hit_sz = mp_is_2p() ? MP2P_RING_HIT_SIZE : MP_RING_HIT_SIZE;
                lv_obj_set_size(btn_mp_ring_hit[player][s], hit_sz, hit_sz);
                lv_obj_set_pos(btn_mp_ring_hit[player][s], pos.x, pos.y);
                lv_obj_clear_flag(btn_mp_ring_hit[player][s], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    get_mp_counter_slot_pos(player, MP_SLOT_INFECT, &pos);
    if (canvas_poison_mp[player] != NULL) {
        lv_obj_set_pos(canvas_poison_mp[player], pos.x, pos.y);
        lv_img_set_angle(canvas_poison_mp[player], mp_slot_label_angle(player));
    }
    for (a = 0; a < 3; a++) {
        get_mp_counter_slot_pos(player, a + 1, &pos);
        if (canvas_cmd_mp[player][a] != NULL)
            lv_obj_set_pos(canvas_cmd_mp[player][a], pos.x, pos.y);
    }
}

static void sp_slot_arc_from_center(int16_t center, mp_slot_arc_t *out)
{
    int16_t s, e;

    if (out == NULL) return;
    s = (int16_t)(center - SP_SLICE_HALF_DEG);
    e = (int16_t)(center + SP_SLICE_HALF_DEG);
    if (s < 0) s = (int16_t)(s + 360);
    if (e > 360) e = 360;
    out->start = (uint16_t)s;
    out->end = (uint16_t)e;
}

static void sp_get_counter_slot_pos(int slot, mp_slot_pos_t *out)
{
    int16_t deg;
    double rad;
    lv_coord_t half;

    if (out == NULL || slot < 0 || slot >= SP_RING_SLOTS) return;
    deg = sp_slot_center_deg[slot];
    rad = (double)deg * 3.141592653589793 / 180.0;
    if (slot == MP_SLOT_INFECT) {
        out->x = (lv_coord_t)(180.0 + (double)SP_RING_MID_R * sin(rad) - (double)SP_POISON_W_HALF + 0.5);
        out->y = (lv_coord_t)(180.0 - (double)SP_RING_MID_R * cos(rad) - (double)SP_POISON_H_HALF + 0.5);
        return;
    }
    half = 15;
    out->x = (lv_coord_t)(180.0 + (double)SP_RING_MID_R * sin(rad) - (double)half + 0.5);
    out->y = (lv_coord_t)(180.0 - (double)SP_RING_MID_R * cos(rad) - (double)half + 0.5);
}

static void sp_ring_hit_pos(int slot, mp_slot_pos_t *out)
{
    mp_slot_pos_t pos;
    lv_coord_t vhalf;

    if (out == NULL || slot < 0 || slot >= SP_RING_SLOTS) return;
    sp_get_counter_slot_pos(slot, &pos);
    if (slot == MP_SLOT_INFECT) {
        out->x = pos.x + SP_POISON_W_HALF - MP_RING_HIT_HALF;
        out->y = pos.y + SP_POISON_H_HALF - MP_RING_HIT_HALF;
        return;
    }
    vhalf = 15;
    out->x = pos.x + vhalf - MP_RING_HIT_HALF;
    out->y = pos.y + vhalf - MP_RING_HIT_HALF;
}

static int sp_cmd_attacker_for_slot(int slot)
{
    if (slot < 1 || slot > 3) return 0;
    return slot;
}

static void sp_layout_ring_segment(int slot)
{
    mp_slot_arc_t arc_ang;

    if (slot < 0 || slot >= SP_RING_SLOTS || sp_ring_seg[slot] == NULL) return;
    sp_slot_arc_from_center(sp_slot_center_deg[slot], &arc_ang);
    lv_arc_set_bg_angles(sp_ring_seg[slot], arc_ang.start, arc_ang.end);
}

static void sp_layout_counter_slots(void)
{
    mp_slot_pos_t pos;
    int a, s;

    for (s = 0; s < SP_RING_SLOTS; s++) {
        if (sp_ring_seg[s] != NULL) {
            lv_obj_set_size(sp_ring_seg[s], SP_RING_DIAM, SP_RING_DIAM);
            lv_obj_set_style_arc_width(sp_ring_seg[s], SP_RING_WIDTH, LV_PART_MAIN);
        }
        sp_layout_ring_segment(s);
        sp_ring_hit_pos(s, &pos);
        if (btn_sp_ring_hit[s] != NULL) {
            lv_obj_set_size(btn_sp_ring_hit[s], MP_RING_HIT_SIZE, MP_RING_HIT_SIZE);
            lv_obj_set_pos(btn_sp_ring_hit[s], pos.x, pos.y);
        }
    }
    sp_get_counter_slot_pos(MP_SLOT_INFECT, &pos);
    if (canvas_poison_sp != NULL)
        lv_obj_set_pos(canvas_poison_sp, pos.x, pos.y);
    for (a = 0; a < 3; a++) {
        sp_get_counter_slot_pos(a + 1, &pos);
        if (canvas_cmd_sp[a] != NULL)
            lv_obj_set_pos(canvas_cmd_sp[a], pos.x, pos.y);
    }
}

static void sp_set_ring_seg_highlight(int slot, bool selected)
{
    mp_slot_arc_t arc_ang;
    lv_obj_t *seg;
    lv_color_t base_color;

    if (slot < 0 || slot >= SP_RING_SLOTS) return;
    seg = sp_ring_seg[slot];
    if (seg == NULL) return;

    sp_slot_arc_from_center(sp_slot_center_deg[slot], &arc_ang);
    lv_arc_set_bg_angles(seg, arc_ang.start, arc_ang.end);

    base_color = (slot == MP_SLOT_INFECT)
        ? lv_color_black()
        : get_player_active_color(sp_cmd_attacker_for_slot(slot));
    lv_obj_set_style_arc_color(seg, base_color, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(seg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(seg, SP_RING_WIDTH, LV_PART_MAIN);

    if (selected) {
        lv_obj_set_style_arc_width(seg, SP_RING_WIDTH + 5, LV_PART_MAIN);
        if (slot == MP_SLOT_INFECT) {
            lv_arc_set_angles(seg, arc_ang.start, arc_ang.end);
            lv_obj_set_style_arc_color(seg, lv_color_hex(0x34D399), LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(seg, LV_OPA_80, LV_PART_INDICATOR);
            lv_obj_set_style_arc_width(seg, SP_RING_WIDTH + 8, LV_PART_INDICATOR);
            lv_obj_set_style_arc_rounded(seg, false, LV_PART_INDICATOR);
        }
    } else {
        lv_obj_set_style_arc_width(seg, SP_RING_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_arc_width(seg, 0, LV_PART_INDICATOR);
    }
}

static bool main_is_dead(void)
{
    return life_total <= 0 || multiplayer_poison[0] >= POISON_MAX;
}

static void refresh_main_poison_canvas(void)
{
    char pbuf[4];
    lv_draw_img_dsc_t img_dsc;
    lv_draw_label_dsc_t dsc;

    if (canvas_poison_sp == NULL || canvas_poison_sp_buf == NULL) return;

    snprintf(pbuf, sizeof(pbuf), "%d", multiplayer_poison[0]);
    memset(canvas_poison_sp_buf, 0, SP_POISON_BUF_SIZE);

    lv_draw_img_dsc_init(&img_dsc);
    lv_canvas_draw_img(canvas_poison_sp, (SP_POISON_W - 24) / 2, 0, &infect_img, &img_dsc);

    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_white();
    dsc.font  = &lv_font_montserrat_14;
    dsc.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(canvas_poison_sp, 0, 26, SP_POISON_W, &dsc, pbuf);
}

static void sp_handle_ring_slice(int slot);
static void event_sp_ring_slice(lv_event_t *e);
static void event_main_center_menu(lv_event_t *e);
static void event_main_battery_menu(lv_event_t *e);
static void back_from_player_menu(void);

static void set_mp_ring_seg_highlight(int player, int slot, bool selected)
{
    mp_slot_arc_t arc_ang;
    lv_obj_t *seg;
    lv_color_t base_color;

    if (player < 0 || player >= MULTIPLAYER_COUNT || slot < 0 || slot >= mp_ring_slots()) return;
    seg = mp_ring_seg[player][slot];
    if (seg == NULL) return;

    get_mp_counter_slot_arc(player, slot, &arc_ang);
    lv_arc_set_bg_angles(seg, arc_ang.start, arc_ang.end);

    base_color = (slot == MP_SLOT_INFECT)
        ? lv_color_black()
        : get_player_active_color(mp_cmd_attacker_for_slot(player, slot));
    lv_obj_set_style_arc_color(seg, base_color, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(seg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(seg, mp_ring_width(), LV_PART_MAIN);

    if (selected) {
        lv_obj_set_style_arc_width(seg, mp_ring_width() + 5, LV_PART_MAIN);
        if (slot == MP_SLOT_INFECT) {
            lv_arc_set_angles(seg, arc_ang.start, arc_ang.end);
            lv_obj_set_style_arc_color(seg, lv_color_hex(0x34D399), LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(seg, LV_OPA_80, LV_PART_INDICATOR);
            lv_obj_set_style_arc_width(seg, mp_ring_width() + 8, LV_PART_INDICATOR);
            lv_obj_set_style_arc_rounded(seg, false, LV_PART_INDICATOR);
        }
    } else {
        lv_obj_set_style_arc_width(seg, mp_ring_width(), LV_PART_MAIN);
        lv_obj_set_style_arc_width(seg, 0, LV_PART_INDICATOR);
    }
}

static const float battery_curve_voltages[] = {
    3.35f, 3.55f, 3.68f, 3.74f, 3.80f, 3.88f, 3.96f, 4.06f, 4.18f
};
static const int battery_curve_percentages[] = {0, 5, 12, 22, 34, 48, 64, 82, 100};

static void back_to_main(void);
static void open_multiplayer_screen(void);
static void goto_state(mp_timer_state_t new_state);
static void mp_timer_update_highlight(void);
static void mp_timer_refresh_btn_label(void);
static void players_persist_load(void);
static void players_persist_save(void);

// ----------------------------------------------------
// 7-segment map
// segment order:
// 0 top
// 1 upper-left
// 2 upper-right
// 3 middle
// 4 lower-left
// 5 lower-right
// 6 bottom
// ----------------------------------------------------

static const uint8_t seg_map[10][7] = {
    {1,1,1,0,1,1,1}, // 0
    {0,0,1,0,0,1,0}, // 1
    {1,0,1,1,1,0,1}, // 2
    {1,0,1,1,0,1,1}, // 3
    {0,1,1,1,0,1,0}, // 4
    {1,1,0,1,0,1,1}, // 5
    {1,1,0,1,1,1,1}, // 6
    {1,0,1,0,0,1,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}  // 9
};

static lv_color_t get_life_color(int value)
{
    if (value > 40)  return lv_color_hex(0x10B981); // ArcMind success green
    if (value >= 30) return lv_color_hex(0x8B5CF6); // ArcMind primary violet
    if (value >= 11) return lv_color_hex(0x22D3EE); // ArcMind cyan
    return lv_color_hex(0xEF4444);                  // ArcMind error red
}

static int clamp_life(int value)
{
    if (value < LIFE_MIN) return LIFE_MIN;
    if (value > LIFE_MAX) return LIFE_MAX;
    return value;
}

static int clamp_brightness(int value)
{
    if (value < 5) return 5;
    if (value > 100) return 100;
    return value;
}

static int clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

static void refresh_battery_ui(void);
static int battery_percent_from_voltage(float voltage);
static void update_battery_measurement(bool force);
static bool battery_charging_indicated(void);

#define BATT_ARC_SIZE   358
#define BATT_ARC_START  243
#define BATT_ARC_END    297
#define BATT_ARC_WIDTH  4
/* Li-ion reads ~0.2V high under charge; subtract before SOC lookup */
#define BATTERY_CHARGE_V_BIAS 0.22f
#define BATTERY_CHARGE_PCT_CAP 99
#define BATTERY_CHARGE_PCT_MAX_STEP 4

static bool battery_charging_indicated(void)
{
    if (knob_usb_power_present()) return true;
    if (!battery_sample_valid) return false;
    return battery_is_charging;
}

static void main_battery_hub_icons_apply(bool hub_visible)
{
    bool charging = battery_charging_indicated();

    if (label_main_battery != NULL) {
        if (hub_visible) lv_obj_clear_flag(label_main_battery, LV_OBJ_FLAG_HIDDEN);
        else              lv_obj_add_flag(label_main_battery, LV_OBJ_FLAG_HIDDEN);
    }
    if (label_main_battery_charge != NULL) {
        if (hub_visible && charging) {
            lv_obj_clear_flag(label_main_battery_charge, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_opa(label_main_battery_charge, LV_OPA_COVER, 0);
        } else {
            lv_obj_add_flag(label_main_battery_charge, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (label_main_battery_settings != NULL) {
        if (hub_visible && !charging) {
            lv_obj_clear_flag(label_main_battery_settings, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(label_main_battery_settings, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void main_battery_pct_set_visible(bool visible)
{
    main_battery_hub_icons_apply(visible);
}

static bool main_delta_is_visible(void)
{
    return label_life_delta != NULL &&
           !lv_obj_has_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
}

static int battery_percent_from_voltage(float voltage)
{
    size_t i;

    if (voltage <= battery_curve_voltages[0]) return 0;
    for (i = 1; i < (sizeof(battery_curve_voltages) / sizeof(battery_curve_voltages[0])); i++) {
        if (voltage <= battery_curve_voltages[i]) {
            float low_v = battery_curve_voltages[i - 1];
            float high_v = battery_curve_voltages[i];
            int low_p = battery_curve_percentages[i - 1];
            int high_p = battery_curve_percentages[i];
            float ratio = (voltage - low_v) / (high_v - low_v);
            return clamp_percent((int)(low_p + ((high_p - low_p) * ratio) + 0.5f));
        }
    }

    return 100;
}

#define CENTER_BATT_SIZE  64
#define CENTER_BATT_WIDTH 5

static lv_color_t battery_indicator_color(int pct)
{
    if (pct < 0) return lv_color_hex(0x4A4060);
    if (pct <= 20) return lv_color_hex(0xEF4444);
    if (pct <= 50) return lv_color_hex(0xFACC15);
    return lv_color_hex(0x8B5CF6);
}

#define BATT_HUB_PCT_Y     -8
#define BATT_HUB_ICON_Y    12

static void mp_battery_hub_icons_apply(bool hub_visible)
{
    bool charging = battery_charging_indicated();

    if (label_multiplayer_battery != NULL) {
        if (hub_visible) lv_obj_clear_flag(label_multiplayer_battery, LV_OBJ_FLAG_HIDDEN);
        else              lv_obj_add_flag(label_multiplayer_battery, LV_OBJ_FLAG_HIDDEN);
    }
    if (label_multiplayer_battery_charge != NULL) {
        if (hub_visible && charging) {
            lv_obj_clear_flag(label_multiplayer_battery_charge, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_opa(label_multiplayer_battery_charge, LV_OPA_COVER, 0);
        } else {
            lv_obj_add_flag(label_multiplayer_battery_charge, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (label_multiplayer_battery_settings != NULL) {
        if (hub_visible && !charging) {
            lv_obj_clear_flag(label_multiplayer_battery_settings, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(label_multiplayer_battery_settings, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void mp_battery_pct_label_set_visible(bool visible)
{
    mp_battery_hub_icons_apply(visible);
}

static void main_screen_raise_layers(void)
{
    int s, a;

    for (s = 0; s < SP_RING_SLOTS; s++) {
        if (sp_ring_seg[s] != NULL)
            lv_obj_move_foreground(sp_ring_seg[s]);
    }
    if (canvas_poison_sp != NULL &&
        !lv_obj_has_flag(canvas_poison_sp, LV_OBJ_FLAG_HIDDEN))
        lv_obj_move_foreground(canvas_poison_sp);
    for (a = 0; a < 3; a++) {
        if (canvas_cmd_sp[a] != NULL &&
            !lv_obj_has_flag(canvas_cmd_sp[a], LV_OBJ_FLAG_HIDDEN))
            lv_obj_move_foreground(canvas_cmd_sp[a]);
    }
    if (canvas_main_life != NULL)
        lv_obj_move_foreground(canvas_main_life);
    if (canvas_main_name != NULL)
        lv_obj_move_foreground(canvas_main_name);
    if (img_main_skull != NULL &&
        !lv_obj_has_flag(img_main_skull, LV_OBJ_FLAG_HIDDEN))
        lv_obj_move_foreground(img_main_skull);
    if (main_battery_panel != NULL)
        lv_obj_move_foreground(main_battery_panel);
    if (label_life_delta != NULL &&
        !lv_obj_has_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN))
        lv_obj_move_foreground(label_life_delta);
    for (s = 0; s < SP_RING_SLOTS; s++) {
        if (btn_sp_ring_hit[s] != NULL &&
            !lv_obj_has_flag(btn_sp_ring_hit[s], LV_OBJ_FLAG_HIDDEN))
            lv_obj_move_foreground(btn_sp_ring_hit[s]);
    }
    if (main_center_panel != NULL)
        lv_obj_move_foreground(main_center_panel);
}

static bool mp_delta_is_visible(void)
{
    return canvas_mp_delta_ctr != NULL &&
           !lv_obj_has_flag(canvas_mp_delta_ctr, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *make_battery_arc(lv_obj_t *parent)
{
    lv_obj_t *arc = lv_arc_create(parent);

    lv_obj_set_size(arc, BATT_ARC_SIZE, BATT_ARC_SIZE);
    lv_obj_center(arc);
    lv_arc_set_bg_angles(arc, BATT_ARC_START, BATT_ARC_END);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, BATT_ARC_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, BATT_ARC_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1E1B3A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x4A4060), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_70, LV_PART_MAIN);
    return arc;
}

static lv_obj_t *make_center_battery_arc(lv_obj_t *parent)
{
    lv_obj_t *arc = lv_arc_create(parent);

    lv_obj_set_size(arc, CENTER_BATT_SIZE, CENTER_BATT_SIZE);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, CENTER_BATT_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, CENTER_BATT_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1E1B3A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x4A4060), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_80, LV_PART_MAIN);
    return arc;
}

static lv_obj_t *make_battery_charge_icon(lv_obj_t *parent, lv_obj_t *percent_label)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFACC15), 0);
    lv_obj_align_to(label, percent_label, LV_ALIGN_OUT_RIGHT_MID, 3, 0);
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static void battery_layout_apply(lv_obj_t *label, lv_obj_t *charge, bool charging, bool center)
{
    if (charge == NULL || label == NULL) return;
    if (charging) {
        lv_obj_clear_flag(charge, LV_OBJ_FLAG_HIDDEN);
        if (center) {
            lv_obj_align_to(charge, label, LV_ALIGN_OUT_BOTTOM_MID, 0, -1);
        } else {
            lv_obj_align_to(charge, label, LV_ALIGN_OUT_RIGHT_MID, 3, 0);
        }
    } else {
        lv_obj_add_flag(charge, LV_OBJ_FLAG_HIDDEN);
    }
}

static void battery_widgets_to_front(lv_obj_t *arc, lv_obj_t *label, lv_obj_t *charge)
{
    if (arc != NULL) lv_obj_move_foreground(arc);
    if (label != NULL) lv_obj_move_foreground(label);
    if (charge != NULL) lv_obj_move_foreground(charge);
}

static void update_battery_charging_state(void)
{
    if (battery_voltage_prev > 0.0f) {
        if (battery_voltage > battery_voltage_prev + 0.012f) {
            battery_is_charging = true;
        } else if (battery_voltage < battery_voltage_prev - 0.010f) {
            battery_is_charging = false;
        }
    }

    battery_voltage_prev = battery_voltage;
}

static void set_battery_charge_visible(lv_obj_t *label, bool visible)
{
    if (label == NULL) return;
    if (visible) lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
}

void knob_update_battery(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = lv_tick_get();

    if (last_tick != 0 && lv_tick_elaps(last_tick) < 5000U) {
        return;
    }

    last_tick = now;
    refresh_battery_ui();
}

static void update_battery_measurement(bool force)
{
    if (!force && battery_sample_valid && (lv_tick_elaps(battery_sample_tick) < 5000)) {
        return;
    }

    battery_voltage = knob_read_battery_voltage();
    battery_sample_tick = lv_tick_get();
    battery_sample_valid = (battery_voltage > 0.1f);
    if (battery_sample_valid) {
        update_battery_charging_state();
    } else {
        battery_is_charging = false;
    }
}

static int read_battery_percent(void)
{
    float soc_voltage;
    int pct;
    bool charging;

    update_battery_measurement(false);
    if (!battery_sample_valid) {
        battery_percent_smoothed = -1;
        return -1;
    }

    charging = battery_charging_indicated();
    if (charging) {
        soc_voltage = battery_voltage - BATTERY_CHARGE_V_BIAS;
        if (soc_voltage < battery_curve_voltages[0]) {
            soc_voltage = battery_curve_voltages[0];
        }
        pct = battery_percent_from_voltage(soc_voltage);
        if (pct > BATTERY_CHARGE_PCT_CAP) {
            pct = BATTERY_CHARGE_PCT_CAP;
        }
        if (battery_percent_smoothed >= 0 && pct > battery_percent_smoothed + BATTERY_CHARGE_PCT_MAX_STEP) {
            pct = battery_percent_smoothed + BATTERY_CHARGE_PCT_MAX_STEP;
        }
    } else {
        pct = battery_percent_from_voltage(battery_voltage);
        if (battery_voltage >= 4.14f) {
            pct = 100;
        }
    }

    battery_percent_smoothed = pct;
    return pct;
}

static void refresh_battery_ui(void)
{
    char buf[32];
    char detail_buf[48];

    battery_percent = read_battery_percent();

    {
        lv_color_t bar_color;
        int bar_val;
        if (battery_percent < 0) {
            snprintf(buf, sizeof(buf), "--%");
            bar_val = 0;
            bar_color = lv_color_hex(0x4A4060);
        } else {
            snprintf(buf, sizeof(buf), "%d%%", battery_percent);
            bar_val = battery_percent;
            bar_color = battery_indicator_color(battery_percent);
        }
        if (label_main_battery != NULL)
            lv_label_set_text(label_main_battery, buf);
        if (label_multiplayer_battery != NULL)
            lv_label_set_text(label_multiplayer_battery, buf);
        if (arc_main_battery != NULL) {
            lv_arc_set_value(arc_main_battery, bar_val);
            lv_obj_set_style_arc_color(arc_main_battery, bar_color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(arc_main_battery, LV_OPA_COVER, LV_PART_INDICATOR);
        }
        if (arc_multiplayer_battery != NULL) {
            lv_arc_set_value(arc_multiplayer_battery, bar_val);
            lv_obj_set_style_arc_color(arc_multiplayer_battery, bar_color, LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(arc_multiplayer_battery, LV_OPA_COVER, LV_PART_INDICATOR);
        }
        {
            bool charging = battery_charging_indicated();
            main_battery_hub_icons_apply(true);
            mp_battery_hub_icons_apply(!mp_delta_is_visible());
            if (lv_scr_act() == screen_main) {
                if (main_battery_panel != NULL)
                    lv_obj_move_foreground(main_battery_panel);
                main_screen_raise_layers();
            } else {
                battery_widgets_to_front(arc_multiplayer_battery, label_multiplayer_battery,
                                         label_multiplayer_battery_charge);
                if (label_multiplayer_battery_settings != NULL)
                    lv_obj_move_foreground(label_multiplayer_battery_settings);
                if (mp_battery_panel != NULL)
                    lv_obj_move_foreground(mp_battery_panel);
            }
            (void)charging;
        }
    }

    if (label_settings_battery == NULL) return;

    if (battery_percent < 0) {
        lv_label_set_text(label_settings_battery, i18n_get(I18N_BATTERY_UNKNOWN));
        if (label_settings_battery_detail != NULL) {
            lv_label_set_text(label_settings_battery_detail, i18n_get(I18N_NO_CALIBRATION));
        }
        return;
    }

    snprintf(buf, sizeof(buf), i18n_get(I18N_BATTERY_FMT), battery_percent);
    lv_label_set_text(label_settings_battery, buf);
    if (label_settings_battery_detail != NULL) {
        snprintf(detail_buf, sizeof(detail_buf), i18n_get(I18N_VOLTAGE_FMT), battery_voltage);
        lv_label_set_text(label_settings_battery_detail, detail_buf);
    }
}

static void brightness_init()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_RES,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_FREQ,
        .clk_cfg = LEDC_USE_RTC8M_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = BACKLIGHT_PIN,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
}

static void brightness_apply()
{
    uint32_t duty = (uint32_t)((brightness_percent * BACKLIGHT_DUTY_MAX) / 100);
    ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
    ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
}

static void settings_load(void)
{
    nvs_handle_t handle;
    if (nvs_open("arcmind", NVS_READONLY, &handle) == ESP_OK) {
        int8_t dim_val = 0;
        int8_t bri_val = DEFAULT_BRIGHTNESS_PERCENT;
        int8_t mirror_val = 0;
        int8_t timer_idx_val = 0;
        int8_t table_val = 0;
        int8_t lang_val = 0;
        int8_t players_val = 4;
        nvs_get_i8(handle, "auto_dim", &dim_val);
        nvs_get_i8(handle, "brightness", &bri_val);
        nvs_get_i8(handle, "mirror", &mirror_val);
        nvs_get_i8(handle, "timer_dur", &timer_idx_val);
        nvs_get_i8(handle, "table_shape", &table_val);
        nvs_get_i8(handle, "language", &lang_val);
        nvs_get_i8(handle, "players", &players_val);
        auto_dim_enabled = (dim_val != 0);
        brightness_percent = clamp_brightness(bri_val);
        mirror_enabled = (mirror_val != 0);
        if (timer_idx_val >= 0 && timer_idx_val < MP_TIMER_OPTIONS_COUNT)
            mp_timer_duration_idx = timer_idx_val;
        round_table_enabled = (table_val != 0);
        if (lang_val >= 0 && lang_val < LANG_COUNT)
            i18n_set_language_index(lang_val);
        if (players_val == 1 || players_val == 2 || players_val == 4)
            game_player_count = players_val;
        nvs_close(handle);
    }
    players_persist_load();
}

static void players_persist_load(void)
{
    nvs_handle_t handle;
    if (nvs_open("arcmind", NVS_READONLY, &handle) != ESP_OK) return;
    for (int pi = 0; pi < MULTIPLAYER_COUNT; pi++) {
        char key[12];
        char name_buf[16];
        size_t name_len = sizeof(name_buf);
        int8_t color_val = (int8_t)pi;
        snprintf(key, sizeof(key), "p_name_%d", pi);
        name_len = sizeof(name_buf);
        if (nvs_get_str(handle, key, name_buf, &name_len) == ESP_OK && name_buf[0] != '\0') {
            snprintf(multiplayer_names[pi], sizeof(multiplayer_names[pi]), "%s", name_buf);
        }
        snprintf(key, sizeof(key), "p_color_%d", pi);
        if (nvs_get_i8(handle, key, &color_val) == ESP_OK &&
            color_val >= 0 && color_val < PLAYER_PALETTE_COUNT) {
            multiplayer_color[pi] = (uint8_t)color_val;
        }
    }
    nvs_close(handle);
}

static void players_persist_save(void)
{
    nvs_handle_t handle;
    if (nvs_open("arcmind", NVS_READWRITE, &handle) != ESP_OK) return;
    for (int pi = 0; pi < MULTIPLAYER_COUNT; pi++) {
        char key[12];
        snprintf(key, sizeof(key), "p_name_%d", pi);
        nvs_set_str(handle, key, multiplayer_names[pi]);
        snprintf(key, sizeof(key), "p_color_%d", pi);
        nvs_set_i8(handle, key, (int8_t)multiplayer_color[pi]);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

static void settings_save(void)
{
    if (!settings_dirty) return;
    nvs_handle_t handle;
    if (nvs_open("arcmind", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i8(handle, "auto_dim", auto_dim_enabled ? 1 : 0);
        nvs_set_i8(handle, "brightness", (int8_t)brightness_percent);
        nvs_set_i8(handle, "mirror", mirror_enabled ? 1 : 0);
        nvs_set_i8(handle, "timer_dur", (int8_t)mp_timer_duration_idx);
        nvs_set_i8(handle, "table_shape", round_table_enabled ? 1 : 0);
        nvs_set_i8(handle, "language", (int8_t)i18n_get_language_index());
        nvs_set_i8(handle, "players", (int8_t)game_player_count);
        nvs_commit(handle);
        nvs_close(handle);
        settings_dirty = false;
    }
    players_persist_save();
}

bool activity_kick(void)
{
    bool was_dimmed = dimmed;
    last_activity_tick = lv_tick_get();
    if (dimmed) {
        setCpuFrequencyMhz(CPU_FREQ_ACTIVE);
        dimmed = false;
        undim_tick = last_activity_tick;
        brightness_apply();
    }
    return was_dimmed;
}

static bool in_undim_grace(void)
{
    return undim_tick != 0 && lv_tick_elaps(undim_tick) < UNDIM_GRACE_MS;
}

bool knob_is_dimmed(void)
{
    return dimmed;
}

static void auto_dim_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!auto_dim_enabled || dimmed) return;
    if (lv_tick_elaps(last_activity_tick) >= AUTO_DIM_TIMEOUT_MS) {
        dimmed = true;
        uint32_t duty = (uint32_t)((AUTO_DIM_BRIGHTNESS * BACKLIGHT_DUTY_MAX) / 100);
        ledc_set_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty);
        ledc_update_duty(BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL);
        setCpuFrequencyMhz(CPU_FREQ_IDLE);
    }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *txt, lv_coord_t w, lv_coord_t h, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(label, UI_FONT_14, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_center(label);

    return btn;
}

static void set_btn_text(lv_obj_t *btn, i18n_id_t id)
{
    lv_obj_t *label;

    if (btn == NULL) return;
    label = lv_obj_get_child(btn, 0);
    if (label != NULL) {
        lv_label_set_text(label, i18n_get(id));
        lv_obj_set_style_text_font(label, UI_FONT_14, 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    }
}

static void style_menu_label(lv_obj_t *label)
{
    if (label == NULL) return;
    lv_obj_set_style_text_color(label, lv_color_hex(0xE9E0FF), 0);
    lv_obj_set_style_text_font(label, UI_FONT_14, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
}

static void style_arcmind_menu_button(lv_obj_t *btn)
{
    lv_obj_t *label;

    if (btn == NULL) return;
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1B3A), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    label = lv_obj_get_child(btn, 0);
    style_menu_label(label);
}

static void style_arcmind_side_button(lv_obj_t *btn)
{
    lv_obj_t *label;

    if (btn == NULL) return;
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1630), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    label = lv_obj_get_child(btn, 0);
    if (label) {
        lv_obj_set_style_text_color(label, lv_color_hex(0xD8CCFF), 0);
        lv_obj_set_style_text_font(label, UI_FONT_14, 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    }
}

static void style_arcmind_keyboard(lv_obj_t *kb)
{
    if (kb == NULL) return;
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x13111F), 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_pad_all(kb, 2, 0);
    lv_obj_set_style_pad_gap(kb, 2, 0);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x1E1B3A), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, lv_color_hex(0x4C1D95), LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 6, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(0xC4B5FD), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x2D2A45), LV_PART_ITEMS | LV_STATE_PRESSED);
}

static lv_obj_t *make_plain_box(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *make_seg(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_radius(obj, 2, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}


static void create_digit(lv_obj_t *parent, lv_obj_t **seg)
{
    seg[0] = make_seg(parent, 10,   0, 40,  8);  // top
    seg[1] = make_seg(parent,  0,   8,  8, 44);  // upper-left
    seg[2] = make_seg(parent, 52,   8,  8, 44);  // upper-right
    seg[3] = make_seg(parent, 10,  52, 40,  8);  // middle
    seg[4] = make_seg(parent,  0,  60,  8, 44);  // lower-left
    seg[5] = make_seg(parent, 52,  60,  8, 44);  // lower-right
    seg[6] = make_seg(parent, 10, 104, 40,  8);  // bottom
}

static void set_seg_style(lv_obj_t *seg, lv_color_t color, bool on)
{
    if (on) {
        lv_obj_set_style_bg_color(seg, color, 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_bg_color(seg, lv_color_hex(0x101010), 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_30, 0);
    }
}

/* Player palette aligned with ArcMind tokens (brand, bracket, status). */
static lv_color_t get_player_base_color(int index)
{
    static const uint32_t colors[PLAYER_PALETTE_COUNT] = {
        0x6D28D9, 0x0891B2, 0xEAB308, 0x059669, 0xEA580C, 0xDC2626,
        0x7C3AED, 0x4F46E5, 0x2563EB, 0xD97706, 0x9333EA, 0x1D4ED8
    };
    int pi;
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x303030);
    pi = multiplayer_color[index];
    if (pi >= PLAYER_PALETTE_COUNT) pi = index % PLAYER_PALETTE_COUNT;
    return lv_color_hex(colors[pi]);
}

static lv_color_t get_player_active_color(int index)
{
    static const uint32_t colors[PLAYER_PALETTE_COUNT] = {
        0x8B5CF6, 0x22D3EE, 0xFACC15, 0x10B981, 0xFB923C, 0xEF4444,
        0xA78BFA, 0x6366F1, 0x60A5FA, 0xF59E0B, 0xA855F7, 0x3B82F6
    };
    int pi;
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x505050);
    pi = multiplayer_color[index];
    if (pi >= PLAYER_PALETTE_COUNT) pi = index % PLAYER_PALETTE_COUNT;
    return lv_color_hex(colors[pi]);
}

static lv_color_t get_player_text_color(int index)
{
    static const bool dark_text[PLAYER_PALETTE_COUNT] = {
        false, false, true, false, false, false,
        false, false, false, true, false, false
    };
    int pi;
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_white();
    pi = multiplayer_color[index];
    if (pi >= PLAYER_PALETTE_COUNT) pi = index % PLAYER_PALETTE_COUNT;
    return dark_text[pi] ? lv_color_black() : lv_color_white();
}

static lv_color_t get_palette_active_color(int palette_idx)
{
    static const uint32_t colors[PLAYER_PALETTE_COUNT] = {
        0x8B5CF6, 0x22D3EE, 0xFACC15, 0x10B981, 0xFB923C, 0xEF4444,
        0xA78BFA, 0x6366F1, 0x60A5FA, 0xF59E0B, 0xA855F7, 0x3B82F6
    };
    if (palette_idx < 0 || palette_idx >= PLAYER_PALETTE_COUNT) return lv_color_hex(0x505050);
    return lv_color_hex(colors[palette_idx]);
}

static bool is_palette_color_taken(int palette_idx, int except_player)
{
    int i;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (i == except_player) continue;
        if (multiplayer_color[i] == (uint8_t)palette_idx) return true;
    }
    return false;
}

static bool is_player_dead(int player)
{
    if (player < 0 || player >= MULTIPLAYER_COUNT) return false;
    return multiplayer_life[player] <= 0 || multiplayer_poison[player] >= POISON_MAX;
}


static void set_digit_segments(lv_obj_t **seg, int value, lv_color_t color, bool visible)
{
    int i;

    for (i = 0; i < 7; i++) {
        if (!visible) {
            lv_obj_add_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
            set_seg_style(seg[i], color, seg_map[value][i] ? true : false);
        }
    }
}

static void set_minus_segments(lv_obj_t **seg, lv_color_t color, bool visible)
{
    int i;

    if (digit_sign_plus_vert != NULL) {
        lv_obj_add_flag(digit_sign_plus_vert, LV_OBJ_FLAG_HIDDEN);
    }

    for (i = 0; i < 7; i++) {
        if (!visible) {
            lv_obj_add_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
            set_seg_style(seg[i], color, (i == 3));
        }
    }
}

static void set_plus_segments(lv_obj_t **seg, lv_color_t color, bool visible)
{
    int i;

    if (digit_sign_plus_vert != NULL) {
        if (!visible) {
            lv_obj_add_flag(digit_sign_plus_vert, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(digit_sign_plus_vert, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(digit_sign_plus_vert, color, 0);
            lv_obj_set_style_bg_opa(digit_sign_plus_vert, LV_OPA_COVER, 0);
        }
    }

    for (i = 0; i < 7; i++) {
        if (!visible) {
            lv_obj_add_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
            set_seg_style(seg[i], color, (i == 3));
        }
    }
}

static void refresh_life_digits()
{
    int display_value = life_total;
    int abs_v = (display_value < 0) ? -display_value : display_value;
    int hundreds = abs_v / 100;
    int tens = (abs_v / 10) % 10;
    int ones = abs_v % 10;
    bool negative = (display_value < 0);
    lv_coord_t x_offset = 0;
    lv_color_t c = get_life_color(display_value);

    if (negative && abs_v >= 100) x_offset = -25;

    if (life_container != NULL) {
        lv_obj_align(life_container, LV_ALIGN_CENTER, x_offset, -6);
    }
    if (life_hitbox != NULL) {
        lv_obj_align(life_hitbox, LV_ALIGN_CENTER, x_offset, -8);
    }

    lv_obj_add_flag(digit_box_sign, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(digit_box_hundreds, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(digit_box_tens, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);
    set_minus_segments(digit_sign, c, false);
    set_plus_segments(digit_sign, c, false);

    if (negative && abs_v >= 100) {
        lv_obj_set_pos(digit_box_sign, 0, 0);
        lv_obj_set_pos(digit_box_hundreds, 70, 0);
        lv_obj_set_pos(digit_box_tens, 140, 0);
        lv_obj_set_pos(digit_box_ones, 210, 0);

        lv_obj_clear_flag(digit_box_sign, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_hundreds, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_tens, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);

        set_minus_segments(digit_sign, c, true);
        set_digit_segments(digit_hundreds, hundreds, c, true);
        set_digit_segments(digit_tens, tens, c, true);
        set_digit_segments(digit_ones, ones, c, true);
    }
    else if (negative && abs_v >= 10) {
        lv_obj_set_pos(digit_box_sign, 10, 0);
        lv_obj_set_pos(digit_box_tens, 80, 0);
        lv_obj_set_pos(digit_box_ones, 150, 0);

        lv_obj_clear_flag(digit_box_sign, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_tens, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);

        set_minus_segments(digit_sign, c, true);
        set_digit_segments(digit_tens, tens, c, true);
        set_digit_segments(digit_ones, ones, c, true);
    }
    else if (negative) {
        lv_obj_set_pos(digit_box_sign, 45, 0);
        lv_obj_set_pos(digit_box_ones, 115, 0);

        lv_obj_clear_flag(digit_box_sign, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);

        set_minus_segments(digit_sign, c, true);
        set_digit_segments(digit_ones, ones, c, true);
    }
    else if (abs_v >= 100) {
        lv_obj_set_pos(digit_box_hundreds, 45, 0);
        lv_obj_set_pos(digit_box_tens, 115, 0);
        lv_obj_set_pos(digit_box_ones, 185, 0);

        lv_obj_clear_flag(digit_box_hundreds, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_tens, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);

        set_digit_segments(digit_hundreds, hundreds, c, true);
        set_digit_segments(digit_tens, tens, c, true);
        set_digit_segments(digit_ones, ones, c, true);
    }
    else if (abs_v >= 10) {
        lv_obj_set_pos(digit_box_tens, 80, 0);
        lv_obj_set_pos(digit_box_ones, 150, 0);

        lv_obj_clear_flag(digit_box_tens, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);

        set_digit_segments(digit_tens, tens, c, true);
        set_digit_segments(digit_ones, ones, c, true);
    }
    else {
        lv_obj_set_pos(digit_box_ones, 115, 0);
        lv_obj_clear_flag(digit_box_ones, LV_OBJ_FLAG_HIDDEN);
        set_digit_segments(digit_ones, ones, c, true);
    }
}

static void refresh_main_ui()
{
    char buf[8];
    lv_color_t player_color = get_player_active_color(0);
    lv_draw_label_dsc_t dsc;
    int s, a;
    bool dead = main_is_dead();

    sp_layout_counter_slots();

    if (main_bg_panel != NULL) {
        lv_obj_set_style_bg_color(main_bg_panel, player_color, 0);
        lv_obj_set_style_bg_opa(main_bg_panel, LV_OPA_10, 0);
    }

    snprintf(buf, sizeof(buf), "%d", life_total);
    if (canvas_main_life != NULL && canvas_main_life_buf != NULL) {
        memset(canvas_main_life_buf, 0, MAIN_LIFE_BUF_SIZE);
        if (!dead) {
            lv_draw_label_dsc_init(&dsc);
            dsc.color = player_color;
            dsc.font  = &lv_font_montserrat_36;
            dsc.align = LV_TEXT_ALIGN_CENTER;
            lv_canvas_draw_text(canvas_main_life, 0,
                (MAIN_LIFE_H - MAIN_LIFE_FONT_H) / 2, MAIN_LIFE_W, &dsc, buf);
        }
        lv_obj_align(canvas_main_life, LV_ALIGN_CENTER, 0, MAIN_CENTER_Y);
        lv_obj_clear_flag(canvas_main_life, LV_OBJ_FLAG_HIDDEN);
    }

    if (canvas_main_name != NULL && canvas_main_name_buf != NULL) {
        lv_draw_label_dsc_init(&dsc);
        dsc.color = player_color;
        dsc.font  = &lv_font_montserrat_22;
        dsc.align = LV_TEXT_ALIGN_CENTER;
        memset(canvas_main_name_buf, 0, MAIN_NAME_BUF_SIZE);
        lv_canvas_draw_text(canvas_main_name, 0,
            (MAIN_NAME_H - MAIN_NAME_FONT_H) / 2, MAIN_NAME_W, &dsc, multiplayer_names[0]);
        lv_obj_align(canvas_main_name, LV_ALIGN_CENTER, 0, MAIN_CENTER_Y - 38);
        lv_obj_clear_flag(canvas_main_name, LV_OBJ_FLAG_HIDDEN);
    }

    if (img_main_skull != NULL) {
        lv_obj_align(img_main_skull, LV_ALIGN_CENTER, 0, MAIN_CENTER_Y);
        if (dead) lv_obj_clear_flag(img_main_skull, LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_add_flag(img_main_skull, LV_OBJ_FLAG_HIDDEN);
    }

    for (s = 0; s < SP_RING_SLOTS; s++) {
        bool selected = (s == MP_SLOT_INFECT)
            ? (main_poison_selected >= 0)
            : (cmd_main_selected == s - 1);
        if (sp_ring_seg[s] != NULL)
            lv_obj_clear_flag(sp_ring_seg[s], LV_OBJ_FLAG_HIDDEN);
        sp_set_ring_seg_highlight(s, selected);
        if (btn_sp_ring_hit[s] != NULL) {
            if (dead && s == MP_SLOT_INFECT)
                lv_obj_add_flag(btn_sp_ring_hit[s], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_clear_flag(btn_sp_ring_hit[s], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (canvas_poison_sp != NULL) {
        if (dead) {
            lv_obj_add_flag(canvas_poison_sp, LV_OBJ_FLAG_HIDDEN);
        } else {
            refresh_main_poison_canvas();
            lv_obj_clear_flag(canvas_poison_sp, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(canvas_poison_sp);
        }
    }

    for (a = 0; a < 3; a++) {
        if (canvas_cmd_sp[a] == NULL || canvas_cmd_sp_buf[a] == NULL) continue;
        lv_draw_label_dsc_t cdsc;
        snprintf(buf, sizeof(buf), "%d", cmd_main_damage[a]);
        lv_draw_label_dsc_init(&cdsc);
        cdsc.color = lv_color_white();
        cdsc.font  = &lv_font_montserrat_14_pt;
        cdsc.align = LV_TEXT_ALIGN_CENTER;
        memset(canvas_cmd_sp_buf[a], 0, CMP_CMD_BUF_SIZE);
        lv_canvas_draw_text(canvas_cmd_sp[a], 0,
            (CMP_CMD_H - 14) / 2, CMP_CMD_W, &cdsc, buf);
        lv_obj_clear_flag(canvas_cmd_sp[a], LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(canvas_cmd_sp[a]);
    }

    for (s = 0; s < SP_RING_SLOTS; s++) {
        if (btn_sp_ring_hit[s] != NULL && !(dead && s == MP_SLOT_INFECT))
            lv_obj_move_foreground(btn_sp_ring_hit[s]);
    }
    main_screen_raise_layers();
}

static void mp_timer_refresh_btn_label(void);
static void refresh_settings_ui(void);
static void refresh_multiplayer_menu_ui(void);
static void refresh_multiplayer_color_ui(void);
static void refresh_multiplayer_name_ui(void);
static void refresh_multiplayer_cmd_select_ui(void);
static void refresh_multiplayer_cmd_damage_ui(void);
static void refresh_multiplayer_all_damage_ui(void);

static void refresh_texts_ui(void)
{
    int i;

    if (label_settings_title != NULL)
        lv_label_set_text(label_settings_title, i18n_get(I18N_SETTINGS));

    for (i = 0; i < SETTINGS_FIELD_COUNT; i++) {
        if (settings_row_name[i] != NULL)
            lv_label_set_text(settings_row_name[i], i18n_get(settings_field_ids[i]));
    }

    set_btn_text(btn_settings_back, I18N_BACK);
    set_btn_text(btn_menu_reset, I18N_RESET);
    set_btn_text(btn_menu_back, I18N_BACK);
    set_btn_text(btn_menu_settings, I18N_SETTINGS);
    set_btn_text(btn_menu_select_first, I18N_SELECT_FIRST_PLAYER);
    if (menu_players_name != NULL)
        lv_label_set_text(menu_players_name, i18n_get(I18N_PLAYERS));
    style_menu_label(lv_obj_get_child(btn_menu_settings, 0));
    style_menu_label(lv_obj_get_child(btn_menu_select_first, 0));
    set_btn_text(btn_mp_menu_rename, I18N_RENAME);
    set_btn_text(btn_mp_menu_pick_color, I18N_PICK_COLOR);
    set_btn_text(btn_mp_menu_back, I18N_BACK);
    set_btn_text(btn_mp_color_back, I18N_BACK);
    set_btn_text(btn_mp_name_save, I18N_SAVE);
    set_btn_text(btn_mp_name_back, I18N_BACK);
    set_btn_text(btn_mp_cmd_select_back, I18N_BACK);
    set_btn_text(btn_mp_cmd_damage_back, I18N_BACK);
    set_btn_text(btn_mp_all_damage_apply, I18N_APPLY);
    set_btn_text(btn_mp_all_damage_back, I18N_BACK);

    if (label_multiplayer_cmd_damage_hint != NULL)
        lv_label_set_text(label_multiplayer_cmd_damage_hint, i18n_get(I18N_CMD_DAMAGE_HINT));
    if (label_multiplayer_all_damage_hint != NULL)
        lv_label_set_text(label_multiplayer_all_damage_hint, i18n_get(I18N_ALL_DAMAGE_HINT));

    if (menu_players_name != NULL)
        lv_label_set_text(menu_players_name, i18n_get(I18N_PLAYERS));

    refresh_menu_focus_ui();

    refresh_settings_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_color_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_multiplayer_all_damage_ui();
    refresh_battery_ui();
    mp_timer_refresh_btn_label();
}

static bool settings_field_visible(int idx)
{
#if SETTINGS_TIMER_FIELD_VISIBLE
    (void)idx;
    return true;
#else
    return idx != SETTINGS_TIMER_FIELD_IDX;
#endif
}

static lv_coord_t settings_row_offset_y(int field_idx)
{
#if SETTINGS_TIMER_FIELD_VISIBLE
    static const lv_coord_t row_y[SETTINGS_FIELD_COUNT] = {-125, -79, -33, 13, 54, 98};
    return row_y[field_idx];
#else
    switch (field_idx) {
    case 0: return -125;
    case 1: return -79;
    case 2: return -33;
    case 4: return 13;
    case 5: return 54;
    default: return 13;
    }
#endif
}

static void refresh_settings_ui()
{
    char buf[16];
    int i;

    for (i = 0; i < SETTINGS_FIELD_COUNT; i++) {
        if (!settings_field_visible(i) || settings_row[i] == NULL) continue;
        lv_color_t border_c = (i == settings_selected_field)
            ? lv_color_hex(0x8B5CF6)
            : lv_color_hex(0x2D2A45);
        lv_obj_set_style_border_color(settings_row[i], border_c, 0);
    }

    snprintf(buf, sizeof(buf), "%d%%", brightness_percent);
    if (settings_row_value[0] != NULL) lv_label_set_text(settings_row_value[0], buf);
    if (settings_row_value[1] != NULL) lv_label_set_text(settings_row_value[1], i18n_get(auto_dim_enabled ? I18N_ON : I18N_OFF));
    if (settings_row_value[2] != NULL) lv_label_set_text(settings_row_value[2], i18n_get(mirror_enabled ? I18N_ON : I18N_OFF));
    if (settings_row_value[3] != NULL) {
        uint16_t secs = MP_TIMER_OPTIONS[mp_timer_duration_idx];
        if (secs == 0) lv_label_set_text(settings_row_value[3], i18n_get(I18N_OFF));
        else { snprintf(buf, sizeof(buf), "%ds", secs); lv_label_set_text(settings_row_value[3], buf); }
    }
    if (settings_row_value[4] != NULL) lv_label_set_text(settings_row_value[4], i18n_get(round_table_enabled ? I18N_ROUND : I18N_RECT));
    if (settings_row_value[5] != NULL) lv_label_set_text(settings_row_value[5], i18n_language_name(i18n_get_language_index()));

    if (btn_settings_back != NULL)
        lv_obj_move_foreground(btn_settings_back);

    refresh_battery_ui();
}

static void set_multiplayer_area_opa(int index, lv_opa_t opa)
{
    if (index < 0 || index >= MULTIPLAYER_COUNT) return;

    if (round_table_enabled) {
        if (multiplayer_round_segments[index] != NULL)
            lv_obj_set_style_arc_opa(multiplayer_round_segments[index], opa, LV_PART_MAIN);
        if (multiplayer_quadrants[index] != NULL)
            lv_obj_set_style_bg_opa(multiplayer_quadrants[index], LV_OPA_TRANSP, 0);
    } else if (multiplayer_quadrants[index] != NULL) {
        lv_obj_set_style_bg_opa(multiplayer_quadrants[index], opa, 0);
    }
}

static void set_cmd_mp_circle_selected(int victim, int attacker_slot, bool selected)
{
    if (victim < 0 || victim >= MULTIPLAYER_COUNT) return;
    if (attacker_slot < 0 || attacker_slot >= 3) return;
    set_mp_ring_seg_highlight(victim, attacker_slot + 1, selected);
}

static void refresh_poison_canvas(int player)
{
    char pbuf[4];
    lv_draw_img_dsc_t img_dsc;
    lv_draw_label_dsc_t dsc;

    if (player < 0 || player >= MULTIPLAYER_COUNT) return;
    if (canvas_poison_mp[player] == NULL || canvas_poison_mp_buf[player] == NULL) return;

    snprintf(pbuf, sizeof(pbuf), "%d", multiplayer_poison[player]);
    memset(canvas_poison_mp_buf[player], 0, CMP_POISON_BUF_SIZE);

    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_white();
    if (mp_is_2p()) {
        lv_draw_img_dsc_init(&img_dsc);
        lv_canvas_draw_img(canvas_poison_mp[player], 0, 0, &infect_img, &img_dsc);
        dsc.font = MP2P_SLICE_VALUE_FONT;
        dsc.align = LV_TEXT_ALIGN_LEFT;
        lv_canvas_draw_text(canvas_poison_mp[player], 24,
            (CMP_POISON_H - MP2P_SLICE_VALUE_H) / 2, CMP_POISON_W - 24, &dsc, pbuf);
    } else {
        lv_draw_img_dsc_init(&img_dsc);
        lv_canvas_draw_img(canvas_poison_mp[player], 0, 0, &infect_img, &img_dsc);
        dsc.font = &lv_font_montserrat_14;
        dsc.align = LV_TEXT_ALIGN_LEFT;
        lv_canvas_draw_text(canvas_poison_mp[player], 24, 5, CMP_POISON_W - 24, &dsc, pbuf);
    }
    lv_img_set_angle(canvas_poison_mp[player], mp_slot_label_angle(player));
}

static void refresh_multiplayer_ui()
{
    static const int16_t rect_life_offsets_x[MULTIPLAYER_COUNT] = {-52, 52, 52, -52};
    static const int16_t rect_life_offsets_y[MULTIPLAYER_COUNT] = {-52, -52, 52, 52};
    const int16_t *life_offsets_x = rect_life_offsets_x;
    const int16_t *life_offsets_y = rect_life_offsets_y;
    static const int16_t rect_name_offsets_x[MULTIPLAYER_COUNT] = {-52, 52, 52, -52};
    const int16_t *name_offsets_x = rect_name_offsets_x;
    int16_t name_offsets_y[MULTIPLAYER_COUNT];
    int16_t round_life_x[MULTIPLAYER_COUNT];
    int16_t round_life_y[MULTIPLAYER_COUNT];
    int16_t round_name_x[MULTIPLAYER_COUNT];
    int16_t round_name_y[MULTIPLAYER_COUNT];
    static const lv_coord_t rect_quad_x[MULTIPLAYER_COUNT] = {0, 180, 180, 0};
    static const lv_coord_t rect_quad_y[MULTIPLAYER_COUNT] = {0, 0, 180, 180};
    static const int16_t rect_2p_life_x[MULTIPLAYER_COUNT] = {0, 0, 0, 0};
    static const int16_t rect_2p_life_y[MULTIPLAYER_COUNT] = {0, -88, 88, 0};
    static const int16_t rect_mirror_diag_x[MULTIPLAYER_COUNT] = {-14, 14, 14, -14};
    static const int16_t rect_mirror_diag_y[MULTIPLAYER_COUNT] = {-14, -14, 14, 14};
    char buf[8];
    int i, a, s;
    int ring_slots = mp_ring_slots();

    if (round_table_enabled && !mp_is_2p()) {
        for (i = 0; i < MULTIPLAYER_COUNT; i++) {
            round_content_offset(i, false, &round_life_x[i], &round_life_y[i]);
            round_content_offset(i, true, &round_name_x[i], &round_name_y[i]);
            name_offsets_y[i] = 0;
        }
        if (!mirror_enabled) {
            /* Left/right wedges: radial offset puts name beside life — stack vertically. */
            round_name_x[1] = round_life_x[1];
            round_name_y[1] = (int16_t)(round_life_y[1] - 34);
            round_name_x[3] = round_life_x[3];
            round_name_y[3] = (int16_t)(round_life_y[3] - 34);
        }
    } else if (mp_is_2p()) {
        name_offsets_y[1] = (int16_t)(mirror_enabled ? -58 : -102);
        name_offsets_y[2] = 58;
    } else {
        /* Name x same as life x; name y is above life from each player's perspective.
         * P0/P1: mirror=OFF -> above on screen; mirror=ON -> below on screen.
         * P2/P3: always above life on screen. */
        name_offsets_y[0] = (int16_t)(mirror_enabled ? -32 : -78);
        name_offsets_y[1] = (int16_t)(mirror_enabled ? -32 : -78);
        name_offsets_y[2] = 25;
        name_offsets_y[3] = 25;
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        lv_color_t player_color = get_player_active_color(i);
        int16_t life_x, life_y, name_x, name_y;

        if (!mp_player_active(i)) {
            if (multiplayer_quadrants[i] != NULL)
                lv_obj_add_flag(multiplayer_quadrants[i], LV_OBJ_FLAG_HIDDEN);
            if (multiplayer_round_segments[i] != NULL)
                lv_obj_add_flag(multiplayer_round_segments[i], LV_OBJ_FLAG_HIDDEN);
            if (round_wedge_hit[i] != NULL)
                lv_obj_add_flag(round_wedge_hit[i], LV_OBJ_FLAG_HIDDEN);
            if (canvas_mp_life[i] != NULL)
                lv_obj_add_flag(canvas_mp_life[i], LV_OBJ_FLAG_HIDDEN);
            if (canvas_mp_name[i] != NULL)
                lv_obj_add_flag(canvas_mp_name[i], LV_OBJ_FLAG_HIDDEN);
            if (label_multiplayer_life[i] != NULL)
                lv_obj_add_flag(label_multiplayer_life[i], LV_OBJ_FLAG_HIDDEN);
            if (label_multiplayer_name[i] != NULL)
                lv_obj_add_flag(label_multiplayer_name[i], LV_OBJ_FLAG_HIDDEN);
            if (img_skull[i] != NULL)
                lv_obj_add_flag(img_skull[i], LV_OBJ_FLAG_HIDDEN);
            if (canvas_poison_mp[i] != NULL)
                lv_obj_add_flag(canvas_poison_mp[i], LV_OBJ_FLAG_HIDDEN);
            for (s = 0; s < 4; s++) {
                if (mp_ring_seg[i][s] != NULL)
                    lv_obj_add_flag(mp_ring_seg[i][s], LV_OBJ_FLAG_HIDDEN);
                if (btn_mp_ring_hit[i][s] != NULL)
                    lv_obj_add_flag(btn_mp_ring_hit[i][s], LV_OBJ_FLAG_HIDDEN);
            }
            for (a = 0; a < 3; a++) {
                if (canvas_cmd_mp[i][a] != NULL)
                    lv_obj_add_flag(canvas_cmd_mp[i][a], LV_OBJ_FLAG_HIDDEN);
            }
            continue;
        }

        if (round_table_enabled && !mp_is_2p()) {
            life_x = round_life_x[i];
            life_y = round_life_y[i];
            name_x = round_name_x[i];
            name_y = round_name_y[i];
        } else if (mp_is_2p()) {
            life_x = rect_2p_life_x[i];
            life_y = rect_2p_life_y[i];
            if (!mirror_enabled && i == 1) {
                life_y = (int16_t)(life_y + 16);
            }
            name_x = 0;
            name_y = name_offsets_y[i];
        } else {
            life_x = rect_life_offsets_x[i];
            life_y = rect_life_offsets_y[i];
            name_x = name_offsets_x[i];
            name_y = name_offsets_y[i];
            if (mirror_enabled) {
                life_x = (int16_t)(life_x + rect_mirror_diag_x[i]);
                life_y = (int16_t)(life_y + rect_mirror_diag_y[i]);
                name_x = (int16_t)(name_x + rect_mirror_diag_x[i]);
                name_y = (int16_t)(name_y + rect_mirror_diag_y[i]);
            } else if (i == 2 || i == 3) {
                life_y = (int16_t)(life_y + 16);
                name_y = (int16_t)(name_y + 16);
            }
        }

        if (multiplayer_quadrants[i] != NULL) {
            if (round_table_enabled && !mp_is_2p()) {
                lv_obj_add_flag(multiplayer_quadrants[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(multiplayer_quadrants[i], LV_OBJ_FLAG_HIDDEN);
                if (mp_is_2p()) {
                    lv_obj_set_size(multiplayer_quadrants[i], 360, 180);
                    lv_obj_set_pos(multiplayer_quadrants[i], 0, (i == 1) ? 0 : 180);
                } else {
                    lv_obj_set_size(multiplayer_quadrants[i], 180, 180);
                    lv_obj_set_pos(multiplayer_quadrants[i], rect_quad_x[i], rect_quad_y[i]);
                }
                lv_obj_set_style_radius(multiplayer_quadrants[i], 0, 0);
                lv_obj_set_style_border_color(multiplayer_quadrants[i], lv_color_black(), 0);
                lv_obj_set_style_border_opa(multiplayer_quadrants[i], LV_OPA_COVER, 0);
            }
            lv_obj_set_style_bg_color(multiplayer_quadrants[i], player_color, 0);
            set_multiplayer_area_opa(i, (i == multiplayer_selected) ? LV_OPA_20 : LV_OPA_10);
        }

        if (multiplayer_round_segments[i] != NULL) {
            lv_obj_set_style_arc_color(multiplayer_round_segments[i], player_color, LV_PART_MAIN);
            if (round_table_enabled && !mp_is_2p()) {
                lv_obj_set_size(multiplayer_round_segments[i], 358, 358);
                lv_obj_set_style_arc_width(multiplayer_round_segments[i], 179, LV_PART_MAIN);
                lv_arc_set_rotation(multiplayer_round_segments[i], MP_RING_ROTATION);
                lv_arc_set_bg_angles(multiplayer_round_segments[i],
                    (uint16_t)ROUND_WEDGE_START[i], (uint16_t)ROUND_WEDGE_END[i]);
                lv_obj_clear_flag(multiplayer_round_segments[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(multiplayer_round_segments[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (round_wedge_hit[i] != NULL) {
            if (round_table_enabled && mp_player_active(i) && !mp_is_2p())
                lv_obj_clear_flag(round_wedge_hit[i], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(round_wedge_hit[i], LV_OBJ_FLAG_HIDDEN);
        }

        layout_mp_counter_slots(i);

        snprintf(buf, sizeof(buf), "%d", multiplayer_life[i]);

        if (mp_use_canvas_life_name(i)) {
            /* Canvas + lv_img_set_angle avoids LVGL-heap layer alloc for rotated text. */
            int16_t angle = mp_facing_angle(i);
            lv_draw_label_dsc_t dsc;

            if (label_multiplayer_life[i] != NULL)
                lv_obj_add_flag(label_multiplayer_life[i], LV_OBJ_FLAG_HIDDEN);
            if (label_multiplayer_name[i] != NULL)
                lv_obj_add_flag(label_multiplayer_name[i], LV_OBJ_FLAG_HIDDEN);

            if (canvas_mp_life[i] != NULL && canvas_mp_life_buf[i] != NULL) {
                memset(canvas_mp_life_buf[i], 0, CMP_LIFE_BUF_SIZE);
                if (!is_player_dead(i)) {
                    lv_draw_label_dsc_init(&dsc);
                    dsc.color = player_color;
                    dsc.font  = &lv_font_montserrat_36;
                    dsc.align = LV_TEXT_ALIGN_CENTER;
                    lv_canvas_draw_text(canvas_mp_life[i], 0,
                        (CMP_LIFE_H - 36) / 2, CMP_LIFE_W, &dsc, buf);
                }
                lv_obj_align(canvas_mp_life[i], LV_ALIGN_CENTER, life_x, life_y);
                lv_img_set_angle(canvas_mp_life[i], angle);
                lv_obj_clear_flag(canvas_mp_life[i], LV_OBJ_FLAG_HIDDEN);
            }

            if (canvas_mp_name[i] != NULL && canvas_mp_name_buf[i] != NULL) {
                lv_draw_label_dsc_init(&dsc);
                dsc.color = player_color;
                dsc.font  = &lv_font_montserrat_14_pt;
                dsc.align = LV_TEXT_ALIGN_CENTER;
                memset(canvas_mp_name_buf[i], 0, CMP_NAME_BUF_SIZE);
                lv_canvas_draw_text(canvas_mp_name[i], 0,
                    (CMP_NAME_H - 14) / 2, CMP_NAME_W, &dsc, multiplayer_names[i]);
                lv_obj_align(canvas_mp_name[i], LV_ALIGN_CENTER, name_x, name_y);
                lv_img_set_angle(canvas_mp_name[i], angle);
                lv_obj_clear_flag(canvas_mp_name[i], LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            /* Bottom players: plain labels, no rotation */
            if (canvas_mp_life[i] != NULL)
                lv_obj_add_flag(canvas_mp_life[i], LV_OBJ_FLAG_HIDDEN);
            if (canvas_mp_name[i] != NULL)
                lv_obj_add_flag(canvas_mp_name[i], LV_OBJ_FLAG_HIDDEN);

            if (label_multiplayer_life[i] != NULL) {
                if (!is_player_dead(i)) {
                    lv_label_set_text(label_multiplayer_life[i], buf);
                    lv_obj_set_style_text_color(label_multiplayer_life[i], player_color, 0);
                    lv_obj_align(label_multiplayer_life[i], LV_ALIGN_CENTER, life_x, life_y);
                    lv_obj_clear_flag(label_multiplayer_life[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(label_multiplayer_life[i], LV_OBJ_FLAG_HIDDEN);
                }
            }

            if (label_multiplayer_name[i] != NULL) {
                lv_label_set_text(label_multiplayer_name[i], multiplayer_names[i]);
                lv_obj_set_style_text_color(label_multiplayer_name[i], player_color, 0);
                lv_obj_align(label_multiplayer_name[i], LV_ALIGN_CENTER, name_x, name_y);
                lv_obj_clear_flag(label_multiplayer_name[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* Dead overlays: life <= 0 or poison >= 10 */
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        int16_t life_x, life_y;

        if (img_skull[i] == NULL || !mp_player_active(i)) continue;
        if (round_table_enabled && !mp_is_2p()) {
            life_x = round_life_x[i];
            life_y = round_life_y[i];
        } else if (mp_is_2p()) {
            life_x = rect_2p_life_x[i];
            life_y = rect_2p_life_y[i];
            if (!mirror_enabled && i == 1)
                life_y = (int16_t)(life_y + 16);
        } else {
            life_x = rect_life_offsets_x[i];
            life_y = rect_life_offsets_y[i];
            if (mirror_enabled) {
                life_x = (int16_t)(life_x + rect_mirror_diag_x[i]);
                life_y = (int16_t)(life_y + rect_mirror_diag_y[i]);
            } else if (i == 2 || i == 3) {
                life_y = (int16_t)(life_y + 16);
            }
        }
        lv_obj_align(img_skull[i], LV_ALIGN_CENTER, life_x, life_y);
        if (mp_use_canvas_life_name(i))
            lv_img_set_angle(img_skull[i], mp_facing_angle(i));
        else
            lv_img_set_angle(img_skull[i], 0);
        if (is_player_dead(i)) {
            lv_obj_clear_flag(img_skull[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(img_skull[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Ring slices: slot 0 = black poison arc, slots 1-3 = commander colors */
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (!mp_player_active(i)) continue;
        for (s = 0; s < ring_slots; s++) {
            bool selected = (s == MP_SLOT_INFECT)
                ? (mp_poison_selected == i)
                : (cmd_mp_selected_victim == i && cmd_mp_selected_attacker == (s - 1));
            set_mp_ring_seg_highlight(i, s, selected);
            if (mp_ring_seg[i][s] != NULL)
                lv_obj_clear_flag(mp_ring_seg[i][s], LV_OBJ_FLAG_HIDDEN);
        }
        for (s = ring_slots; s < 4; s++) {
            if (mp_ring_seg[i][s] != NULL)
                lv_obj_add_flag(mp_ring_seg[i][s], LV_OBJ_FLAG_HIDDEN);
            if (btn_mp_ring_hit[i][s] != NULL)
                lv_obj_add_flag(btn_mp_ring_hit[i][s], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Poison icon + count on black ring slice (canvas rotation — mirror-safe) */
    {
        for (i = 0; i < MULTIPLAYER_COUNT; i++) {
            lv_obj_t *canvas = canvas_poison_mp[i];
            lv_obj_t *poison_hit = btn_mp_ring_hit[i][MP_SLOT_INFECT];
            if (canvas == NULL || !mp_player_active(i)) continue;
            if (is_player_dead(i)) {
                lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
                if (poison_hit != NULL)
                    lv_obj_add_flag(poison_hit, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_clear_flag(canvas, LV_OBJ_FLAG_HIDDEN);
            if (poison_hit != NULL)
                lv_obj_clear_flag(poison_hit, LV_OBJ_FLAG_HIDDEN);
            refresh_poison_canvas(i);
            lv_obj_move_foreground(canvas);
        }
    }

    /* Ring hit targets on top so every slice receives taps */
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (!mp_player_active(i)) continue;
        for (s = 0; s < ring_slots; s++) {
            lv_obj_t *hit = btn_mp_ring_hit[i][s];
            if (hit == NULL) continue;
            lv_obj_clear_flag(hit, LV_OBJ_FLAG_HIDDEN);
            if (s == MP_SLOT_INFECT && is_player_dead(i)) continue;
            lv_obj_move_foreground(hit);
        }
    }

    /* Commander circle labels: canvas + rotation on outer ring */
    {
        int v, cmd_slots;
        for (v = 0; v < MULTIPLAYER_COUNT; v++) {
            int16_t angle;
            if (!mp_player_active(v)) continue;
            cmd_slots = mp_is_2p() ? 1 : 3;
            angle = mp_slot_label_angle(v);
            for (a = 0; a < 3; a++) {
                if (canvas_cmd_mp[v][a] == NULL || canvas_cmd_mp_buf[v][a] == NULL) continue;
                if (a >= cmd_slots) {
                    lv_obj_add_flag(canvas_cmd_mp[v][a], LV_OBJ_FLAG_HIDDEN);
                    continue;
                }
                char nbuf[8];
                lv_draw_label_dsc_t dsc;
                int attacker = mp_is_2p() ? mp2p_opponent(v) : cmp_mp_attackers[v][a];
                snprintf(nbuf, sizeof(nbuf), "%d", multiplayer_cmd_damage_totals[v][attacker]);
                lv_draw_label_dsc_init(&dsc);
                dsc.color = lv_color_white();
                dsc.align = LV_TEXT_ALIGN_CENTER;
                if (mp_is_2p()) {
                    dsc.font = MP2P_SLICE_VALUE_FONT;
                    memset(canvas_cmd_mp_buf[v][a], 0, CMP_CMD_BUF_SIZE);
                    lv_canvas_draw_text(canvas_cmd_mp[v][a], 0,
                        (CMP_CMD_H - MP2P_SLICE_VALUE_H) / 2, CMP_CMD_W, &dsc, nbuf);
                } else {
                    dsc.font = &lv_font_montserrat_14_pt;
                    memset(canvas_cmd_mp_buf[v][a], 0, CMP_CMD_BUF_SIZE);
                    lv_canvas_draw_text(canvas_cmd_mp[v][a], 0,
                        (CMP_CMD_H - 14) / 2, CMP_CMD_W, &dsc, nbuf);
                }
                lv_img_set_angle(canvas_cmd_mp[v][a], angle);
                lv_obj_clear_flag(canvas_cmd_mp[v][a], LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(canvas_cmd_mp[v][a]);
            }
        }
    }

    if (round_table_enabled && !mp_is_2p()) {
        for (i = 0; i < MULTIPLAYER_COUNT; i++) {
            if (round_wedge_hit[i] != NULL)
                lv_obj_move_background(round_wedge_hit[i]);
        }
    }

    mp_timer_update_highlight();
}

static void refresh_multiplayer_menu_ui()
{
    char buf[32];

    if (label_multiplayer_menu_title == NULL) return;

    snprintf(buf, sizeof(buf), i18n_get(I18N_MENU_FMT), multiplayer_names[multiplayer_menu_player]);
    lv_label_set_text(label_multiplayer_menu_title, buf);
}

static void refresh_multiplayer_color_ui()
{
    char buf[32];
    int pi;

    if (label_multiplayer_color_title != NULL) {
        snprintf(buf, sizeof(buf), i18n_get(I18N_MENU_FMT), multiplayer_names[multiplayer_menu_player]);
        lv_label_set_text(label_multiplayer_color_title, buf);
    }

    for (pi = 0; pi < PLAYER_PALETTE_COUNT; pi++) {
        lv_obj_t *sw = mp_color_swatch[pi];
        lv_obj_t *mark = mp_color_taken_mark[pi];
        bool taken;
        bool selected;
        if (sw == NULL) continue;
        taken = is_palette_color_taken(pi, multiplayer_menu_player);
        selected = (multiplayer_color[multiplayer_menu_player] == (uint8_t)pi);
        lv_obj_set_style_bg_color(sw, get_palette_active_color(pi), 0);
        lv_obj_set_style_bg_opa(sw, taken ? LV_OPA_50 : LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(sw,
            selected ? lv_color_hex(0xC4B5FD) : lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(sw, selected ? 3 : 1, 0);
        if (mark != NULL) {
            if (taken) lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
            else         lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void refresh_multiplayer_name_ui()
{
    char buf[40];

    if (label_multiplayer_name_title != NULL) {
        snprintf(buf, sizeof(buf), i18n_get(I18N_RENAME_FMT), multiplayer_names[multiplayer_menu_player]);
        lv_label_set_text(label_multiplayer_name_title, buf);
    }

    if (textarea_multiplayer_name != NULL) {
        lv_textarea_set_text(textarea_multiplayer_name, multiplayer_names[multiplayer_menu_player]);
    }
}

static void refresh_multiplayer_cmd_select_ui()
{
    char buf[48];
    int i;
    int row = 0;

    if (label_multiplayer_cmd_select_title != NULL) {
        snprintf(buf, sizeof(buf), i18n_get(I18N_TARGET_FMT), multiplayer_names[multiplayer_menu_player]);
        lv_label_set_text(label_multiplayer_cmd_select_title, buf);
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (i == multiplayer_menu_player) continue;
        multiplayer_cmd_target_choices[row] = i;
        if (button_multiplayer_cmd_target[row] != NULL) {
            lv_obj_clear_flag(button_multiplayer_cmd_target[row], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(button_multiplayer_cmd_target[row], get_player_base_color(i), 0);
            lv_obj_set_style_bg_opa(button_multiplayer_cmd_target[row], LV_OPA_COVER, 0);
        }
        if (label_multiplayer_cmd_target[row] != NULL) {
            snprintf(buf, sizeof(buf), "%s  %d", multiplayer_names[i], multiplayer_cmd_damage_totals[i][multiplayer_menu_player]);
            lv_label_set_text(label_multiplayer_cmd_target[row], buf);
            lv_obj_set_style_text_color(label_multiplayer_cmd_target[row], get_player_text_color(i), 0);
        }
        row++;
    }

    while (row < (MULTIPLAYER_COUNT - 1)) {
        multiplayer_cmd_target_choices[row] = -1;
        if (button_multiplayer_cmd_target[row] != NULL) {
            lv_obj_add_flag(button_multiplayer_cmd_target[row], LV_OBJ_FLAG_HIDDEN);
        }
        row++;
    }
}

static void refresh_multiplayer_cmd_damage_ui()
{
    char title_buf[48];
    char value_buf[32];

    if (multiplayer_cmd_target < 0 || multiplayer_cmd_target >= MULTIPLAYER_COUNT) return;

    if (label_multiplayer_cmd_damage_title != NULL) {
        snprintf(title_buf, sizeof(title_buf), "%s -> %s",
                 multiplayer_names[multiplayer_cmd_source],
                 multiplayer_names[multiplayer_cmd_target]);
        lv_label_set_text(label_multiplayer_cmd_damage_title, title_buf);
    }

    if (label_multiplayer_cmd_damage_value != NULL) {
        snprintf(value_buf, sizeof(value_buf), i18n_get(I18N_DAMAGE_FMT), multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target]);
        lv_label_set_text(label_multiplayer_cmd_damage_value, value_buf);
    }
}

static void refresh_multiplayer_all_damage_ui()
{
    char buf[32];

    if (label_multiplayer_all_damage_title != NULL) {
        lv_label_set_text(label_multiplayer_all_damage_title, i18n_get(I18N_ALL_PLAYERS));
    }

    if (label_multiplayer_all_damage_value != NULL) {
        snprintf(buf, sizeof(buf), i18n_get(I18N_DAMAGE_FMT), multiplayer_all_damage_value);
        lv_label_set_text(label_multiplayer_all_damage_value, buf);
    }
}

static void life_delta_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    life_delta_acc = 0;
    if (label_life_delta != NULL)
        lv_obj_add_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
    if (life_delta_hide_timer != NULL)
        lv_timer_pause(life_delta_hide_timer);
}

static void mp_delta_hide_cb(lv_timer_t *timer)
{
    int i;
    (void)timer;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) mp_delta_acc[i] = 0;
    mp_delta_player = -1;
    if (canvas_mp_delta_ctr != NULL)
        lv_obj_add_flag(canvas_mp_delta_ctr, LV_OBJ_FLAG_HIDDEN);
    mp_battery_pct_label_set_visible(true);
    if (mp_delta_hide_timer != NULL)
        lv_timer_pause(mp_delta_hide_timer);
}

static void show_life_delta(void)
{
    char buf[8];
    lv_color_t color;
    if (label_life_delta == NULL) return;
    if (life_delta_acc == 0) {
        lv_obj_add_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (life_delta_acc > 0) {
        snprintf(buf, sizeof(buf), "+%d", life_delta_acc);
        color = lv_color_hex(0x22C55E);
    } else {
        snprintf(buf, sizeof(buf), "%d", life_delta_acc);
        color = lv_color_hex(0xEF4444);
    }
    lv_label_set_text(label_life_delta, buf);
    lv_obj_set_style_text_color(label_life_delta, color, 0);
    lv_obj_align(label_life_delta, LV_ALIGN_CENTER, MAIN_DELTA_X, MAIN_CENTER_Y);
    lv_obj_clear_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
    if (life_delta_hide_timer != NULL) {
        lv_timer_reset(life_delta_hide_timer);
        lv_timer_resume(life_delta_hide_timer);
    }
}

static void show_mp_delta(int player)
{
    char buf[8];
    lv_color_t color;
    lv_draw_label_dsc_t dsc;
    int16_t angle;

    if (player < 0 || player >= MULTIPLAYER_COUNT) return;
    if (canvas_mp_delta_ctr == NULL || canvas_mp_delta_ctr_buf == NULL) return;

    if (mp_delta_acc[player] == 0) {
        lv_obj_add_flag(canvas_mp_delta_ctr, LV_OBJ_FLAG_HIDDEN);
        mp_battery_pct_label_set_visible(true);
        return;
    }

    if (mp_delta_acc[player] > 0) {
        snprintf(buf, sizeof(buf), "+%d", mp_delta_acc[player]);
        color = lv_color_hex(0x22C55E);
    } else {
        snprintf(buf, sizeof(buf), "%d", mp_delta_acc[player]);
        color = lv_color_hex(0xEF4444);
    }

    mp_delta_player = player;
    angle = mp_facing_angle(player);

    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font  = &lv_font_montserrat_36;
    dsc.align = LV_TEXT_ALIGN_CENTER;
    memset(canvas_mp_delta_ctr_buf, 0, CMP_DELTA_BUF_SIZE);
    lv_canvas_draw_text(canvas_mp_delta_ctr, 0,
        (CMP_DELTA_H - 36) / 2, CMP_DELTA_W, &dsc, buf);
    lv_obj_align(canvas_mp_delta_ctr, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_angle(canvas_mp_delta_ctr, angle);
    lv_obj_clear_flag(canvas_mp_delta_ctr, LV_OBJ_FLAG_HIDDEN);
    mp_battery_pct_label_set_visible(false);

    if (mp_delta_hide_timer != NULL) {
        lv_timer_reset(mp_delta_hide_timer);
        lv_timer_resume(mp_delta_hide_timer);
    }
}

static void change_life(int delta)
{
    life_total = clamp_life(life_total + delta);
    life_delta_acc += delta;
    show_life_delta();
    refresh_main_ui();
}

static void refresh_menu_brightness_ui(void)
{
    char buf[16];
    if (menu_brightness_bar != NULL)
        lv_bar_set_value(menu_brightness_bar, brightness_percent, LV_ANIM_OFF);
    if (menu_brightness_label != NULL) {
        snprintf(buf, sizeof(buf), "%d%%", brightness_percent);
        lv_label_set_text(menu_brightness_label, buf);
    }
}

static void change_brightness(int delta)
{
    brightness_percent = clamp_brightness(brightness_percent + delta);
    settings_dirty = true;
    brightness_apply();
    refresh_settings_ui();
    refresh_menu_brightness_ui();
}

static void change_settings_field(int dir)
{
    switch (settings_selected_field) {
    case 0:
        change_brightness(dir);
        break;
    case 1:
        auto_dim_enabled = (dir > 0);
        if (!auto_dim_enabled && dimmed) {
            dimmed = false;
            brightness_apply();
        }
        settings_dirty = true;
        refresh_settings_ui();
        break;
    case 2:
        mirror_enabled = (dir > 0);
        settings_dirty = true;
        refresh_settings_ui();
        refresh_multiplayer_ui();
        break;
    case 3:
        mp_timer_duration_idx += dir;
        if (mp_timer_duration_idx < 0) mp_timer_duration_idx = 0;
        if (mp_timer_duration_idx >= MP_TIMER_OPTIONS_COUNT) mp_timer_duration_idx = MP_TIMER_OPTIONS_COUNT - 1;
        settings_dirty = true;
        refresh_settings_ui();
        // Update timer button visibility immediately
        goto_state(mp_timer_duration_idx > 0 ? MTIMER_IDLE : MTIMER_OFF);
        break;
    case 4:
        round_table_enabled = (dir > 0);
        settings_dirty = true;
        refresh_settings_ui();
        refresh_multiplayer_ui();
        mp_timer_update_highlight();
        break;
    case 5:
        i18n_set_language_index((i18n_get_language_index() + dir + LANG_COUNT) % LANG_COUNT);
        settings_dirty = true;
        refresh_texts_ui();
        break;
    }
}

static void change_multiplayer_life(int delta)
{
    if (multiplayer_selected < 0 || multiplayer_selected >= MULTIPLAYER_COUNT) return;
    // if (mirror_enabled && multiplayer_selected <= 1) delta = -delta;
    mp_poison_selected = -1;
    multiplayer_life[multiplayer_selected] = clamp_life(multiplayer_life[multiplayer_selected] + delta);
    mp_delta_acc[multiplayer_selected] += delta;
    show_mp_delta(multiplayer_selected);
    refresh_multiplayer_ui();
}

static void change_multiplayer_poison(int delta)
{
    int p = mp_poison_selected;
    if (p < 0 || p >= MULTIPLAYER_COUNT) return;
    int next = multiplayer_poison[p] + delta;
    if (next < 0) next = 0;
    if (next > POISON_MAX) next = POISON_MAX;
    multiplayer_poison[p] = next;
    refresh_multiplayer_ui();
}

static void change_main_poison(int delta)
{
    int next = multiplayer_poison[0] + delta;
    if (next < 0) next = 0;
    if (next > POISON_MAX) next = POISON_MAX;
    multiplayer_poison[0] = next;
    refresh_main_ui();
}

static void change_cmd_main_damage(int delta)
{
    int new_dmg, life_delta;
    if (cmd_main_selected < 0 || cmd_main_selected >= 3) return;
    new_dmg = cmd_main_damage[cmd_main_selected] + delta;
    if (new_dmg < 0) new_dmg = 0;
    life_delta = new_dmg - cmd_main_damage[cmd_main_selected];
    cmd_main_damage[cmd_main_selected] = new_dmg;
    life_total = clamp_life(life_total - life_delta);
    if (life_delta != 0) {
        life_delta_acc -= life_delta;
        show_life_delta();
    }
    refresh_main_ui();
}

static void change_cmd_mp_damage(int delta)
{
    int v, a, attacker, new_dmg, life_delta;

    v = cmd_mp_selected_victim;
    a = cmd_mp_selected_attacker;
    if (v < 0 || a < 0) return;
    if (mp_is_2p()) {
        attacker = mp2p_opponent(v);
    } else {
        if (a >= 3) return;
        attacker = cmp_mp_attackers[v][a];
    }
    new_dmg = multiplayer_cmd_damage_totals[v][attacker] + delta;
    if (new_dmg < 0) new_dmg = 0;
    life_delta = new_dmg - multiplayer_cmd_damage_totals[v][attacker];
    multiplayer_cmd_damage_totals[v][attacker] = new_dmg;
    multiplayer_life[v] = clamp_life(multiplayer_life[v] - life_delta);
    if (life_delta != 0) {
        mp_delta_acc[v] -= life_delta;
        show_mp_delta(v);
    }
    refresh_multiplayer_ui();
}

static void change_multiplayer_cmd_damage(int delta)
{
    int updated_total;

    if (multiplayer_cmd_target < 0 || multiplayer_cmd_target >= MULTIPLAYER_COUNT) return;
    if (multiplayer_cmd_source < 0 || multiplayer_cmd_source >= MULTIPLAYER_COUNT) return;

    updated_total = multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target] + delta;
    if (updated_total < 0) {
        delta = -multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target];
        updated_total = 0;
    }

    multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target] = updated_total;
    multiplayer_cmd_delta = updated_total;

    multiplayer_life[multiplayer_cmd_target] = clamp_life(multiplayer_life[multiplayer_cmd_target] - delta);
    refresh_multiplayer_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
}

static void change_multiplayer_all_damage(int delta)
{
    multiplayer_all_damage_value += delta;
    if (multiplayer_all_damage_value < 0) multiplayer_all_damage_value = 0;
    refresh_multiplayer_all_damage_ui();
}

static void mp_ensure_valid_selection(void)
{
    if (!mp_player_active(multiplayer_selected))
        multiplayer_selected = 1;
}

static void refresh_menu_focus_ui(void)
{
    char buf[8];
    lv_color_t players_border = (menu_focus == 0)
        ? lv_color_hex(0x8B5CF6) : lv_color_hex(0x4C1D95);
    lv_color_t bright_border = (menu_focus == 1)
        ? lv_color_hex(0x8B5CF6) : lv_color_hex(0x4C1D95);

    if (menu_players_row != NULL) {
        lv_obj_set_style_border_color(menu_players_row, players_border, 0);
        lv_obj_set_style_border_width(menu_players_row, menu_focus == 0 ? 2 : 1, 0);
    }
    if (menu_players_value != NULL) {
        snprintf(buf, sizeof(buf), "%d", game_player_count);
        lv_label_set_text(menu_players_value, buf);
    }
    if (menu_brightness_bar != NULL) {
        lv_obj_set_style_border_color(menu_brightness_bar, bright_border, LV_PART_MAIN);
        lv_obj_set_style_border_width(menu_brightness_bar, menu_focus == 1 ? 2 : 1, LV_PART_MAIN);
    }
    if (btn_menu_select_first != NULL) {
        if (game_player_count <= 1)
            lv_obj_add_flag(btn_menu_select_first, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_clear_flag(btn_menu_select_first, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_menu_player_count_ui(void)
{
    refresh_menu_focus_ui();
}

static void hide_main_menu(void);

static void change_game_player_count(int dir)
{
    int next = game_player_count;

    if (dir > 0) {
        if (next == 1) next = 2;
        else if (next == 2) next = 4;
    } else if (dir < 0) {
        if (next == 4) next = 2;
        else if (next == 2) next = 1;
    }
    if (next == game_player_count) return;
    game_player_count = next;
    apply_game_player_count();
}

static void apply_game_player_count(void)
{
    lv_obj_t *dest;
    bool menu_open;

    settings_dirty = true;
    mp_ensure_valid_selection();
    if (game_player_count == 1) {
        dest = screen_main;
        back_to_main();
    } else {
        dest = screen_multiplayer;
        open_multiplayer_screen();
    }
    menu_open = (menu_overlay != NULL &&
                 !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN));
    if (menu_open) {
        screen_before_menu = dest;
        lv_obj_move_foreground(menu_overlay);
    }
    refresh_multiplayer_ui();
    refresh_menu_player_count_ui();
    settings_save();
}

static void hide_main_menu(void)
{
    if (menu_overlay != NULL) {
        lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_main_menu(void)
{
    if (menu_overlay != NULL) {
        menu_focus = 0;
        refresh_menu_focus_ui();
        lv_obj_clear_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(menu_overlay);
    }
}


static void reset_all_values(void)
{
    int i;

    life_total = DEFAULT_LIFE_TOTAL;
    brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_life[i] = DEFAULT_LIFE_TOTAL;
        multiplayer_color[i] = (uint8_t)i;
        multiplayer_poison[i] = 0;
        snprintf(multiplayer_names[i], sizeof(multiplayer_names[i]), "P%d", i + 1);
    }
    mp_poison_selected = -1;
    multiplayer_selected = round_table_enabled ? 2 : 1;
    mp_ensure_valid_selection();
    multiplayer_menu_player = 0;
    multiplayer_cmd_source = 0;
    multiplayer_cmd_target = -1;
    multiplayer_cmd_delta = 0;
    memset(multiplayer_cmd_damage_totals, 0, sizeof(multiplayer_cmd_damage_totals));
    multiplayer_all_damage_value = 0;

    {
        int ri, rj;
        for (ri = 0; ri < 3; ri++) {
            cmd_main_damage[ri] = 0;
        }
        cmd_main_selected = -1;
        main_poison_selected = -1;
        set_cmd_mp_circle_selected(cmd_mp_selected_victim, cmd_mp_selected_attacker, false);
        cmd_mp_selected_victim = -1;
        cmd_mp_selected_attacker = -1;
        for (ri = 0; ri < MULTIPLAYER_COUNT; ri++)
            for (rj = 0; rj < 3; rj++)
                cmd_mp_label[ri][rj] = NULL;
    }

    {
        int di;
        life_delta_acc = 0;
        if (label_life_delta != NULL)
            lv_obj_add_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
        if (life_delta_hide_timer != NULL)
            lv_timer_pause(life_delta_hide_timer);
        for (di = 0; di < MULTIPLAYER_COUNT; di++) {
            mp_delta_acc[di] = 0;
        }
        if (mp_delta_hide_timer != NULL)
            lv_timer_pause(mp_delta_hide_timer);
    }

    goto_state(mp_timer_duration_idx > 0 ? MTIMER_IDLE : MTIMER_OFF);

    settings_dirty = true;
    players_persist_save();
    brightness_apply();
    refresh_main_ui();
    refresh_settings_ui();
    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_color_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_multiplayer_all_damage_ui();
}


static void intro_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *dest;

    (void)timer;
    if (intro_timer != NULL) {
        lv_timer_pause(intro_timer);
    }
    dest = (game_player_count == 1) ? screen_main : screen_multiplayer;
    if (game_player_count != 1)
        mp_ensure_valid_selection();
    lv_scr_load_anim(dest, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, false);
}

// ----------------------------------------------------
// navigation
// ----------------------------------------------------

static void load_screen_if_needed(lv_obj_t *screen)
{
    if (screen != NULL && lv_scr_act() != screen) {
        lv_scr_load(screen);
    }
}

static void open_settings_screen()
{
    screen_before_settings = lv_scr_act();
    settings_selected_field = 0;
    update_battery_measurement(true);
    refresh_settings_ui();
    load_screen_if_needed(screen_settings);
}

static void open_multiplayer_screen()
{
    mp_ensure_valid_selection();
    refresh_multiplayer_ui();
    load_screen_if_needed(screen_multiplayer);
}

static void open_multiplayer_menu_screen(int player_index)
{
    multiplayer_menu_player = player_index;
    refresh_multiplayer_menu_ui();
    load_screen_if_needed(screen_multiplayer_menu);
}

static void open_multiplayer_name_screen(void)
{
    refresh_multiplayer_name_ui();
    load_screen_if_needed(screen_multiplayer_name);
}

static void open_multiplayer_color_screen(void)
{
    refresh_multiplayer_color_ui();
    load_screen_if_needed(screen_multiplayer_color);
}

static void open_multiplayer_cmd_select_screen(void)
{
    refresh_multiplayer_cmd_select_ui();
    load_screen_if_needed(screen_multiplayer_cmd_select);
}

static void open_multiplayer_cmd_damage_screen(int target_index)
{
    multiplayer_cmd_source = target_index;
    multiplayer_cmd_target = multiplayer_menu_player;
    multiplayer_cmd_delta = multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target];
    refresh_multiplayer_cmd_damage_ui();
    load_screen_if_needed(screen_multiplayer_cmd_damage);
}

static void open_multiplayer_all_damage_screen(void)
{
    multiplayer_all_damage_value = 0;
    refresh_multiplayer_all_damage_ui();
    load_screen_if_needed(screen_multiplayer_all_damage);
}


static void back_to_main()
{
    refresh_main_ui();
    load_screen_if_needed(screen_main);
}

// ----------------------------------------------------
// events
// ----------------------------------------------------


static void event_settings_select_row(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < SETTINGS_FIELD_COUNT && settings_field_visible(idx)) {
        settings_selected_field = idx;
        refresh_settings_ui();
    }
}

static void event_settings_back(lv_event_t *e)
{
    (void)e;
    settings_save();
    if (screen_before_settings != NULL && screen_before_settings != screen_settings) {
        lv_obj_t *dest = screen_before_settings;
        screen_before_settings = NULL;
        lv_scr_load(dest);
    } else {
        back_to_main();
    }
}


static void event_hide_main_menu(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
}

static void event_menu_back(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
    if (screen_before_menu != NULL && screen_before_menu != screen_settings) {
        lv_obj_t *dest = screen_before_menu;
        screen_before_menu = NULL;
        lv_scr_load(dest);
    } else {
        back_to_main();
    }
}

static void event_menu_reset(lv_event_t *e)
{
    (void)e;
    lv_obj_t *src = screen_before_menu;
    screen_before_menu = NULL;
    hide_main_menu();
    reset_all_values();
    if (src == screen_multiplayer) {
        open_multiplayer_screen();
    }
    /* else stays on screen_main which reset_all_values already refreshed */
}


static void event_menu_focus_players(lv_event_t *e)
{
    (void)e;
    menu_focus = 0;
    refresh_menu_focus_ui();
    activity_kick();
}

static void event_menu_focus_brightness(lv_event_t *e)
{
    (void)e;
    menu_focus = 1;
    refresh_menu_focus_ui();
    activity_kick();
}

static void event_menu_select_first_player(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
    set_cmd_mp_circle_selected(cmd_mp_selected_victim, cmd_mp_selected_attacker, false);
    cmd_mp_selected_victim = -1;
    cmd_mp_selected_attacker = -1;
    open_multiplayer_screen();
    goto_state(MTIMER_SPINNING);
}

static void event_cmd_main_circle(lv_event_t *e)
{
    (void)e;
}

static void sp_handle_ring_slice(int slot)
{
    if (slot < 0 || slot >= SP_RING_SLOTS) return;

    if (slot == MP_SLOT_INFECT) {
        cmd_main_selected = -1;
        main_poison_selected = (main_poison_selected >= 0) ? -1 : 0;
    } else {
        int a = slot - 1;
        main_poison_selected = -1;
        cmd_main_selected = (cmd_main_selected == a) ? -1 : a;
    }
    refresh_main_ui();
    activity_kick();
}

static void event_sp_ring_slice(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    sp_handle_ring_slice(idx);
}

static void event_main_center_menu(lv_event_t *e)
{
    (void)e;
    if (menu_overlay != NULL && !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) return;
    activity_kick();
    open_multiplayer_menu_screen(0);
}

static void back_from_player_menu(void)
{
    players_persist_save();
    if (game_player_count == 1) back_to_main();
    else open_multiplayer_screen();
}

static void event_main_battery_menu(lv_event_t *e)
{
    (void)e;
    if (menu_overlay != NULL && !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) return;
    activity_kick();
    screen_before_menu = lv_scr_act();
    show_main_menu();
}

static void mp_handle_ring_slice(int player, int slot)
{
    if (player < 0 || player >= MULTIPLAYER_COUNT) return;
    if (!mp_player_active(player)) return;
    if (slot < 0 || slot >= mp_ring_slots()) return;

    multiplayer_selected = player;
    if (mp_timer_state == MTIMER_WAITING) {
        mp_timer_current_player = player;
        mp_timer_select_player = player;
        mp_timer_refresh_btn_label();
    }

    if (slot == MP_SLOT_INFECT) {
        if (cmd_mp_selected_victim >= 0) {
            set_cmd_mp_circle_selected(cmd_mp_selected_victim, cmd_mp_selected_attacker, false);
            cmd_mp_selected_victim = -1;
            cmd_mp_selected_attacker = -1;
        }
        mp_poison_selected = (mp_poison_selected == player) ? -1 : player;
    } else {
        int a = slot - 1;
        mp_poison_selected = -1;
        if (cmd_mp_selected_victim == player && cmd_mp_selected_attacker == a) {
            set_cmd_mp_circle_selected(player, a, false);
            cmd_mp_selected_victim = -1;
            cmd_mp_selected_attacker = -1;
        } else {
            set_cmd_mp_circle_selected(cmd_mp_selected_victim, cmd_mp_selected_attacker, false);
            cmd_mp_selected_victim = player;
            cmd_mp_selected_attacker = a;
            set_cmd_mp_circle_selected(player, a, true);
        }
    }
    refresh_multiplayer_ui();
}

static void event_mp_ring_slice(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    mp_handle_ring_slice(idx / 4, idx % 4);
    activity_kick();
}

static void event_menu_settings(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
    open_settings_screen();
}

static void event_battery_open_menu(lv_event_t *e)
{
    (void)e;
    if (menu_overlay != NULL && !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) return;
    activity_kick();
    screen_before_menu = lv_scr_act();
    show_main_menu();
}



static void event_multiplayer_select(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    int player = (int)(intptr_t)lv_event_get_user_data(e);

    if (indev != NULL) {
        lv_point_t pt;
        int rp, slot;
        lv_indev_get_point(indev, &pt);
        if (mp_resolve_ring_click(pt.x, pt.y, &player, &slot)) {
            mp_handle_ring_slice(player, slot);
            activity_kick();
            return;
        }
        if (round_table_enabled) {
            rp = mp_round_player_from_point(pt.x, pt.y);
            if (rp >= 0) player = rp;
        }
    }

    multiplayer_selected = player;
    mp_poison_selected = -1;
    if (mp_timer_state == MTIMER_WAITING) {
        mp_timer_current_player = multiplayer_selected;
        mp_timer_select_player = multiplayer_selected;
        mp_timer_refresh_btn_label();
    }
    if (cmd_mp_selected_victim >= 0) {
        set_cmd_mp_circle_selected(cmd_mp_selected_victim, cmd_mp_selected_attacker, false);
        cmd_mp_selected_victim = -1;
        cmd_mp_selected_attacker = -1;
    }
    refresh_multiplayer_ui();
}

static void event_multiplayer_open_menu(lv_event_t *e)
{
    multiplayer_selected = (int)(intptr_t)lv_event_get_user_data(e);
    refresh_multiplayer_ui();
    open_multiplayer_menu_screen(multiplayer_selected);
}

static void event_menu_swipe(lv_event_t *e)
{
    lv_point_t point;
    lv_indev_t *indev = lv_indev_get_act();
    lv_coord_t dy;

    if (indev == NULL) return;
    if (menu_overlay != NULL && !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) return;

    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &multiplayer_swipe_start);
        multiplayer_swipe_tracking = true;
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_RELEASED && multiplayer_swipe_tracking) {
        multiplayer_swipe_tracking = false;
        lv_indev_get_point(indev, &point);
        dy = point.y - multiplayer_swipe_start.y;
        if (LV_ABS(dy) > 80 &&
            LV_ABS(point.x - multiplayer_swipe_start.x) < 90) {
            screen_before_menu = lv_scr_act();
            show_main_menu();
        }
    }
}

static void event_multiplayer_menu_back(lv_event_t *e)
{
    (void)e;
    back_from_player_menu();
}

static void event_multiplayer_menu_rename(lv_event_t *e)
{
    (void)e;
    open_multiplayer_name_screen();
}

static void event_multiplayer_menu_pick_color(lv_event_t *e)
{
    (void)e;
    open_multiplayer_color_screen();
}

static void event_mp_color_swatch(lv_event_t *e)
{
    int palette_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (palette_idx < 0 || palette_idx >= PLAYER_PALETTE_COUNT) return;
    if (is_palette_color_taken(palette_idx, multiplayer_menu_player)) return;
    multiplayer_color[multiplayer_menu_player] = (uint8_t)palette_idx;
    players_persist_save();
    refresh_main_ui();
    refresh_multiplayer_ui();
    refresh_multiplayer_color_ui();
    activity_kick();
}

static void event_multiplayer_color_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player);
}

static void event_multiplayer_menu_cmd_damage(lv_event_t *e)
{
    (void)e;
    open_multiplayer_cmd_select_screen();
}

static void event_multiplayer_menu_all_damage(lv_event_t *e)
{
    (void)e;
    open_multiplayer_all_damage_screen();
}

static void event_multiplayer_name_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player);
}

static void event_multiplayer_name_save(lv_event_t *e)
{
    const char *txt;
    size_t len;

    (void)e;
    if (textarea_multiplayer_name == NULL) return;

    txt = lv_textarea_get_text(textarea_multiplayer_name);
    len = strlen(txt);
    if (len == 0) {
        snprintf(multiplayer_names[multiplayer_menu_player], sizeof(multiplayer_names[multiplayer_menu_player]),
                 "P%d", multiplayer_menu_player + 1);
    } else {
        snprintf(multiplayer_names[multiplayer_menu_player],
                 sizeof(multiplayer_names[multiplayer_menu_player]), "%s", txt);
    }

    settings_dirty = true;
    players_persist_save();
    refresh_main_ui();
    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_color_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    open_multiplayer_menu_screen(multiplayer_menu_player);
}

static void event_multiplayer_cmd_select_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player);
}

static void event_multiplayer_cmd_target_pick(lv_event_t *e)
{
    int row = (int)(intptr_t)lv_event_get_user_data(e);

    if (row < 0 || row >= (MULTIPLAYER_COUNT - 1)) return;
    if (multiplayer_cmd_target_choices[row] < 0) return;

    open_multiplayer_cmd_damage_screen(multiplayer_cmd_target_choices[row]);
}

static void event_multiplayer_cmd_damage_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_cmd_select_screen();
}

static void event_multiplayer_all_damage_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player);
}

static void event_multiplayer_all_damage_apply(lv_event_t *e)
{
    int i;

    (void)e;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_life[i] = clamp_life(multiplayer_life[i] - multiplayer_all_damage_value);
    }

    refresh_multiplayer_ui();
    open_multiplayer_screen();
}

void knob_cb(lv_event_t *e)
{
    (void)e;
}

// ----------------------------------------------------
// screen builders
// ----------------------------------------------------

static void intro_set_opa(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void intro_set_translate_y(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t *)obj, (lv_coord_t)v, 0);
}

static void build_unregistered_screen(void)
{
    lv_obj_t *title;
    lv_obj_t *detail;

    screen_unregistered = lv_obj_create(NULL);
    lv_obj_set_size(screen_unregistered, 360, 360);
    lv_obj_set_style_bg_color(screen_unregistered, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_unregistered, 0, 0);
    lv_obj_set_scrollbar_mode(screen_unregistered, LV_SCROLLBAR_MODE_OFF);

    title = lv_label_create(screen_unregistered);
    lv_label_set_text(title, "ArcMind");
    lv_obj_set_style_text_color(title, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -36);

    detail = lv_label_create(screen_unregistered);
    lv_label_set_text(detail, "Device not registered.\nRun make register.");
    lv_obj_set_style_text_color(detail, lv_color_hex(0x958DAC), 0);
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(detail, LV_ALIGN_CENTER, 0, 24);
}

static void build_intro_screen()
{
    lv_anim_t a;

    screen_intro = lv_obj_create(NULL);
    lv_obj_set_size(screen_intro, 360, 360);
    lv_obj_set_style_bg_color(screen_intro, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_intro, 0, 0);
    lv_obj_set_scrollbar_mode(screen_intro, LV_SCROLLBAR_MODE_OFF);

    intro_img = lv_img_create(screen_intro);
    lv_img_set_src(intro_img, &arcmind_logo);
    lv_obj_align(intro_img, LV_ALIGN_CENTER, 0, -28);

    intro_label_name = lv_label_create(screen_intro);
    lv_label_set_text(intro_label_name, "ArcMind");
    lv_obj_set_style_text_color(intro_label_name, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(intro_label_name, &lv_font_montserrat_36, 0);
    lv_obj_align(intro_label_name, LV_ALIGN_CENTER, 0, 76);

    /* Start both elements invisible, offset downward via translate_y.
     * Using translate_y avoids reading coords from an inactive screen,
     * which is unreliable in LVGL 8.3 before lv_scr_load is called. */
    lv_obj_set_style_opa(intro_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_y(intro_img, 24, 0);
    lv_obj_set_style_opa(intro_label_name, LV_OPA_TRANSP, 0);
    lv_obj_set_style_translate_y(intro_label_name, 12, 0);

    /* Logo: slide up + fade in — 700ms, ease-out */
    lv_anim_init(&a);
    lv_anim_set_var(&a, intro_img);
    lv_anim_set_exec_cb(&a, intro_set_translate_y);
    lv_anim_set_values(&a, 24, 0);
    lv_anim_set_time(&a, 700);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, intro_set_opa);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_start(&a);

    /* Text: slide up + fade in — 600ms, 350ms delay, ease-out */
    lv_anim_init(&a);
    lv_anim_set_var(&a, intro_label_name);
    lv_anim_set_exec_cb(&a, intro_set_translate_y);
    lv_anim_set_values(&a, 12, 0);
    lv_anim_set_time(&a, 600);
    lv_anim_set_delay(&a, 350);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, intro_set_opa);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_start(&a);
}


static int mp_timer_next_living_player(int from)
{
    int next = (from + 1) % MULTIPLAYER_COUNT;
    int i;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (mp_player_active(next) && !is_player_dead(next)) return next;
        next = (next + 1) % MULTIPLAYER_COUNT;
    }
    return from; // all dead
}

static int mp_timer_prev_living_player(int from)
{
    int prev = (from + MULTIPLAYER_COUNT - 1) % MULTIPLAYER_COUNT;
    int i;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (mp_player_active(prev) && !is_player_dead(prev)) return prev;
        prev = (prev + MULTIPLAYER_COUNT - 1) % MULTIPLAYER_COUNT;
    }
    return from; // all dead
}

static void mp_timer_update_highlight(void)
{
    int i;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        switch (mp_timer_state) {
        case MTIMER_SELECTING:
        case MTIMER_SPINNING:
            set_multiplayer_area_opa(i, (i == mp_timer_select_player) ? LV_OPA_50 : LV_OPA_10);
            break;
        case MTIMER_WAITING:
            /* Normal selection tint — strong overlay only while picking/spinning */
            set_multiplayer_area_opa(i, (i == multiplayer_selected) ? LV_OPA_20 : LV_OPA_10);
            break;
        case MTIMER_RUNNING:
        case MTIMER_EXPIRED:
            set_multiplayer_area_opa(
                i,
                (i == mp_timer_current_player) ? LV_OPA_40
                                               : ((i == multiplayer_selected) ? LV_OPA_20 : LV_OPA_10)
            );
            break;
        default:
            /* OFF/IDLE: respect normal multiplayer selection highlight */
            set_multiplayer_area_opa(i, (i == multiplayer_selected) ? LV_OPA_20 : LV_OPA_10);
            break;
        }
    }
}

static void mp_timer_refresh_arc(void)
{
    if (arc_mp_timer == NULL) return;
    int dur_ms = MP_TIMER_OPTIONS[mp_timer_duration_idx] * 1000;
    if (dur_ms <= 0) return;

    int pct = (mp_timer_remaining_ms * 100) / dur_ms;
    lv_color_t c;
    if (pct > 50)      c = lv_color_hex(0x7C3AED);
    else if (pct > 15) c = lv_color_hex(0xEAB308);
    else               c = lv_color_hex(0xEF4444);
    lv_obj_set_style_arc_color(arc_mp_timer, c, LV_PART_INDICATOR);

    int arc_val = (mp_timer_remaining_ms * 1000) / dur_ms;
    lv_arc_set_value(arc_mp_timer, arc_val);
}

static void mp_timer_refresh_btn_label(void)
{
    char buf[16];
    int dur_s;
    int rem_s;

    if (label_mp_timer_btn == NULL) return;

    switch (mp_timer_state) {
    case MTIMER_OFF:
        break;
    case MTIMER_IDLE:
        dur_s = MP_TIMER_OPTIONS[mp_timer_duration_idx];
        snprintf(buf, sizeof(buf), "%d:%02d", dur_s / 60, dur_s % 60);
        lv_label_set_text(label_mp_timer_btn, buf);
        lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0x8B5CF6), 0);
        break;
    case MTIMER_SELECTING:
        snprintf(buf, sizeof(buf), "P%d?", mp_timer_select_player + 1);
        lv_label_set_text(label_mp_timer_btn, buf);
        lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0xC4B5FD), 0);
        break;
    case MTIMER_SPINNING:
        lv_label_set_text(label_mp_timer_btn, "...");
        lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0xC4B5FD), 0);
        break;
    case MTIMER_WAITING:
        snprintf(buf, sizeof(buf), i18n_get(I18N_GO_PLAYER_FMT), mp_timer_current_player + 1);
        lv_label_set_text(label_mp_timer_btn, buf);
        lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0x8B5CF6), 0);
        break;
    case MTIMER_RUNNING:
        rem_s = (mp_timer_remaining_ms + 999) / 1000;
        snprintf(buf, sizeof(buf), "%d:%02d", rem_s / 60, rem_s % 60);
        lv_label_set_text(label_mp_timer_btn, buf);
        lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0xC4B5FD), 0);
        break;
    case MTIMER_EXPIRED:
        lv_label_set_text(label_mp_timer_btn, i18n_get(I18N_TIME_EXPIRED));
        lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0xEF4444), 0);
        break;
    }
}

static void mp_timer_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    if (mp_timer_state != MTIMER_RUNNING) return;

    mp_timer_remaining_ms -= 100;
    if (mp_timer_remaining_ms <= 0) {
        mp_timer_remaining_ms = 0;
        goto_state(MTIMER_EXPIRED);
        return;
    }
    mp_timer_refresh_arc();
    mp_timer_refresh_btn_label();
}

static void mp_timer_blink_cb(lv_timer_t *timer)
{
    (void)timer;
    int idx;
    mp_timer_blink_visible = !mp_timer_blink_visible;

    if (mp_timer_state == MTIMER_SELECTING || mp_timer_state == MTIMER_SPINNING) {
        idx = mp_timer_select_player;
        set_multiplayer_area_opa(idx, mp_timer_blink_visible ? LV_OPA_50 : LV_OPA_10);
    } else if (mp_timer_state == MTIMER_RUNNING) {
        idx = mp_timer_current_player;
        set_multiplayer_area_opa(idx, mp_timer_blink_visible ? LV_OPA_40 : LV_OPA_10);
    } else if (mp_timer_state == MTIMER_EXPIRED) {
        idx = mp_timer_current_player;
        set_multiplayer_area_opa(idx, mp_timer_blink_visible ? LV_OPA_60 : LV_OPA_10);
        if (label_mp_timer_btn != NULL)
            lv_obj_set_style_text_color(label_mp_timer_btn,
                mp_timer_blink_visible ? lv_color_hex(0xEF4444) : lv_color_hex(0x4A0F0F), 0);
    }
}

static const uint16_t SPIN_DELAYS[] = {60,60,60,60,80,80,80,100,120,160,210,280,370,480,0};

static void mp_timer_spin_cb(lv_timer_t *timer)
{
    if (SPIN_DELAYS[mp_spin_step] == 0) {
        // Sequence finished — snap to predetermined target
        mp_timer_select_player = mp_spin_target;
        mp_timer_update_highlight();
        lv_timer_pause(timer);
        mp_timer_current_player = mp_spin_target;
        multiplayer_selected    = mp_spin_target;
        goto_state(MTIMER_WAITING);
        return;
    }
    mp_timer_select_player = mp_timer_next_living_player(mp_timer_select_player);
    mp_timer_update_highlight();
    lv_timer_set_period(timer, SPIN_DELAYS[mp_spin_step]);
    mp_spin_step++;
}

static void goto_state(mp_timer_state_t new_state)
{
    mp_timer_state = new_state;

    // Stop all running timers
    if (mp_timer_tick  != NULL) lv_timer_pause(mp_timer_tick);
    if (mp_timer_blink != NULL) lv_timer_pause(mp_timer_blink);
    if (mp_timer_spin_tmr != NULL) lv_timer_pause(mp_timer_spin_tmr);

    switch (new_state) {
    case MTIMER_OFF:
        if (arc_mp_timer != NULL) lv_obj_add_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
        if (btn_mp_timer != NULL) lv_obj_add_flag(btn_mp_timer, LV_OBJ_FLAG_HIDDEN);
        break;

    case MTIMER_IDLE:
        if (arc_mp_timer != NULL) lv_obj_add_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
        if (btn_mp_timer != NULL) {
            lv_obj_clear_flag(btn_mp_timer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(btn_mp_timer);
        }
        mp_timer_current_player = -1;
        mp_timer_remaining_ms = MP_TIMER_OPTIONS[mp_timer_duration_idx] * 1000;
        mp_timer_refresh_btn_label();
        break;

    case MTIMER_SELECTING:
        if (arc_mp_timer != NULL) lv_obj_add_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
        mp_timer_select_moved = false;
        mp_timer_select_player = (mp_timer_current_player >= 0)
            ? mp_timer_next_living_player(mp_timer_current_player)
            : 0;
        mp_timer_update_highlight();
        mp_timer_blink_visible = true;
        if (mp_timer_blink != NULL) {
            lv_timer_set_period(mp_timer_blink, 200);
            lv_timer_resume(mp_timer_blink);
        }
        mp_timer_refresh_btn_label();
        break;

    case MTIMER_SPINNING:
        if (arc_mp_timer != NULL) lv_obj_add_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
        {
            int living[MULTIPLAYER_COUNT], n = 0, j;
            for (j = 0; j < MULTIPLAYER_COUNT; j++)
                if (mp_player_active(j) && !is_player_dead(j)) living[n++] = j;
            if (n == 0) {
                mp_timer_state = MTIMER_IDLE;
                mp_timer_update_highlight();
                mp_timer_refresh_btn_label();
                return;
            }
            mp_spin_target = living[(int)((uint32_t)esp_random() % (uint32_t)n)];
        }
        mp_spin_step = 0;
        mp_timer_update_highlight();
        if (mp_timer_spin_tmr != NULL) {
            lv_timer_set_period(mp_timer_spin_tmr, SPIN_DELAYS[0]);
            lv_timer_resume(mp_timer_spin_tmr);
        }
        mp_timer_refresh_btn_label();
        break;

    case MTIMER_WAITING:
        if (arc_mp_timer != NULL) lv_obj_add_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
        mp_timer_remaining_ms = MP_TIMER_OPTIONS[mp_timer_duration_idx] * 1000;
        mp_timer_update_highlight();
        mp_timer_refresh_btn_label();
        break;

    case MTIMER_RUNNING:
        if (arc_mp_timer != NULL) {
            lv_obj_clear_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
            lv_arc_set_value(arc_mp_timer, 1000);
            lv_obj_set_style_arc_color(arc_mp_timer, lv_color_hex(0x7C3AED), LV_PART_INDICATOR);
        }
        mp_timer_blink_visible = true;
        mp_timer_update_highlight();
        if (mp_timer_tick  != NULL) lv_timer_resume(mp_timer_tick);
        if (mp_timer_blink != NULL) {
            lv_timer_set_period(mp_timer_blink, 600);
            lv_timer_resume(mp_timer_blink);
        }
        mp_timer_refresh_btn_label();
        break;

    case MTIMER_EXPIRED:
        if (arc_mp_timer != NULL) {
            lv_arc_set_value(arc_mp_timer, 0);
            lv_obj_set_style_arc_color(arc_mp_timer, lv_color_hex(0xEF4444), LV_PART_INDICATOR);
            lv_obj_clear_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);
        }
        mp_timer_blink_visible = true;
        mp_timer_update_highlight();
        if (mp_timer_blink != NULL) {
            lv_timer_set_period(mp_timer_blink, 200);
            lv_timer_resume(mp_timer_blink);
        }
        mp_timer_refresh_btn_label();
        break;
    default:
        break;
    }

    if (new_state == MTIMER_OFF || new_state == MTIMER_IDLE)
        mp_timer_update_highlight();
}

static void mp_timer_btn_pressed_cb(lv_event_t *e)
{
    int next;
    (void)e;
    switch (mp_timer_state) {
    case MTIMER_IDLE:
        goto_state(MTIMER_SELECTING);
        break;
    case MTIMER_SELECTING:
        if (mp_timer_select_moved) {
            mp_timer_current_player = mp_timer_select_player;
            multiplayer_selected    = mp_timer_current_player;
            goto_state(MTIMER_WAITING);
        } else {
            goto_state(MTIMER_SPINNING);
        }
        break;
    case MTIMER_WAITING:
        goto_state(MTIMER_RUNNING);
        break;
    case MTIMER_RUNNING:
        next = mp_timer_next_living_player(mp_timer_current_player);
        mp_timer_current_player = next;
        multiplayer_selected    = next;
        mp_timer_remaining_ms   = MP_TIMER_OPTIONS[mp_timer_duration_idx] * 1000;
        goto_state(MTIMER_RUNNING);
        break;
    case MTIMER_EXPIRED:
        next = mp_timer_next_living_player(mp_timer_current_player);
        mp_timer_current_player = next;
        multiplayer_selected    = next;
        mp_timer_remaining_ms   = MP_TIMER_OPTIONS[mp_timer_duration_idx] * 1000;
        goto_state(MTIMER_RUNNING);
        break;
    default:
        break;
    }
}

static void build_multiplayer_screen()
{
    static const char *player_names[MULTIPLAYER_COUNT] = {"P1", "P2", "P3", "P4"};
    static const lv_coord_t quad_x[MULTIPLAYER_COUNT] = {0, 180, 180, 0};
    static const lv_coord_t quad_y[MULTIPLAYER_COUNT] = {0, 0, 180, 180};
    static const uint16_t round_arc_start[MULTIPLAYER_COUNT] = {135, 225, 315, 45};
    static const uint16_t round_arc_end[MULTIPLAYER_COUNT] = {225, 315, 45, 135};
    int i;

    screen_multiplayer = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(screen_multiplayer, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_multiplayer, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    mp_battery_panel = lv_obj_create(screen_multiplayer);
    lv_obj_set_size(mp_battery_panel, 68, 68);
    lv_obj_center(mp_battery_panel);
    lv_obj_set_style_bg_opa(mp_battery_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mp_battery_panel, 0, 0);
    lv_obj_set_style_pad_all(mp_battery_panel, 0, 0);
    lv_obj_add_flag(mp_battery_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(mp_battery_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mp_battery_panel, event_battery_open_menu, LV_EVENT_CLICKED, NULL);

    arc_multiplayer_battery = make_center_battery_arc(mp_battery_panel);
    lv_obj_center(arc_multiplayer_battery);

    label_multiplayer_battery = lv_label_create(mp_battery_panel);
    lv_label_set_text(label_multiplayer_battery, "--%");
    lv_obj_set_style_text_color(label_multiplayer_battery, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_multiplayer_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(label_multiplayer_battery, LV_ALIGN_CENTER, 0, BATT_HUB_PCT_Y);
    lv_obj_clear_flag(label_multiplayer_battery, LV_OBJ_FLAG_CLICKABLE);

    label_multiplayer_battery_charge = lv_label_create(mp_battery_panel);
    lv_label_set_text(label_multiplayer_battery_charge, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(label_multiplayer_battery_charge, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_multiplayer_battery_charge, lv_color_hex(0xFACC15), 0);
    lv_obj_align(label_multiplayer_battery_charge, LV_ALIGN_CENTER, 0, BATT_HUB_ICON_Y);
    lv_obj_add_flag(label_multiplayer_battery_charge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_multiplayer_battery_charge, LV_OBJ_FLAG_CLICKABLE);

    label_multiplayer_battery_settings = lv_label_create(mp_battery_panel);
    lv_label_set_text(label_multiplayer_battery_settings, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(label_multiplayer_battery_settings, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_multiplayer_battery_settings, lv_color_hex(0x8B5CF6), 0);
    lv_obj_align(label_multiplayer_battery_settings, LV_ALIGN_CENTER, 0, BATT_HUB_ICON_Y);
    lv_obj_add_flag(label_multiplayer_battery_settings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_multiplayer_battery_settings, LV_OBJ_FLAG_CLICKABLE);

    /* Ring: 4 slices per player — slot 0 black poison arc, 1-3 commander arcs */
    {
        int v, s;
        for (v = 0; v < MULTIPLAYER_COUNT; v++) {
            for (s = 0; s < 4; s++) {
                mp_ring_seg[v][s] = lv_arc_create(screen_multiplayer);
                lv_obj_set_size(mp_ring_seg[v][s], MP_RING_DIAM, MP_RING_DIAM);
                lv_obj_center(mp_ring_seg[v][s]);
                lv_arc_set_rotation(mp_ring_seg[v][s], MP_RING_ROTATION);
                lv_obj_remove_style(mp_ring_seg[v][s], NULL, LV_PART_KNOB);
                lv_obj_set_style_arc_width(mp_ring_seg[v][s], MP_RING_WIDTH, LV_PART_MAIN);
                lv_obj_set_style_arc_width(mp_ring_seg[v][s], 0, LV_PART_INDICATOR);
                lv_obj_set_style_arc_rounded(mp_ring_seg[v][s], false, LV_PART_MAIN);
                lv_obj_clear_flag(mp_ring_seg[v][s], LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }

    /* Touch targets — one per ring slice (arcs are visual only) */
    {
        int v, s;
        mp_slot_pos_t pos;
        for (v = 0; v < MULTIPLAYER_COUNT; v++) {
            for (s = 0; s < 4; s++) {
                lv_obj_t *hit;
                mp_ring_hit_pos(v, s, &pos);
                hit = lv_btn_create(screen_multiplayer);
                lv_obj_set_size(hit, MP_RING_HIT_SIZE, MP_RING_HIT_SIZE);
                lv_obj_set_pos(hit, pos.x, pos.y);
                lv_obj_set_style_radius(hit, 3, 0);
                lv_obj_set_style_shadow_width(hit, 0, 0);
                lv_obj_set_style_pad_all(hit, 0, 0);
                lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_opa(hit, LV_OPA_TRANSP, 0);
                lv_obj_add_event_cb(hit, event_mp_ring_slice, LV_EVENT_CLICKED,
                                    (void *)(intptr_t)(v * 4 + s));
                btn_mp_ring_hit[v][s] = hit;
            }
        }
    }

    /* Infect counters — canvas (icon + count), lv_img_set_angle for mirror/round */
    {
        int v;
        mp_slot_pos_t pos;
        for (v = 0; v < MULTIPLAYER_COUNT; v++) {
            canvas_poison_mp_buf[v] = (uint8_t *)heap_caps_malloc(
                CMP_POISON_BUF_SIZE, MALLOC_CAP_SPIRAM);
            canvas_poison_mp[v] = lv_canvas_create(screen_multiplayer);
            lv_canvas_set_buffer(canvas_poison_mp[v], canvas_poison_mp_buf[v],
                                 CMP_POISON_W, CMP_POISON_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
            lv_img_set_pivot(canvas_poison_mp[v], CMP_POISON_W_HALF, CMP_POISON_H_HALF);
            get_mp_counter_slot_pos(v, MP_SLOT_INFECT, &pos);
            lv_obj_set_pos(canvas_poison_mp[v], pos.x, pos.y);
            lv_obj_clear_flag(canvas_poison_mp[v], LV_OBJ_FLAG_CLICKABLE);
        }
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_round_segments[i] = lv_arc_create(screen_multiplayer);
        lv_obj_set_size(multiplayer_round_segments[i], 358, 358);
        lv_obj_center(multiplayer_round_segments[i]);
        lv_arc_set_rotation(multiplayer_round_segments[i], MP_RING_ROTATION);
        lv_arc_set_bg_angles(multiplayer_round_segments[i], round_arc_start[i], round_arc_end[i]);
        lv_obj_remove_style(multiplayer_round_segments[i], NULL, LV_PART_KNOB);
        lv_obj_clear_flag(multiplayer_round_segments[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(multiplayer_round_segments[i], 179, LV_PART_MAIN);
        lv_obj_set_style_arc_width(multiplayer_round_segments[i], 0, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(multiplayer_round_segments[i], false, LV_PART_MAIN);
        lv_obj_set_style_arc_color(multiplayer_round_segments[i], get_player_active_color(i), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(multiplayer_round_segments[i], LV_OPA_10, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(multiplayer_round_segments[i], LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_add_flag(multiplayer_round_segments[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        round_wedge_hit[i] = lv_btn_create(screen_multiplayer);
        lv_obj_set_size(round_wedge_hit[i], ROUND_WEDGE_BTN_SIZE, ROUND_WEDGE_BTN_SIZE);
        lv_obj_set_pos(round_wedge_hit[i], ROUND_WEDGE_BTN_XY[i][0], ROUND_WEDGE_BTN_XY[i][1]);
        lv_obj_set_style_bg_opa(round_wedge_hit[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(round_wedge_hit[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(round_wedge_hit[i], 0, 0);
        lv_obj_add_event_cb(round_wedge_hit[i], event_multiplayer_select, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_add_event_cb(round_wedge_hit[i], event_multiplayer_open_menu, LV_EVENT_LONG_PRESSED,
                            (void *)(intptr_t)i);
        lv_obj_add_event_cb(round_wedge_hit[i], event_menu_swipe, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(round_wedge_hit[i], event_menu_swipe, LV_EVENT_RELEASED, NULL);
        lv_obj_add_flag(round_wedge_hit[i], LV_OBJ_FLAG_HIDDEN);
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_quadrants[i] = lv_btn_create(screen_multiplayer);
        lv_obj_set_size(multiplayer_quadrants[i], 180, 180);
        lv_obj_set_pos(multiplayer_quadrants[i], quad_x[i], quad_y[i]);
        lv_obj_set_style_radius(multiplayer_quadrants[i], 0, 0);
        lv_obj_set_style_border_width(multiplayer_quadrants[i], 1, 0);
        lv_obj_set_style_border_color(multiplayer_quadrants[i], lv_color_black(), 0);
        lv_obj_set_style_shadow_width(multiplayer_quadrants[i], 0, 0);
        lv_obj_add_flag(multiplayer_quadrants[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(multiplayer_quadrants[i], event_multiplayer_select, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(multiplayer_quadrants[i], event_multiplayer_open_menu, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(multiplayer_quadrants[i], event_menu_swipe, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(multiplayer_quadrants[i], event_menu_swipe, LV_EVENT_RELEASED, NULL);

        canvas_mp_name_buf[i] = (uint8_t *)heap_caps_malloc(
            CMP_NAME_BUF_SIZE, MALLOC_CAP_SPIRAM);
        canvas_mp_name[i] = lv_canvas_create(screen_multiplayer);
        lv_canvas_set_buffer(canvas_mp_name[i], canvas_mp_name_buf[i],
                             CMP_NAME_W, CMP_NAME_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_img_set_pivot(canvas_mp_name[i], CMP_NAME_W / 2, CMP_NAME_H / 2);
        lv_obj_clear_flag(canvas_mp_name[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(canvas_mp_name[i], LV_OBJ_FLAG_HIDDEN);

        canvas_mp_life_buf[i] = (uint8_t *)heap_caps_malloc(
            CMP_LIFE_BUF_SIZE, MALLOC_CAP_SPIRAM);
        canvas_mp_life[i] = lv_canvas_create(screen_multiplayer);
        lv_canvas_set_buffer(canvas_mp_life[i], canvas_mp_life_buf[i],
                             CMP_LIFE_W, CMP_LIFE_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_img_set_pivot(canvas_mp_life[i], CMP_LIFE_W / 2, CMP_LIFE_H / 2);
        lv_obj_clear_flag(canvas_mp_life[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(canvas_mp_life[i], LV_OBJ_FLAG_HIDDEN);

        if (i < 2) {
            label_multiplayer_name[i] = NULL;
            label_multiplayer_life[i] = NULL;
        } else {
            /* P2, P3: plain labels, no rotation needed */
            label_multiplayer_name[i] = lv_label_create(screen_multiplayer);
            lv_label_set_text(label_multiplayer_name[i], player_names[i]);
            lv_obj_set_style_text_color(label_multiplayer_name[i], lv_color_white(), 0);
            lv_obj_set_style_text_font(label_multiplayer_name[i], &lv_font_montserrat_14_pt, 0);

            label_multiplayer_life[i] = lv_label_create(screen_multiplayer);
            lv_label_set_text(label_multiplayer_life[i], "40");
            lv_obj_set_style_text_color(label_multiplayer_life[i], lv_color_white(), 0);
            lv_obj_set_style_text_font(label_multiplayer_life[i], &lv_font_montserrat_36, 0);

        }
    }

    /* Single center-of-screen delta canvas — shared by all players */
    canvas_mp_delta_ctr_buf = (uint8_t *)heap_caps_malloc(CMP_DELTA_BUF_SIZE, MALLOC_CAP_SPIRAM);
    canvas_mp_delta_ctr = lv_canvas_create(screen_multiplayer);
    lv_canvas_set_buffer(canvas_mp_delta_ctr, canvas_mp_delta_ctr_buf,
                         CMP_DELTA_W, CMP_DELTA_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_img_set_pivot(canvas_mp_delta_ctr, CMP_DELTA_W / 2, CMP_DELTA_H / 2);
    lv_obj_clear_flag(canvas_mp_delta_ctr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas_mp_delta_ctr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(canvas_mp_delta_ctr, LV_ALIGN_CENTER, 0, 0);

    /* Skull overlays — one per quadrant, hidden until life <= 0 */
    {
        static const lv_coord_t skull_cx[MULTIPLAYER_COUNT] = {90, 270, 270, 90};
        static const lv_coord_t skull_cy[MULTIPLAYER_COUNT] = {90,  90, 270, 270};
        for (i = 0; i < MULTIPLAYER_COUNT; i++) {
            img_skull[i] = lv_img_create(screen_multiplayer);
            lv_img_set_src(img_skull[i], &dead_img);
            lv_obj_set_pos(img_skull[i], skull_cx[i] - 20, skull_cy[i] - 20);
            lv_obj_clear_flag(img_skull[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(img_skull[i], LV_OBJ_FLAG_HIDDEN);
            lv_img_set_pivot(img_skull[i], 20, 20);
        }
    }

    /* Countdown arc — behind everything, shown only during RUNNING/EXPIRED */
    arc_mp_timer = lv_arc_create(screen_multiplayer);
    lv_obj_set_size(arc_mp_timer, 354, 354);
    lv_obj_center(arc_mp_timer);
    lv_arc_set_rotation(arc_mp_timer, 270);
    lv_arc_set_bg_angles(arc_mp_timer, 0, 360);
    lv_arc_set_range(arc_mp_timer, 0, 1000);
    lv_arc_set_value(arc_mp_timer, 1000);
    lv_obj_remove_style(arc_mp_timer, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_mp_timer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc_mp_timer, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_mp_timer, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_mp_timer, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_mp_timer, lv_color_hex(0x7C3AED), LV_PART_INDICATOR);
    lv_obj_add_flag(arc_mp_timer, LV_OBJ_FLAG_HIDDEN);

    /* Timer button — shown when timer is not OFF */
    btn_mp_timer = lv_btn_create(screen_multiplayer);
    lv_obj_set_size(btn_mp_timer, 110, 34);
    lv_obj_align(btn_mp_timer, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_bg_color(btn_mp_timer, lv_color_hex(0x120F2D), 0);
    lv_obj_set_style_border_color(btn_mp_timer, lv_color_hex(0x7C3AED), 0);
    lv_obj_set_style_border_width(btn_mp_timer, 2, 0);
    lv_obj_set_style_radius(btn_mp_timer, 10, 0);
    lv_obj_set_style_shadow_width(btn_mp_timer, 0, 0);
    lv_obj_set_style_pad_all(btn_mp_timer, 0, 0);
    lv_obj_add_event_cb(btn_mp_timer, mp_timer_btn_pressed_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(btn_mp_timer, 10);
    lv_obj_add_flag(btn_mp_timer, LV_OBJ_FLAG_HIDDEN);

    label_mp_timer_btn = lv_label_create(btn_mp_timer);
    lv_label_set_text(label_mp_timer_btn, "");
    lv_obj_set_style_text_color(label_mp_timer_btn, lv_color_hex(0x8B5CF6), 0);
    lv_obj_set_style_text_font(label_mp_timer_btn, &lv_font_montserrat_14_pt, 0);
    lv_obj_center(label_mp_timer_btn);

    /* Commander damage labels on ring slices */
    {
        int v, a;
        mp_slot_pos_t pos;

        for (v = 0; v < MULTIPLAYER_COUNT; v++) {
            for (a = 0; a < 3; a++) {
                cmd_mp_circle[v][a] = NULL;
                cmd_mp_label[v][a] = NULL;
                canvas_cmd_mp_buf[v][a] = (uint8_t *)heap_caps_malloc(
                    CMP_CMD_BUF_SIZE, MALLOC_CAP_SPIRAM);
                canvas_cmd_mp[v][a] = lv_canvas_create(screen_multiplayer);
                lv_canvas_set_buffer(canvas_cmd_mp[v][a], canvas_cmd_mp_buf[v][a],
                                     CMP_CMD_W, CMP_CMD_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
                lv_img_set_pivot(canvas_cmd_mp[v][a], CMP_CMD_W / 2, CMP_CMD_H / 2);
                get_mp_counter_slot_pos(v, a + 1, &pos);
                lv_obj_set_pos(canvas_cmd_mp[v][a], pos.x, pos.y);
                lv_obj_clear_flag(canvas_cmd_mp[v][a], LV_OBJ_FLAG_CLICKABLE);
            }
        }
    }

    refresh_multiplayer_ui();
}

static void build_multiplayer_menu_screen()
{
    screen_multiplayer_menu = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_menu, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_menu, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_menu, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_menu, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_menu_title = lv_label_create(screen_multiplayer_menu);
    lv_obj_set_style_text_color(label_multiplayer_menu_title, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_multiplayer_menu_title, UI_FONT_22, 0);
    lv_obj_align(label_multiplayer_menu_title, LV_ALIGN_TOP_MID, 0, 22);

    mp_menu_panel = lv_obj_create(screen_multiplayer_menu);
    lv_obj_set_size(mp_menu_panel, 230, 230);
    lv_obj_align(mp_menu_panel, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_radius(mp_menu_panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(mp_menu_panel, lv_color_hex(0x13111F), 0);
    lv_obj_set_style_bg_opa(mp_menu_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(mp_menu_panel, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(mp_menu_panel, 2, 0);
    lv_obj_set_style_pad_all(mp_menu_panel, 0, 0);
    lv_obj_set_scrollbar_mode(mp_menu_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(mp_menu_panel, LV_OBJ_FLAG_CLICKABLE);

    btn_mp_menu_rename = make_button(mp_menu_panel, i18n_get(I18N_RENAME), 150, 36, event_multiplayer_menu_rename);
    lv_obj_align(btn_mp_menu_rename, LV_ALIGN_CENTER, 0, -34);
    style_arcmind_menu_button(btn_mp_menu_rename);

    btn_mp_menu_pick_color = make_button(mp_menu_panel, i18n_get(I18N_PICK_COLOR), 150, 36, event_multiplayer_menu_pick_color);
    lv_obj_align(btn_mp_menu_pick_color, LV_ALIGN_CENTER, 0, 22);
    style_arcmind_menu_button(btn_mp_menu_pick_color);

    btn_mp_menu_back = make_button(screen_multiplayer_menu, i18n_get(I18N_BACK), 50, 50, event_multiplayer_menu_back);
    lv_obj_align(btn_mp_menu_back, LV_ALIGN_CENTER, -148, 8);
    style_arcmind_side_button(btn_mp_menu_back);
}

static void build_multiplayer_color_screen()
{
    int pi;
    static const lv_coord_t grid_x[PLAYER_COLOR_COLS] = {-66, -22, 22, 66};
    static const lv_coord_t grid_y[3] = {-44, 0, 44};

    screen_multiplayer_color = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_color, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_color, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_color, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_color, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_color_title = lv_label_create(screen_multiplayer_color);
    lv_obj_set_style_text_color(label_multiplayer_color_title, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_multiplayer_color_title, UI_FONT_22, 0);
    lv_obj_align(label_multiplayer_color_title, LV_ALIGN_TOP_MID, 0, 22);

    for (pi = 0; pi < PLAYER_PALETTE_COUNT; pi++) {
        int col = pi % PLAYER_COLOR_COLS;
        int row = pi / PLAYER_COLOR_COLS;
        mp_color_swatch[pi] = lv_btn_create(screen_multiplayer_color);
        lv_obj_set_size(mp_color_swatch[pi], 36, 36);
        lv_obj_align(mp_color_swatch[pi], LV_ALIGN_CENTER,
                     grid_x[col], grid_y[row] + 16);
        lv_obj_set_style_radius(mp_color_swatch[pi], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(mp_color_swatch[pi], 0, 0);
        lv_obj_set_style_pad_all(mp_color_swatch[pi], 0, 0);
        lv_obj_add_event_cb(mp_color_swatch[pi], event_mp_color_swatch, LV_EVENT_CLICKED,
                            (void *)(intptr_t)pi);

        mp_color_taken_mark[pi] = lv_label_create(mp_color_swatch[pi]);
        lv_label_set_text(mp_color_taken_mark[pi], LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(mp_color_taken_mark[pi], lv_color_hex(0xEF4444), 0);
        lv_obj_set_style_text_font(mp_color_taken_mark[pi], &lv_font_montserrat_22, 0);
        lv_obj_center(mp_color_taken_mark[pi]);
        lv_obj_add_flag(mp_color_taken_mark[pi], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mp_color_taken_mark[pi], LV_OBJ_FLAG_CLICKABLE);
    }

    btn_mp_color_back = make_button(screen_multiplayer_color, i18n_get(I18N_BACK), 50, 50, event_multiplayer_color_back);
    lv_obj_align(btn_mp_color_back, LV_ALIGN_BOTTOM_MID, 0, -28);
    style_arcmind_side_button(btn_mp_color_back);
}

static void build_multiplayer_name_screen()
{
    screen_multiplayer_name = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_name, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_name, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_name, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_name, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_clip_corner(screen_multiplayer_name, true, 0);
    lv_obj_set_style_radius(screen_multiplayer_name, LV_RADIUS_CIRCLE, 0);

    label_multiplayer_name_title = lv_label_create(screen_multiplayer_name);
    lv_obj_set_style_text_color(label_multiplayer_name_title, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_multiplayer_name_title, UI_FONT_22, 0);
    lv_obj_align(label_multiplayer_name_title, LV_ALIGN_TOP_MID, 0, 28);

    textarea_multiplayer_name = lv_textarea_create(screen_multiplayer_name);
    lv_obj_set_size(textarea_multiplayer_name, 200, 36);
    lv_obj_align(textarea_multiplayer_name, LV_ALIGN_TOP_MID, 0, 62);
    lv_textarea_set_max_length(textarea_multiplayer_name, 15);
    lv_textarea_set_one_line(textarea_multiplayer_name, true);
    lv_obj_set_style_bg_color(textarea_multiplayer_name, lv_color_hex(0x1E1B3A), 0);
    lv_obj_set_style_border_color(textarea_multiplayer_name, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(textarea_multiplayer_name, 1, 0);
    lv_obj_set_style_text_color(textarea_multiplayer_name, lv_color_hex(0xE9E0FF), 0);
    lv_obj_set_style_text_font(textarea_multiplayer_name, UI_FONT_14, 0);

    btn_mp_name_save = make_button(screen_multiplayer_name, i18n_get(I18N_SAVE), 80, 34, event_multiplayer_name_save);
    lv_obj_align(btn_mp_name_save, LV_ALIGN_TOP_MID, 52, 108);
    style_arcmind_menu_button(btn_mp_name_save);

    btn_mp_name_back = make_button(screen_multiplayer_name, i18n_get(I18N_BACK), 80, 34, event_multiplayer_name_back);
    lv_obj_align(btn_mp_name_back, LV_ALIGN_TOP_MID, -52, 108);
    style_arcmind_menu_button(btn_mp_name_back);

    keyboard_multiplayer_name = lv_keyboard_create(screen_multiplayer_name);
    lv_obj_set_size(keyboard_multiplayer_name, 304, 136);
    lv_obj_align(keyboard_multiplayer_name, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_keyboard_set_textarea(keyboard_multiplayer_name, textarea_multiplayer_name);
    lv_keyboard_set_mode(keyboard_multiplayer_name, LV_KEYBOARD_MODE_TEXT_LOWER);
    style_arcmind_keyboard(keyboard_multiplayer_name);
}

static void build_multiplayer_cmd_select_screen()
{
    int i;

    screen_multiplayer_cmd_select = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_cmd_select, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_cmd_select, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_cmd_select, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_cmd_select, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_cmd_select_title = lv_label_create(screen_multiplayer_cmd_select);
    lv_obj_set_style_text_color(label_multiplayer_cmd_select_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_select_title, UI_FONT_22, 0);
    lv_obj_align(label_multiplayer_cmd_select_title, LV_ALIGN_TOP_MID, 0, 22);

    for (i = 0; i < (MULTIPLAYER_COUNT - 1); i++) {
        button_multiplayer_cmd_target[i] = lv_btn_create(screen_multiplayer_cmd_select);
        lv_obj_set_size(button_multiplayer_cmd_target[i], 220, 46);
        lv_obj_align(button_multiplayer_cmd_target[i], LV_ALIGN_CENTER, 0, -42 + (i * 58));
        lv_obj_add_event_cb(button_multiplayer_cmd_target[i], event_multiplayer_cmd_target_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        label_multiplayer_cmd_target[i] = lv_label_create(button_multiplayer_cmd_target[i]);
        lv_obj_set_style_text_font(label_multiplayer_cmd_target[i], UI_FONT_22, 0);
        lv_obj_set_style_text_color(label_multiplayer_cmd_target[i], lv_color_white(), 0);
        lv_obj_center(label_multiplayer_cmd_target[i]);
    }

    btn_mp_cmd_select_back = make_button(screen_multiplayer_cmd_select, i18n_get(I18N_BACK), 120, 46, event_multiplayer_cmd_select_back);
    lv_obj_align(btn_mp_cmd_select_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_multiplayer_cmd_damage_screen()
{
    screen_multiplayer_cmd_damage = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_cmd_damage, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_cmd_damage, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_cmd_damage, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_cmd_damage, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_cmd_damage_title = lv_label_create(screen_multiplayer_cmd_damage);
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_title, UI_FONT_22, 0);
    lv_obj_align(label_multiplayer_cmd_damage_title, LV_ALIGN_TOP_MID, 0, 26);

    label_multiplayer_cmd_damage_value = lv_label_create(screen_multiplayer_cmd_damage);
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_value, &lv_font_montserrat_36, 0);
    lv_obj_align(label_multiplayer_cmd_damage_value, LV_ALIGN_CENTER, 0, -8);

    label_multiplayer_cmd_damage_hint = lv_label_create(screen_multiplayer_cmd_damage);
    lv_label_set_text(label_multiplayer_cmd_damage_hint, i18n_get(I18N_CMD_DAMAGE_HINT));
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_hint, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_hint, UI_FONT_14, 0);
    lv_obj_align(label_multiplayer_cmd_damage_hint, LV_ALIGN_CENTER, 0, 38);

    btn_mp_cmd_damage_back = make_button(screen_multiplayer_cmd_damage, i18n_get(I18N_BACK), 120, 46, event_multiplayer_cmd_damage_back);
    lv_obj_align(btn_mp_cmd_damage_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_multiplayer_all_damage_screen()
{
    screen_multiplayer_all_damage = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_all_damage, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_all_damage, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_all_damage, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_all_damage, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_all_damage_title = lv_label_create(screen_multiplayer_all_damage);
    lv_obj_set_style_text_color(label_multiplayer_all_damage_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_title, UI_FONT_22, 0);
    lv_obj_align(label_multiplayer_all_damage_title, LV_ALIGN_TOP_MID, 0, 26);

    label_multiplayer_all_damage_value = lv_label_create(screen_multiplayer_all_damage);
    lv_obj_set_style_text_color(label_multiplayer_all_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_value, &lv_font_montserrat_36, 0);
    lv_obj_align(label_multiplayer_all_damage_value, LV_ALIGN_CENTER, 0, -8);

    label_multiplayer_all_damage_hint = lv_label_create(screen_multiplayer_all_damage);
    lv_label_set_text(label_multiplayer_all_damage_hint, i18n_get(I18N_ALL_DAMAGE_HINT));
    lv_obj_set_style_text_color(label_multiplayer_all_damage_hint, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_hint, UI_FONT_14, 0);
    lv_obj_align(label_multiplayer_all_damage_hint, LV_ALIGN_CENTER, 0, 38);

    btn_mp_all_damage_apply = make_button(screen_multiplayer_all_damage, i18n_get(I18N_APPLY), 120, 46, event_multiplayer_all_damage_apply);
    lv_obj_align(btn_mp_all_damage_apply, LV_ALIGN_BOTTOM_MID, 0, -78);

    btn_mp_all_damage_back = make_button(screen_multiplayer_all_damage, i18n_get(I18N_BACK), 120, 46, event_multiplayer_all_damage_back);
    lv_obj_align(btn_mp_all_damage_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_main_screen()
{
    lv_obj_t *btn = NULL;

    screen_main = lv_obj_create(NULL);
    lv_obj_set_size(screen_main, 360, 360);
    lv_obj_set_style_bg_color(screen_main, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_main, 0, 0);
    lv_obj_set_scrollbar_mode(screen_main, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_main, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_main, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_main, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    main_bg_panel = lv_obj_create(screen_main);
    lv_obj_set_size(main_bg_panel, 360, 360);
    lv_obj_center(main_bg_panel);
    lv_obj_set_style_bg_color(main_bg_panel, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_bg_opa(main_bg_panel, LV_OPA_10, 0);
    lv_obj_set_style_border_width(main_bg_panel, 0, 0);
    lv_obj_set_style_radius(main_bg_panel, 0, 0);
    lv_obj_clear_flag(main_bg_panel, LV_OBJ_FLAG_CLICKABLE);

    {
        int s, a;
        mp_slot_pos_t pos;

        for (s = 0; s < SP_RING_SLOTS; s++) {
            sp_ring_seg[s] = lv_arc_create(screen_main);
            lv_obj_set_size(sp_ring_seg[s], SP_RING_DIAM, SP_RING_DIAM);
            lv_obj_center(sp_ring_seg[s]);
            lv_arc_set_rotation(sp_ring_seg[s], MP_RING_ROTATION);
            lv_obj_remove_style(sp_ring_seg[s], NULL, LV_PART_KNOB);
            lv_obj_set_style_arc_width(sp_ring_seg[s], SP_RING_WIDTH, LV_PART_MAIN);
            lv_obj_set_style_arc_width(sp_ring_seg[s], 0, LV_PART_INDICATOR);
            lv_obj_set_style_arc_rounded(sp_ring_seg[s], false, LV_PART_MAIN);
            lv_obj_clear_flag(sp_ring_seg[s], LV_OBJ_FLAG_CLICKABLE);
        }

        for (s = 0; s < SP_RING_SLOTS; s++) {
            lv_obj_t *hit;
            sp_ring_hit_pos(s, &pos);
            hit = lv_btn_create(screen_main);
            lv_obj_set_size(hit, MP_RING_HIT_SIZE, MP_RING_HIT_SIZE);
            lv_obj_set_pos(hit, pos.x, pos.y);
            lv_obj_set_style_radius(hit, 3, 0);
            lv_obj_set_style_shadow_width(hit, 0, 0);
            lv_obj_set_style_pad_all(hit, 0, 0);
            lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_opa(hit, LV_OPA_TRANSP, 0);
            lv_obj_add_event_cb(hit, event_sp_ring_slice, LV_EVENT_CLICKED, (void *)(intptr_t)s);
            btn_sp_ring_hit[s] = hit;
        }

        canvas_poison_sp_buf = (uint8_t *)heap_caps_malloc(
            SP_POISON_BUF_SIZE, MALLOC_CAP_SPIRAM);
        canvas_poison_sp = lv_canvas_create(screen_main);
        lv_canvas_set_buffer(canvas_poison_sp, canvas_poison_sp_buf,
                             SP_POISON_W, SP_POISON_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_img_set_pivot(canvas_poison_sp, SP_POISON_W_HALF, SP_POISON_H_HALF);
        sp_get_counter_slot_pos(MP_SLOT_INFECT, &pos);
        lv_obj_set_pos(canvas_poison_sp, pos.x, pos.y);
        lv_obj_clear_flag(canvas_poison_sp, LV_OBJ_FLAG_CLICKABLE);

        for (a = 0; a < 3; a++) {
            canvas_cmd_sp_buf[a] = (uint8_t *)heap_caps_malloc(
                CMP_CMD_BUF_SIZE, MALLOC_CAP_SPIRAM);
            canvas_cmd_sp[a] = lv_canvas_create(screen_main);
            lv_canvas_set_buffer(canvas_cmd_sp[a], canvas_cmd_sp_buf[a],
                                 CMP_CMD_W, CMP_CMD_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
            lv_img_set_pivot(canvas_cmd_sp[a], CMP_CMD_W / 2, CMP_CMD_H / 2);
            sp_get_counter_slot_pos(a + 1, &pos);
            lv_obj_set_pos(canvas_cmd_sp[a], pos.x, pos.y);
            lv_obj_clear_flag(canvas_cmd_sp[a], LV_OBJ_FLAG_CLICKABLE);
        }
    }

    canvas_main_life_buf = (uint8_t *)heap_caps_malloc(
        MAIN_LIFE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    canvas_main_life = lv_canvas_create(screen_main);
    lv_canvas_set_buffer(canvas_main_life, canvas_main_life_buf,
                         MAIN_LIFE_W, MAIN_LIFE_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_img_set_pivot(canvas_main_life, MAIN_LIFE_W / 2, MAIN_LIFE_H / 2);
    lv_obj_clear_flag(canvas_main_life, LV_OBJ_FLAG_CLICKABLE);

    canvas_main_name_buf = (uint8_t *)heap_caps_malloc(
        MAIN_NAME_BUF_SIZE, MALLOC_CAP_SPIRAM);
    canvas_main_name = lv_canvas_create(screen_main);
    lv_canvas_set_buffer(canvas_main_name, canvas_main_name_buf,
                         MAIN_NAME_W, MAIN_NAME_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_img_set_pivot(canvas_main_name, MAIN_NAME_W / 2, MAIN_NAME_H / 2);
    lv_obj_clear_flag(canvas_main_name, LV_OBJ_FLAG_CLICKABLE);

    img_main_skull = lv_img_create(screen_main);
    lv_img_set_src(img_main_skull, &dead_img);
    lv_obj_set_size(img_main_skull, 40, 40);
    lv_img_set_pivot(img_main_skull, 20, 20);
    lv_obj_clear_flag(img_main_skull, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img_main_skull, LV_OBJ_FLAG_HIDDEN);

    main_battery_panel = lv_obj_create(screen_main);
    lv_obj_set_size(main_battery_panel, 68, 68);
    lv_obj_align(main_battery_panel, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_bg_opa(main_battery_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_battery_panel, 0, 0);
    lv_obj_set_style_pad_all(main_battery_panel, 0, 0);
    lv_obj_add_flag(main_battery_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(main_battery_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(main_battery_panel, event_main_battery_menu, LV_EVENT_CLICKED, NULL);

    arc_main_battery = make_center_battery_arc(main_battery_panel);
    lv_obj_center(arc_main_battery);

    label_main_battery = lv_label_create(main_battery_panel);
    lv_label_set_text(label_main_battery, "--%");
    lv_obj_set_style_text_color(label_main_battery, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_main_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(label_main_battery, LV_ALIGN_CENTER, 0, BATT_HUB_PCT_Y);
    lv_obj_clear_flag(label_main_battery, LV_OBJ_FLAG_CLICKABLE);

    label_main_battery_charge = lv_label_create(main_battery_panel);
    lv_label_set_text(label_main_battery_charge, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(label_main_battery_charge, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_main_battery_charge, lv_color_hex(0xFACC15), 0);
    lv_obj_align(label_main_battery_charge, LV_ALIGN_CENTER, 0, BATT_HUB_ICON_Y);
    lv_obj_add_flag(label_main_battery_charge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_main_battery_charge, LV_OBJ_FLAG_CLICKABLE);

    label_main_battery_settings = lv_label_create(main_battery_panel);
    lv_label_set_text(label_main_battery_settings, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(label_main_battery_settings, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_main_battery_settings, lv_color_hex(0x8B5CF6), 0);
    lv_obj_align(label_main_battery_settings, LV_ALIGN_CENTER, 0, BATT_HUB_ICON_Y);
    lv_obj_add_flag(label_main_battery_settings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_main_battery_settings, LV_OBJ_FLAG_CLICKABLE);

    main_center_panel = lv_obj_create(screen_main);
    lv_obj_set_size(main_center_panel, 160, 120);
    lv_obj_center(main_center_panel);
    lv_obj_set_style_bg_opa(main_center_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_center_panel, 0, 0);
    lv_obj_set_style_pad_all(main_center_panel, 0, 0);
    lv_obj_add_flag(main_center_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(main_center_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(main_center_panel, event_main_center_menu, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(main_center_panel, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(main_center_panel, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    life_hitbox = make_plain_box(screen_main, 360, 360);
    lv_obj_center(life_hitbox);
    lv_obj_set_style_bg_opa(life_hitbox, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(life_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(life_hitbox, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(life_hitbox, event_menu_swipe, LV_EVENT_RELEASED, NULL);
    lv_obj_move_background(life_hitbox);

    life_container = NULL;

    label_life_delta = lv_label_create(screen_main);
    lv_label_set_text(label_life_delta, "+0");
    lv_obj_set_style_text_color(label_life_delta, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_text_font(label_life_delta, UI_FONT_22, 0);
    lv_obj_align(label_life_delta, LV_ALIGN_CENTER, MAIN_DELTA_X, MAIN_CENTER_Y);
    lv_obj_add_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_life_delta, LV_OBJ_FLAG_CLICKABLE);

    menu_overlay = make_plain_box(lv_layer_top(), 360, 360);
    lv_obj_set_style_bg_color(menu_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(menu_overlay, LV_OPA_50, 0);
    lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(menu_overlay, event_hide_main_menu, LV_EVENT_CLICKED, NULL);

    menu_panel = lv_obj_create(menu_overlay);
    lv_obj_set_size(menu_panel, 230, 230);
    lv_obj_center(menu_panel);
    lv_obj_set_style_radius(menu_panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(menu_panel, lv_color_hex(0x13111F), 0);
    lv_obj_set_style_bg_opa(menu_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(menu_panel, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(menu_panel, 2, 0);
    lv_obj_set_style_pad_all(menu_panel, 0, 0);
    lv_obj_set_scrollbar_mode(menu_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(menu_panel, LV_OBJ_FLAG_CLICKABLE);

    /* Side buttons — ArcMind circle style */
    {
        lv_obj_t *lbl;

        btn_menu_reset = make_button(menu_overlay, i18n_get(I18N_RESET), 50, 50, event_menu_reset);
        lv_obj_set_style_radius(btn_menu_reset, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(btn_menu_reset, lv_color_hex(0x1A1630), 0);
        lv_obj_set_style_bg_opa(btn_menu_reset, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn_menu_reset, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(btn_menu_reset, 1, 0);
        lv_obj_set_style_shadow_width(btn_menu_reset, 0, 0);
        lv_obj_set_style_pad_all(btn_menu_reset, 0, 0);
        lv_obj_align(btn_menu_reset, LV_ALIGN_CENTER, -148, 0);
        lbl = lv_obj_get_child(btn_menu_reset, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xD8CCFF), 0);
            lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
            lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);
        }

        btn_menu_back = make_button(menu_overlay, i18n_get(I18N_BACK), 50, 50, event_menu_back);
        lv_obj_set_style_radius(btn_menu_back, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(btn_menu_back, lv_color_hex(0x1A1630), 0);
        lv_obj_set_style_bg_opa(btn_menu_back, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn_menu_back, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(btn_menu_back, 1, 0);
        lv_obj_set_style_shadow_width(btn_menu_back, 0, 0);
        lv_obj_set_style_pad_all(btn_menu_back, 0, 0);
        lv_obj_align(btn_menu_back, LV_ALIGN_CENTER, 148, 0);
        lbl = lv_obj_get_child(btn_menu_back, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xD8CCFF), 0);
            lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
            lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);
        }
    }

    /* Menu items — ArcMind dark card style */
    {
        menu_players_row = lv_obj_create(menu_panel);
        lv_obj_set_size(menu_players_row, 170, 36);
        lv_obj_set_style_bg_color(menu_players_row, lv_color_hex(0x1E1B3A), 0);
        lv_obj_set_style_bg_opa(menu_players_row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(menu_players_row, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(menu_players_row, 2, 0);
        lv_obj_set_style_radius(menu_players_row, 8, 0);
        lv_obj_set_style_shadow_width(menu_players_row, 0, 0);
        lv_obj_set_style_pad_left(menu_players_row, 12, 0);
        lv_obj_set_style_pad_right(menu_players_row, 12, 0);
        lv_obj_set_style_pad_top(menu_players_row, 0, 0);
        lv_obj_set_style_pad_bottom(menu_players_row, 0, 0);
        lv_obj_set_scrollbar_mode(menu_players_row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(menu_players_row, LV_ALIGN_TOP_MID, 0, 48);
        lv_obj_add_flag(menu_players_row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(menu_players_row, event_menu_focus_players, LV_EVENT_CLICKED, NULL);

        menu_players_name = lv_label_create(menu_players_row);
        lv_label_set_text(menu_players_name, i18n_get(I18N_PLAYERS));
        lv_obj_set_style_text_color(menu_players_name, lv_color_hex(0xE9E0FF), 0);
        lv_obj_set_style_text_font(menu_players_name, UI_FONT_14, 0);
        lv_obj_align(menu_players_name, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_clear_flag(menu_players_name, LV_OBJ_FLAG_CLICKABLE);

        menu_players_value = lv_label_create(menu_players_row);
        lv_label_set_text(menu_players_value, "4");
        lv_obj_set_style_text_color(menu_players_value, lv_color_hex(0x8B5CF6), 0);
        lv_obj_set_style_text_font(menu_players_value, UI_FONT_14, 0);
        lv_obj_align(menu_players_value, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_clear_flag(menu_players_value, LV_OBJ_FLAG_CLICKABLE);

        btn_menu_settings = make_button(menu_panel, i18n_get(I18N_SETTINGS), 170, 36, event_menu_settings);
        lv_obj_set_style_bg_color(btn_menu_settings, lv_color_hex(0x1E1B3A), 0);
        lv_obj_set_style_bg_opa(btn_menu_settings, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn_menu_settings, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(btn_menu_settings, 1, 0);
        lv_obj_set_style_radius(btn_menu_settings, 8, 0);
        lv_obj_set_style_shadow_width(btn_menu_settings, 0, 0);
        lv_obj_set_style_pad_all(btn_menu_settings, 0, 0);
        lv_obj_align(btn_menu_settings, LV_ALIGN_TOP_MID, 0, 90);
        style_menu_label(lv_obj_get_child(btn_menu_settings, 0));

        btn_menu_select_first = make_button(menu_panel, i18n_get(I18N_SELECT_FIRST_PLAYER), 180, 36, event_menu_select_first_player);
        lv_obj_set_style_bg_color(btn_menu_select_first, lv_color_hex(0x1E1B3A), 0);
        lv_obj_set_style_bg_opa(btn_menu_select_first, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn_menu_select_first, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(btn_menu_select_first, 1, 0);
        lv_obj_set_style_radius(btn_menu_select_first, 8, 0);
        lv_obj_set_style_shadow_width(btn_menu_select_first, 0, 0);
        lv_obj_set_style_pad_all(btn_menu_select_first, 0, 0);
        lv_obj_align(btn_menu_select_first, LV_ALIGN_TOP_MID, 0, 132);
        style_menu_label(lv_obj_get_child(btn_menu_select_first, 0));
    }

    /* Brightness bar + label at bottom of menu panel */
    menu_brightness_bar = lv_bar_create(menu_panel);
    lv_obj_set_size(menu_brightness_bar, 110, 8);
    lv_bar_set_range(menu_brightness_bar, 5, 100);
    lv_bar_set_value(menu_brightness_bar, brightness_percent, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(menu_brightness_bar, lv_color_hex(0x1E1B3A), LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_brightness_bar, lv_color_hex(0x4C1D95), LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_brightness_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(menu_brightness_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_brightness_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(menu_brightness_bar, lv_color_hex(0x8B5CF6), LV_PART_INDICATOR);
    lv_obj_set_style_radius(menu_brightness_bar, 2, LV_PART_INDICATOR);
    lv_obj_align(menu_brightness_bar, LV_ALIGN_TOP_MID, 0, 176);
    lv_obj_add_flag(menu_brightness_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu_brightness_bar, event_menu_focus_brightness, LV_EVENT_CLICKED, NULL);

    menu_brightness_label = lv_label_create(menu_panel);
    lv_label_set_text(menu_brightness_label, "30%");
    lv_obj_set_style_text_color(menu_brightness_label, lv_color_hex(0xE9E0FF), 0);
    lv_obj_set_style_text_font(menu_brightness_label, UI_FONT_14, 0);
    lv_obj_align(menu_brightness_label, LV_ALIGN_TOP_MID, 0, 187);
    lv_obj_add_flag(menu_brightness_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu_brightness_label, event_menu_focus_brightness, LV_EVENT_CLICKED, NULL);
    refresh_menu_brightness_ui();
    refresh_menu_focus_ui();
    refresh_main_ui();
}

static void build_settings_screen()
{
    lv_obj_t *btn_back;
    lv_obj_t *lbl;
    int i;

    screen_settings = lv_obj_create(NULL);
    lv_obj_set_size(screen_settings, 360, 360);
    lv_obj_set_style_bg_color(screen_settings, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_settings, 0, 0);
    lv_obj_set_scrollbar_mode(screen_settings, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_settings, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_settings, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_settings, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    /* Title */
    label_settings_title = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_title, i18n_get(I18N_SETTINGS));
    lv_obj_set_style_text_color(label_settings_title, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_settings_title, UI_FONT_14, 0);
    lv_obj_align(label_settings_title, LV_ALIGN_TOP_MID, 0, 16);

    /* Setting rows */
    for (i = 0; i < SETTINGS_FIELD_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(screen_settings);
        lv_obj_set_size(row, 250, 44);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1630), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x2D2A45), 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_left(row, 14, 0);
        lv_obj_set_style_pad_right(row, 14, 0);
        lv_obj_set_style_pad_top(row, 0, 0);
        lv_obj_set_style_pad_bottom(row, 0, 0);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(row, LV_ALIGN_CENTER, 0, settings_row_offset_y(i));
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, event_settings_select_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        if (!settings_field_visible(i))
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        settings_row[i] = row;

        lv_obj_t *name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, i18n_get(settings_field_ids[i]));
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(0xB0A3D4), 0);
        lv_obj_set_style_text_font(name_lbl, UI_FONT_14, 0);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 0, 0);
        settings_row_name[i] = name_lbl;

        lv_obj_t *val_lbl = lv_label_create(row);
        lv_label_set_text(val_lbl, "---");
        lv_obj_set_style_text_color(val_lbl, lv_color_hex(0x8B5CF6), 0);
        lv_obj_set_style_text_font(val_lbl, UI_FONT_14, 0);
        lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
        settings_row_value[i] = val_lbl;
    }

    /* Back — inset from round bezel; narrow enough to stay inside the circle */
    btn_settings_back = make_button(screen_settings, i18n_get(I18N_BACK), 120, 44, event_settings_back);
    lv_obj_set_style_bg_color(btn_settings_back, lv_color_hex(0x1E1B3A), 0);
    lv_obj_set_style_bg_opa(btn_settings_back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_settings_back, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(btn_settings_back, 1, 0);
    lv_obj_set_style_radius(btn_settings_back, 8, 0);
    lv_obj_set_style_shadow_width(btn_settings_back, 0, 0);
    lv_obj_set_style_pad_all(btn_settings_back, 0, 0);
    lv_obj_align(btn_settings_back, LV_ALIGN_BOTTOM_MID, 0, -26);
    lv_obj_set_ext_click_area(btn_settings_back, 12);
    lbl = lv_obj_get_child(btn_settings_back, 0);
    if (lbl) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xC4B5FD), 0);
        lv_obj_set_style_text_font(lbl, UI_FONT_14, 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_move_foreground(btn_settings_back);

    refresh_settings_ui();
}

void knob_gui(void)
{
    nvs_flash_init();
    brightness_init();
    settings_load();
    brightness_apply();

    if (!device_license_is_provisioned()) {
        build_unregistered_screen();
        lv_scr_load(screen_unregistered);
        return;
    }

    build_intro_screen();
    build_main_screen();
    build_multiplayer_screen();
    build_multiplayer_menu_screen();
    build_multiplayer_name_screen();
    build_multiplayer_color_screen();
    build_multiplayer_cmd_select_screen();
    build_multiplayer_cmd_damage_screen();
    build_multiplayer_all_damage_screen();
    build_settings_screen();

    refresh_main_ui();
    refresh_multiplayer_ui();
    refresh_texts_ui();

    intro_timer = lv_timer_create(intro_timer_cb, 2500, NULL);
    life_delta_hide_timer = lv_timer_create(life_delta_hide_cb, 2000, NULL);
    if (life_delta_hide_timer != NULL) {
        lv_timer_pause(life_delta_hide_timer);
    }
    mp_delta_hide_timer = lv_timer_create(mp_delta_hide_cb, 2000, NULL);
    if (mp_delta_hide_timer != NULL) {
        lv_timer_pause(mp_delta_hide_timer);
    }
    mp_timer_tick = lv_timer_create(mp_timer_tick_cb, 100, NULL);
    if (mp_timer_tick != NULL) lv_timer_pause(mp_timer_tick);
    mp_timer_blink = lv_timer_create(mp_timer_blink_cb, 200, NULL);
    if (mp_timer_blink != NULL) lv_timer_pause(mp_timer_blink);
    mp_timer_spin_tmr = lv_timer_create(mp_timer_spin_cb, 60, NULL);
    if (mp_timer_spin_tmr != NULL) lv_timer_pause(mp_timer_spin_tmr);

    goto_state(mp_timer_duration_idx > 0 ? MTIMER_IDLE : MTIMER_OFF);

    last_activity_tick = lv_tick_get();
    auto_dim_timer = lv_timer_create(auto_dim_timer_cb, 1000, NULL);

    update_battery_measurement(true);
    refresh_battery_ui();

    lv_scr_load(screen_intro);
}

// ----------------------------------------------------
// knob event handler
// one click = exactly 1 life / damage / brightness
// ----------------------------------------------------
static void handle_knob_event(knob_event_t k)
{
    activity_kick();
    if (in_undim_grace()) return;

    /* Menu overlay is on lv_layer_top() — encoder adjusts focused field */
    if ((menu_overlay != NULL) && !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) {
        int dir = (k == KNOB_RIGHT) ? 1 : (k == KNOB_LEFT ? -1 : 0);
        if (dir == 0) return;
        if (menu_focus == 0) change_game_player_count(dir);
        else                 change_brightness(dir);
        return;
    }

    if (lv_scr_act() == screen_intro)
    {
        return;
    }
    else if (lv_scr_act() == screen_main)
    {
        if (cmd_main_selected >= 0) {
            if (k == KNOB_LEFT)       change_cmd_main_damage(-1);
            else if (k == KNOB_RIGHT) change_cmd_main_damage(+1);
        } else if (main_poison_selected >= 0) {
            if (k == KNOB_LEFT)       change_main_poison(-1);
            else if (k == KNOB_RIGHT) change_main_poison(+1);
        } else {
            if (k == KNOB_LEFT)       change_life(-1);
            else if (k == KNOB_RIGHT) change_life(+1);
        }
    }
    else if (lv_scr_act() == screen_settings)
    {
        if (k == KNOB_LEFT)      change_settings_field(-1);
        else if (k == KNOB_RIGHT) change_settings_field(+1);
    }
    else if (lv_scr_act() == screen_multiplayer)
    {
        if (mp_timer_state == MTIMER_SELECTING) {
            if (k == KNOB_RIGHT)
                mp_timer_select_player = mp_timer_next_living_player(mp_timer_select_player);
            else
                mp_timer_select_player = mp_timer_prev_living_player(mp_timer_select_player);
            mp_timer_select_moved = true;
            mp_timer_update_highlight();
            mp_timer_refresh_btn_label();
        } else if (cmd_mp_selected_victim >= 0) {
            if (k == KNOB_LEFT)       change_cmd_mp_damage(-1);
            else if (k == KNOB_RIGHT) change_cmd_mp_damage(+1);
        } else if (mp_poison_selected >= 0) {
            if (k == KNOB_LEFT)       change_multiplayer_poison(-1);
            else if (k == KNOB_RIGHT) change_multiplayer_poison(+1);
        } else {
            if (k == KNOB_LEFT)       change_multiplayer_life(-1);
            else if (k == KNOB_RIGHT) change_multiplayer_life(+1);
        }
    }
    else if (lv_scr_act() == screen_multiplayer_cmd_damage)
    {
        if (k == KNOB_LEFT)      change_multiplayer_cmd_damage(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_cmd_damage(+1);
    }
    else if (lv_scr_act() == screen_multiplayer_all_damage)
    {
        if (k == KNOB_LEFT)      change_multiplayer_all_damage(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_all_damage(+1);
    }
}

void knob_change(knob_event_t k, int cont)
{
    uint8_t next_head;

    if (!knob_initialized)
    {
        last_knob_cont = cont;
        knob_initialized = true;
    }

    last_knob_cont = cont;

    next_head = (uint8_t)((knob_event_head + 1U) % KNOB_EVENT_QUEUE_SIZE);
    if (next_head == knob_event_tail) {
        knob_event_tail = (uint8_t)((knob_event_tail + 1U) % KNOB_EVENT_QUEUE_SIZE);
    }

    knob_event_queue[knob_event_head].event = k;
    knob_event_queue[knob_event_head].cont = cont;
    knob_event_head = next_head;
}

void knob_process_pending(void)
{
    uint8_t processed = 0;

    while (knob_event_tail != knob_event_head && processed < 8U) {
        knob_event_t event = knob_event_queue[knob_event_tail].event;
        knob_event_tail = (uint8_t)((knob_event_tail + 1U) % KNOB_EVENT_QUEUE_SIZE);
        handle_knob_event(event);
        processed++;
    }
}
