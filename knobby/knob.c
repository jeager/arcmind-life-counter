#include "knob.h"
#include "arcmind_logo.h"
#include "driver/ledc.h"
#include "esp_random.h"
#include "esp32-hal-cpu.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ENEMY_COUNT 3
#define LIFE_MIN -999
#define LIFE_MAX 999
#define DEFAULT_LIFE_TOTAL 40
#define DEFAULT_BRIGHTNESS_PERCENT 30
#define BATTERY_FULL_VOLTAGE 4.18f
#define BATTERY_EMPTY_VOLTAGE 3.35f
#define KNOBBY_LETTER_COUNT 7
#define MULTIPLAYER_COUNT 4
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
    const char *name;
    int damage;
} enemy_state_t;

typedef struct
{
    knob_event_t event;
    int cont;
} knob_input_event_t;

static int life_total = DEFAULT_LIFE_TOTAL;
static int brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;

static enemy_state_t enemies[ENEMY_COUNT] = {
    {"P1", 0},
    {"P2", 0},
    {"P3", 0}
};

static int selected_enemy = -1;

// ---------- screens ----------
static lv_obj_t *screen_intro = NULL;
static lv_obj_t *screen_main = NULL;
static lv_obj_t *screen_dice = NULL;
static lv_obj_t *screen_select = NULL;
static lv_obj_t *screen_damage = NULL;
static lv_obj_t *screen_settings = NULL;
static lv_obj_t *screen_multiplayer = NULL;
static lv_obj_t *screen_multiplayer_menu = NULL;
static lv_obj_t *screen_multiplayer_name = NULL;
static lv_obj_t *screen_multiplayer_cmd_select = NULL;
static lv_obj_t *screen_multiplayer_cmd_damage = NULL;
static lv_obj_t *screen_multiplayer_all_damage = NULL;
static lv_obj_t *screen_before_menu = NULL;

// ---------- main UI ----------
static lv_obj_t *intro_img = NULL;
static lv_obj_t *intro_label_name = NULL;
static lv_obj_t *arc_life = NULL;
static lv_obj_t *title_icon = NULL;
static lv_obj_t *title_sun_core = NULL;
static lv_obj_t *title_sun_rays[12];
static lv_obj_t *label_knobby_arc[KNOBBY_LETTER_COUNT];
static lv_obj_t *label_dice_result = NULL;
static lv_obj_t *label_dice_hint = NULL;
static lv_obj_t *life_container = NULL;
static lv_obj_t *life_hitbox = NULL;
static lv_obj_t *menu_open_button = NULL;
static lv_obj_t *menu_dot = NULL;
static lv_obj_t *turn_container = NULL;
static lv_obj_t *label_turn = NULL;
static lv_obj_t *label_turn_time = NULL;
static lv_obj_t *turn_live_dot = NULL;
static lv_obj_t *menu_overlay = NULL;
static lv_obj_t *menu_panel = NULL;
static lv_obj_t *menu_button_dice = NULL;
static lv_obj_t *menu_button_timer = NULL;
static lv_obj_t *menu_button_multiplayer = NULL;
static lv_obj_t *label_main_battery = NULL;
static lv_obj_t *bar_main_battery = NULL;
static lv_obj_t *label_multiplayer_battery = NULL;
static lv_obj_t *bar_multiplayer_battery = NULL;
static lv_obj_t *menu_brightness_bar = NULL;
static lv_obj_t *menu_brightness_label = NULL;

// Commander damage — main screen (you vs P2, P3, P4)
static lv_obj_t *cmd_main_circle[3] = {NULL, NULL, NULL};
static lv_obj_t *cmd_main_label[3] = {NULL, NULL, NULL};
static int cmd_main_damage[3] = {0, 0, 0};
static int cmd_main_selected = -1;

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

// ---------- select UI ----------
static lv_obj_t *label_select_title = NULL;
static lv_obj_t *label_enemy_name[ENEMY_COUNT];
static lv_obj_t *label_enemy_damage[ENEMY_COUNT];

// ---------- damage UI ----------
static lv_obj_t *label_damage_title = NULL;
static lv_obj_t *label_damage_value = NULL;
static lv_obj_t *label_damage_hint = NULL;

// ---------- settings UI ----------
static lv_obj_t *label_settings_title = NULL;
static lv_obj_t *label_settings_value = NULL;
static lv_obj_t *label_settings_hint = NULL;
static lv_obj_t *label_settings_battery = NULL;
static lv_obj_t *label_settings_battery_detail = NULL;
static lv_obj_t *arc_brightness = NULL;

// ---------- multiplayer UI ----------
static lv_obj_t *multiplayer_quadrants[MULTIPLAYER_COUNT];
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

static lv_timer_t *turn_timer = NULL;
static lv_timer_t *turn_blink_timer = NULL;
static lv_timer_t *intro_timer = NULL;
static lv_timer_t *life_preview_timer = NULL;
static lv_timer_t *multiplayer_life_preview_timer = NULL;
static int last_knob_cont = 0;
static bool knob_initialized = false;
static knob_input_event_t knob_event_queue[KNOB_EVENT_QUEUE_SIZE];
static volatile uint8_t knob_event_head = 0;
static volatile uint8_t knob_event_tail = 0;

static bool turn_timer_enabled = false;
static bool turn_indicator_visible = true;
static bool turn_ui_visible = false;
static uint32_t turn_elapsed_ms = 0;
static uint32_t turn_started_ms = 0;
static int turn_number = 0;
static int dice_result = 0;
static int battery_percent = -1;
static float battery_voltage = 0.0f;
static uint32_t battery_sample_tick = 0;
static bool battery_sample_valid = false;
static uint8_t turn_blink_steps_remaining = 0;
static int pending_life_delta = 0;
static bool life_preview_active = false;
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
static int multiplayer_pending_life_delta = 0;
static int multiplayer_preview_player = -1;
static bool multiplayer_life_preview_active = false;
static lv_point_t multiplayer_swipe_start = {0, 0};
static char multiplayer_names[MULTIPLAYER_COUNT][16] = {"P1", "P2", "P3", "P4"};

// ---------- life delta display ----------
static lv_obj_t *label_life_delta = NULL;
static lv_obj_t *label_multiplayer_delta[MULTIPLAYER_COUNT];
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
static lv_obj_t *switch_auto_dim = NULL;

// ---------- settings fields ----------
static bool mirror_enabled = false;
static int settings_selected_field = 0; // 0=brightness, 1=auto_dim, 2=mirror
static lv_obj_t *settings_row[3] = {NULL, NULL, NULL};
static lv_obj_t *settings_row_value[3] = {NULL, NULL, NULL};

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
static lv_obj_t *canvas_mp_life[2] = {NULL, NULL};
static lv_obj_t *canvas_mp_name[2] = {NULL, NULL};
static uint8_t  *canvas_mp_life_buf[2] = {NULL, NULL};
static uint8_t  *canvas_mp_name_buf[2] = {NULL, NULL};

/* Single central delta canvas — shown at screen center, rotated to face the active player */
#define CMP_DELTA_W 100
#define CMP_DELTA_H 46
#define CMP_DELTA_BUF_SIZE (CMP_DELTA_W * CMP_DELTA_H * 3)
static lv_obj_t *canvas_mp_delta_ctr = NULL;
static uint8_t  *canvas_mp_delta_ctr_buf = NULL;
static int       mp_delta_player = -1;

#define CMP_CMD_W 34
#define CMP_CMD_H 34
#define CMP_CMD_BUF_SIZE (CMP_CMD_W * CMP_CMD_H * 3)
/* attacker index mapping: mp_attackers[victim][slot] — mirrors the build-time array */
static const int cmp_mp_attackers[MULTIPLAYER_COUNT][3] = {
    {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}
};
static lv_obj_t *canvas_cmd_mp[2][3];
static uint8_t  *canvas_cmd_mp_buf[2][3];

static const float battery_curve_voltages[] = {3.35f, 3.55f, 3.68f, 3.74f, 3.80f, 3.88f, 3.96f, 4.06f, 4.18f};
static const int battery_curve_percentages[] = {0, 5, 12, 22, 34, 48, 64, 82, 100};

static void back_to_main(void);

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

static int get_arc_display_value(int value)
{
    if (value < 0) return 0;
    if (value > 40) return 40;
    return value;
}

static int clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
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

static void update_battery_measurement(bool force)
{
    if (!force && battery_sample_valid && (lv_tick_elaps(battery_sample_tick) < 5000)) {
        return;
    }

    battery_voltage = knob_read_battery_voltage();
    battery_sample_tick = lv_tick_get();
    battery_sample_valid = (battery_voltage > 0.0f);
}

static int read_battery_percent(void)
{
    update_battery_measurement(false);
    if (!battery_sample_valid) return -1;
    return battery_percent_from_voltage(battery_voltage);
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
            if (battery_percent < 20)      bar_color = lv_color_hex(0xEF4444);
            else if (battery_percent < 40) bar_color = lv_color_hex(0x22D3EE);
            else                           bar_color = lv_color_hex(0x8B5CF6);
        }
        if (label_main_battery != NULL)
            lv_label_set_text(label_main_battery, buf);
        if (label_multiplayer_battery != NULL)
            lv_label_set_text(label_multiplayer_battery, buf);
        if (bar_main_battery != NULL) {
            lv_bar_set_value(bar_main_battery, bar_val, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar_main_battery, bar_color, LV_PART_INDICATOR);
        }
        if (bar_multiplayer_battery != NULL) {
            lv_bar_set_value(bar_multiplayer_battery, bar_val, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar_multiplayer_battery, bar_color, LV_PART_INDICATOR);
        }
    }

    if (label_settings_battery == NULL) return;

    if (battery_percent < 0) {
        lv_label_set_text(label_settings_battery, "battery  --%");
        if (label_settings_battery_detail != NULL) {
            lv_label_set_text(label_settings_battery_detail, "no calibrated reading");
        }
        return;
    }

    snprintf(buf, sizeof(buf), "battery  %d%%", battery_percent);
    lv_label_set_text(label_settings_battery, buf);
    if (label_settings_battery_detail != NULL) {
        snprintf(detail_buf, sizeof(detail_buf), "%.2fV calibrated", battery_voltage);
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
    if (nvs_open("knobby", NVS_READONLY, &handle) == ESP_OK) {
        int8_t dim_val = 0;
        int8_t bri_val = DEFAULT_BRIGHTNESS_PERCENT;
        int8_t mirror_val = 0;
        nvs_get_i8(handle, "auto_dim", &dim_val);
        nvs_get_i8(handle, "brightness", &bri_val);
        nvs_get_i8(handle, "mirror", &mirror_val);
        auto_dim_enabled = (dim_val != 0);
        brightness_percent = clamp_brightness(bri_val);
        mirror_enabled = (mirror_val != 0);
        nvs_close(handle);
    }
}

static void settings_save(void)
{
    if (!settings_dirty) return;
    nvs_handle_t handle;
    if (nvs_open("knobby", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i8(handle, "auto_dim", auto_dim_enabled ? 1 : 0);
        nvs_set_i8(handle, "brightness", (int8_t)brightness_percent);
        nvs_set_i8(handle, "mirror", mirror_enabled ? 1 : 0);
        nvs_commit(handle);
        nvs_close(handle);
        settings_dirty = false;
    }
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
    lv_obj_center(label);

    return btn;
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

static lv_color_t get_player_base_color(int index)
{
    static const uint32_t colors[MULTIPLAYER_COUNT] = {0x7B1FE0, 0x29B6F6, 0xFFD600, 0xA5D6A7};
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x303030);
    return lv_color_hex(colors[index]);
}

static lv_color_t get_player_active_color(int index)
{
    static const uint32_t colors[MULTIPLAYER_COUNT] = {0x9C4DFF, 0x4FC3F7, 0xFFEA61, 0xC8E6C9};
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x505050);
    return lv_color_hex(colors[index]);
}

static lv_color_t get_player_text_color(int index)
{
    return (index == 2) ? lv_color_black() : lv_color_white();
}

static lv_color_t get_player_preview_color(int index, int delta)
{
    if (index == 2) {
        return (delta < 0) ? lv_color_hex(0x7A1020) : lv_color_hex(0x215A2A);
    }
    return (delta < 0) ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN);
}

static int get_main_player_index(void)
{
    int i;

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (strcmp(multiplayer_names[i], "m") == 0) {
            return i;
        }
    }

    return -1;
}

static int get_cmd_target_player_index(int row)
{
    int main_player = get_main_player_index();
    int count = 0;
    int i;

    if (row < 0 || row >= ENEMY_COUNT) return row;

    if (main_player < 0) {
        return row;
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (i == main_player) continue;
        if (count == row) return i;
        count++;
    }

    return row;
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

static void refresh_ring()
{
    lv_color_t c = get_life_color(life_total);

    lv_arc_set_value(arc_life, get_arc_display_value(life_total));

    lv_obj_set_style_arc_color(arc_life, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_life, 20, LV_PART_MAIN);

    lv_obj_set_style_arc_color(arc_life, c, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_life, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_life, true, LV_PART_INDICATOR);
}

static void refresh_brightness_ring()
{
    (void)arc_brightness; /* arc removed from settings screen */
}

static void refresh_dice_ui(void)
{
    char buf[8];

    if (label_dice_result == NULL) return;

    if (dice_result <= 0) {
        lv_label_set_text(label_dice_result, "--");
    } else {
        snprintf(buf, sizeof(buf), "%d", dice_result);
        lv_label_set_text(label_dice_result, buf);
    }
}

static uint32_t get_turn_elapsed_ms(void)
{
    uint32_t elapsed = turn_elapsed_ms;

    if (turn_timer_enabled) {
        elapsed += lv_tick_elaps(turn_started_ms);
    }

    return elapsed;
}

static void refresh_turn_ui()
{
    char turn_buf[24];
    char time_buf[24];
    uint32_t total_seconds = get_turn_elapsed_ms() / 1000;
    uint32_t hours = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;

    if (turn_number <= 0) {
        lv_label_set_text(label_turn, "Turn");
    } else {
        snprintf(turn_buf, sizeof(turn_buf), "turn %d", turn_number);
        lv_label_set_text(label_turn, turn_buf);
    }

    snprintf(time_buf, sizeof(time_buf), "%lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes);
    lv_label_set_text(label_turn_time, time_buf);

    if (turn_container != NULL) {
        if (turn_ui_visible) {
            lv_obj_clear_flag(turn_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(turn_container, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (turn_live_dot != NULL) {
        if (turn_timer_enabled) {
            lv_obj_clear_flag(turn_live_dot, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(turn_live_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (turn_container != NULL) {
        lv_obj_set_style_opa(turn_container, turn_indicator_visible ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }

    if (menu_button_timer != NULL) {
        lv_obj_set_style_bg_color(
            menu_button_timer,
            turn_timer_enabled ? lv_color_hex(0x166534) : lv_color_hex(0x1E1B3A),
            0
        );
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
    refresh_ring();
    refresh_life_digits();
    refresh_turn_ui();
}

static void refresh_select_ui()
{
    char buf[32];
    int i;

    for (i = 0; i < ENEMY_COUNT; i++) {
        int player_index = get_cmd_target_player_index(i);
        lv_obj_t *row = (label_enemy_name[i] != NULL) ? lv_obj_get_parent(label_enemy_name[i]) : NULL;
        lv_color_t text_color = get_player_text_color(player_index);

        lv_label_set_text(label_enemy_name[i], multiplayer_names[player_index]);
        snprintf(buf, sizeof(buf), "%d", enemies[i].damage);
        lv_label_set_text(label_enemy_damage[i], buf);
        lv_obj_set_style_text_color(label_enemy_name[i], text_color, 0);
        lv_obj_set_style_text_color(label_enemy_damage[i], text_color, 0);
        if (row != NULL) {
            lv_obj_set_style_bg_color(row, get_player_base_color(player_index), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        }
    }
}

static void refresh_damage_ui()
{
    char buf[64];
    lv_color_t text_color;

    if (selected_enemy < 0 || selected_enemy >= ENEMY_COUNT) return;

    {
        int player_index = get_cmd_target_player_index(selected_enemy);
        text_color = get_player_text_color(player_index);
        lv_label_set_text(label_damage_title, multiplayer_names[player_index]);
        lv_obj_set_style_bg_color(screen_damage, get_player_active_color(player_index), 0);
    }
    lv_obj_set_style_text_color(label_damage_title, text_color, 0);
    lv_obj_set_style_text_color(label_damage_value, text_color, 0);
    lv_obj_set_style_text_color(label_damage_hint, text_color, 0);

    snprintf(buf, sizeof(buf), "Damage: %d", enemies[selected_enemy].damage);
    lv_label_set_text(label_damage_value, buf);
}

static void refresh_settings_ui()
{
    char buf[16];
    int i;

    for (i = 0; i < 3; i++) {
        if (settings_row[i] == NULL) continue;
        lv_color_t border_c = (i == settings_selected_field)
            ? lv_color_hex(0x8B5CF6)
            : lv_color_hex(0x2D2A45);
        lv_obj_set_style_border_color(settings_row[i], border_c, 0);
    }

    snprintf(buf, sizeof(buf), "%d%%", brightness_percent);
    if (settings_row_value[0] != NULL) lv_label_set_text(settings_row_value[0], buf);
    if (settings_row_value[1] != NULL) lv_label_set_text(settings_row_value[1], auto_dim_enabled ? "on" : "off");
    if (settings_row_value[2] != NULL) lv_label_set_text(settings_row_value[2], mirror_enabled ? "on" : "off");

    refresh_battery_ui();
}

static void refresh_multiplayer_ui()
{
    /* Life: centered at each quadrant center */
    static const int16_t life_offsets_x[MULTIPLAYER_COUNT] = {-90, 90, 90, -90};
    static const int16_t life_offsets_y[MULTIPLAYER_COUNT] = {-90, -90, 90, 90};
    /* Name x same as life x; name y is above life from each player's perspective.
     * P0/P1: mirror=OFF → above on screen (y < life_y); mirror=ON → below on screen
     *        (y > life_y) so it appears above from upside-down view.
     * P2/P3: always above life on screen (y < life_y). */
    static const int16_t name_offsets_x[MULTIPLAYER_COUNT] = {-90, 90, 90, -90};
    int16_t name_offsets_y[MULTIPLAYER_COUNT];
    name_offsets_y[0] = (int16_t)(mirror_enabled ? -63 : -118);
    name_offsets_y[1] = (int16_t)(mirror_enabled ? -63 : -118);
    name_offsets_y[2] = 63;
    name_offsets_y[3] = 63;
    char buf[8];
    int i;

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        lv_color_t player_color = get_player_active_color(i);

        if (multiplayer_quadrants[i] != NULL) {
            lv_obj_set_style_bg_color(multiplayer_quadrants[i], player_color, 0);
            lv_obj_set_style_bg_opa(multiplayer_quadrants[i],
                (i == multiplayer_selected) ? LV_OPA_20 : LV_OPA_10, 0);
        }

        snprintf(buf, sizeof(buf), "%d", multiplayer_life[i]);

        if (i < 2) {
            /* P0, P1: canvas + lv_img_set_angle — avoids LVGL-heap layer alloc */
            int16_t angle = mirror_enabled ? 1800 : 0;
            lv_draw_label_dsc_t dsc;

            if (canvas_mp_life[i] != NULL && canvas_mp_life_buf[i] != NULL) {
                lv_draw_label_dsc_init(&dsc);
                dsc.color = player_color;
                dsc.font  = &lv_font_montserrat_36;
                dsc.align = LV_TEXT_ALIGN_CENTER;
                memset(canvas_mp_life_buf[i], 0, CMP_LIFE_BUF_SIZE);
                lv_canvas_draw_text(canvas_mp_life[i], 0,
                    (CMP_LIFE_H - 36) / 2, CMP_LIFE_W, &dsc, buf);
                lv_obj_align(canvas_mp_life[i], LV_ALIGN_CENTER,
                    life_offsets_x[i], life_offsets_y[i]);
                lv_img_set_angle(canvas_mp_life[i], angle);
            }

            if (canvas_mp_name[i] != NULL && canvas_mp_name_buf[i] != NULL) {
                lv_draw_label_dsc_init(&dsc);
                dsc.color = player_color;
                dsc.font  = &lv_font_montserrat_14;
                dsc.align = LV_TEXT_ALIGN_CENTER;
                memset(canvas_mp_name_buf[i], 0, CMP_NAME_BUF_SIZE);
                lv_canvas_draw_text(canvas_mp_name[i], 0,
                    (CMP_NAME_H - 14) / 2, CMP_NAME_W, &dsc, multiplayer_names[i]);
                lv_obj_align(canvas_mp_name[i], LV_ALIGN_CENTER,
                    name_offsets_x[i], name_offsets_y[i]);
                lv_img_set_angle(canvas_mp_name[i], angle);
            }
        } else {
            /* P2, P3: plain labels, no rotation */
            if (label_multiplayer_life[i] != NULL) {
                lv_label_set_text(label_multiplayer_life[i], buf);
                lv_obj_set_style_text_color(label_multiplayer_life[i], player_color, 0);
                lv_obj_align(label_multiplayer_life[i], LV_ALIGN_CENTER,
                    life_offsets_x[i], life_offsets_y[i]);
            }

            if (label_multiplayer_name[i] != NULL) {
                lv_label_set_text(label_multiplayer_name[i], multiplayer_names[i]);
                lv_obj_set_style_text_color(label_multiplayer_name[i], player_color, 0);
                lv_obj_align(label_multiplayer_name[i], LV_ALIGN_CENTER,
                    name_offsets_x[i], name_offsets_y[i]);
            }
        }
    }

    /* Commander circle labels for P0, P1: canvas + lv_img_set_angle */
    {
        int v, a;
        int16_t angle = mirror_enabled ? 1800 : 0;
        for (v = 0; v < 2; v++) {
            for (a = 0; a < 3; a++) {
                if (canvas_cmd_mp[v][a] == NULL || canvas_cmd_mp_buf[v][a] == NULL) continue;
                char nbuf[8];
                lv_draw_label_dsc_t dsc;
                snprintf(nbuf, sizeof(nbuf), "%d",
                    multiplayer_cmd_damage_totals[v][cmp_mp_attackers[v][a]]);
                lv_draw_label_dsc_init(&dsc);
                dsc.color = lv_color_white();
                dsc.font  = &lv_font_montserrat_14;
                dsc.align = LV_TEXT_ALIGN_CENTER;
                memset(canvas_cmd_mp_buf[v][a], 0, CMP_CMD_BUF_SIZE);
                lv_canvas_draw_text(canvas_cmd_mp[v][a], 0,
                    (CMP_CMD_H - 14) / 2, CMP_CMD_W, &dsc, nbuf);
                lv_img_set_angle(canvas_cmd_mp[v][a], angle);
            }
        }
    }
}

static void refresh_multiplayer_menu_ui()
{
    char buf[32];

    if (label_multiplayer_menu_title == NULL) return;

    snprintf(buf, sizeof(buf), "%s menu", multiplayer_names[multiplayer_menu_player]);
    lv_label_set_text(label_multiplayer_menu_title, buf);
}

static void refresh_multiplayer_name_ui()
{
    char buf[40];

    if (label_multiplayer_name_title != NULL) {
        snprintf(buf, sizeof(buf), "Rename %s", multiplayer_names[multiplayer_menu_player]);
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
        snprintf(buf, sizeof(buf), "%s -> target", multiplayer_names[multiplayer_menu_player]);
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
        snprintf(value_buf, sizeof(value_buf), "Damage: %d", multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target]);
        lv_label_set_text(label_multiplayer_cmd_damage_value, value_buf);
    }
}

static void refresh_multiplayer_all_damage_ui()
{
    char buf[32];

    if (label_multiplayer_all_damage_title != NULL) {
        lv_label_set_text(label_multiplayer_all_damage_title, "All players");
    }

    if (label_multiplayer_all_damage_value != NULL) {
        snprintf(buf, sizeof(buf), "Damage: %d", multiplayer_all_damage_value);
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
    /* Rotate 180° when the active player is one of the mirrored top players */
    angle = (mirror_enabled && player <= 1) ? 1800 : 0;

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

    if (mp_delta_hide_timer != NULL) {
        lv_timer_reset(mp_delta_hide_timer);
        lv_timer_resume(mp_delta_hide_timer);
    }
}

static void life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;

    life_total = clamp_life(life_total + pending_life_delta);
    pending_life_delta = 0;
    life_preview_active = false;
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    if (life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    }
    refresh_main_ui();
    refresh_select_ui();
}

static void multiplayer_life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!multiplayer_life_preview_active ||
        multiplayer_preview_player < 0 ||
        multiplayer_preview_player >= MULTIPLAYER_COUNT) {
        if (multiplayer_life_preview_timer != NULL) {
            lv_timer_pause(multiplayer_life_preview_timer);
        }
        return;
    }

    multiplayer_life[multiplayer_preview_player] = clamp_life(
        multiplayer_life[multiplayer_preview_player] + multiplayer_pending_life_delta
    );
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
    refresh_multiplayer_ui();
}

static void change_life(int delta)
{
    life_total = clamp_life(life_total + delta);
    life_delta_acc += delta;
    show_life_delta();
    refresh_main_ui();
}

static void add_damage_to_selected_enemy(int delta)
{
    if (selected_enemy < 0 || selected_enemy >= ENEMY_COUNT) return;

    enemies[selected_enemy].damage += delta;
    if (enemies[selected_enemy].damage < 0)
        enemies[selected_enemy].damage = 0;

    if (delta > 0) {
        change_life(-delta);
    } else if (delta < 0) {
        change_life(-delta);
    }

    refresh_damage_ui();
    refresh_select_ui();
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
    }
}

static void change_multiplayer_life(int delta)
{
    if (multiplayer_selected < 0 || multiplayer_selected >= MULTIPLAYER_COUNT) return;
    // if (mirror_enabled && multiplayer_selected <= 1) delta = -delta;
    multiplayer_life[multiplayer_selected] = clamp_life(multiplayer_life[multiplayer_selected] + delta);
    mp_delta_acc[multiplayer_selected] += delta;
    show_mp_delta(multiplayer_selected);
    refresh_multiplayer_ui();
}

static void change_cmd_main_damage(int delta)
{
    char buf[8];
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
    snprintf(buf, sizeof(buf), "%d", new_dmg);
    if (cmd_main_label[cmd_main_selected] != NULL)
        lv_label_set_text(cmd_main_label[cmd_main_selected], buf);
    refresh_main_ui();
}

static void change_cmd_mp_damage(int delta)
{
    static const int attackers[MULTIPLAYER_COUNT][3] = {
        {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}
    };
    char buf[8];
    int v, a, attacker, new_dmg, life_delta;
    v = cmd_mp_selected_victim;
    a = cmd_mp_selected_attacker;
    if (v < 0 || a < 0) return;
    // if (mirror_enabled && v <= 1) delta = -delta;
    attacker = attackers[v][a];
    new_dmg = multiplayer_cmd_damage_totals[v][attacker] + delta;
    if (new_dmg < 0) new_dmg = 0;
    life_delta = new_dmg - multiplayer_cmd_damage_totals[v][attacker];
    multiplayer_cmd_damage_totals[v][attacker] = new_dmg;
    multiplayer_life[v] = clamp_life(multiplayer_life[v] - life_delta);
    if (life_delta != 0) {
        mp_delta_acc[v] -= life_delta;
        show_mp_delta(v);
    }
    snprintf(buf, sizeof(buf), "%d", new_dmg);
    if (cmd_mp_label[v][a] != NULL)
        lv_label_set_text(cmd_mp_label[v][a], buf);
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

static void hide_main_menu(void)
{
    if (menu_overlay != NULL) {
        lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_main_menu(void)
{
    if (menu_overlay != NULL) {
        lv_obj_clear_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(menu_overlay);
    }
}

static void turn_timer_start_fresh(void)
{
    turn_number = 1;
    turn_elapsed_ms = 0;
    turn_started_ms = lv_tick_get();
    turn_timer_enabled = true;
    turn_indicator_visible = true;
    turn_ui_visible = true;
    turn_blink_steps_remaining = 10;

    if (turn_blink_timer != NULL) {
        lv_timer_resume(turn_blink_timer);
    }

    refresh_turn_ui();
}

static void turn_timer_reset(void)
{
    turn_timer_enabled = false;
    turn_elapsed_ms = 0;
    turn_started_ms = 0;
    turn_number = 0;
    turn_indicator_visible = true;
    turn_ui_visible = false;
    turn_blink_steps_remaining = 0;

    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }

    refresh_turn_ui();
}

static void reset_all_values(void)
{
    int i;

    life_total = DEFAULT_LIFE_TOTAL;
    pending_life_delta = 0;
    life_preview_active = false;
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;
    selected_enemy = -1;
    dice_result = 0;

    for (i = 0; i < ENEMY_COUNT; i++) {
        enemies[i].damage = 0;
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_life[i] = DEFAULT_LIFE_TOTAL;
        snprintf(multiplayer_names[i], sizeof(multiplayer_names[i]), "P%d", i + 1);
    }
    multiplayer_selected = 0;
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
            if (cmd_main_circle[ri] != NULL) lv_obj_set_style_border_width(cmd_main_circle[ri], 2, 0);
            if (cmd_main_label[ri] != NULL) lv_label_set_text(cmd_main_label[ri], "0");
        }
        cmd_main_selected = -1;
        if (cmd_mp_selected_victim >= 0 && cmd_mp_circle[cmd_mp_selected_victim][cmd_mp_selected_attacker] != NULL)
            lv_obj_set_style_border_width(cmd_mp_circle[cmd_mp_selected_victim][cmd_mp_selected_attacker], 2, 0);
        cmd_mp_selected_victim = -1;
        cmd_mp_selected_attacker = -1;
        for (ri = 0; ri < MULTIPLAYER_COUNT; ri++)
            for (rj = 0; rj < 3; rj++)
                if (cmd_mp_label[ri][rj] != NULL) lv_label_set_text(cmd_mp_label[ri][rj], "0");
    }

    if (life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    }
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
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
            if (label_multiplayer_delta[di] != NULL)
                lv_obj_add_flag(label_multiplayer_delta[di], LV_OBJ_FLAG_HIDDEN);
        }
        if (mp_delta_hide_timer != NULL)
            lv_timer_pause(mp_delta_hide_timer);
    }

    brightness_apply();
    turn_timer_reset();
    refresh_main_ui();
    refresh_select_ui();
    refresh_damage_ui();
    refresh_settings_ui();
    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_select_ui();
    refresh_damage_ui();
    refresh_multiplayer_all_damage_ui();
}

static void turn_timer_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_turn_ui();
}

static void turn_blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (turn_blink_steps_remaining == 0) {
        turn_indicator_visible = true;
        if (turn_blink_timer != NULL) {
            lv_timer_pause(turn_blink_timer);
        }
        refresh_turn_ui();
        return;
    }

    turn_indicator_visible = !turn_indicator_visible;
    turn_blink_steps_remaining--;
    refresh_turn_ui();
}

static void intro_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (intro_timer != NULL) {
        lv_timer_pause(intro_timer);
    }
    lv_scr_load_anim(screen_main, LV_SCR_LOAD_ANIM_FADE_OUT, 500, 0, false);
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

static void open_select_screen()
{
    refresh_select_ui();
    load_screen_if_needed(screen_select);
}

static void open_damage_screen(int enemy_index)
{
    selected_enemy = enemy_index;
    refresh_damage_ui();
    load_screen_if_needed(screen_damage);
}

static void open_settings_screen()
{
    settings_selected_field = 0;
    update_battery_measurement(true);
    refresh_settings_ui();
    load_screen_if_needed(screen_settings);
}

static void open_multiplayer_screen()
{
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

static void open_dice_screen(void)
{
    lv_anim_t anim;

    refresh_dice_ui();
    load_screen_if_needed(screen_dice);

    if (label_dice_result != NULL) {
        lv_obj_set_y(label_dice_result, -10);
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, label_dice_result);
        lv_anim_set_values(&anim, -10, -24);
        lv_anim_set_time(&anim, 120);
        lv_anim_set_playback_time(&anim, 140);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_start(&anim);
    }
}

static void back_to_main()
{
    refresh_main_ui();
    load_screen_if_needed(screen_main);
}

// ----------------------------------------------------
// events
// ----------------------------------------------------

static void event_open_select(lv_event_t *e)
{
    (void)e;
    open_select_screen();
}

static void event_open_settings(lv_event_t *e)
{
    (void)e;
    open_settings_screen();
}

static void event_toggle_auto_dim(lv_event_t *e)
{
    (void)e;
    auto_dim_enabled = lv_obj_has_state(switch_auto_dim, LV_STATE_CHECKED);
    settings_dirty = true;
    if (!auto_dim_enabled && dimmed) {
        dimmed = false;
        brightness_apply();
    }
}

static void event_settings_select_row(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < 3) {
        settings_selected_field = idx;
        refresh_settings_ui();
    }
}

static void event_select_itze(lv_event_t *e)
{
    (void)e;
    open_damage_screen(0);
}

static void event_select_atze(lv_event_t *e)
{
    (void)e;
    open_damage_screen(1);
}

static void event_select_utze(lv_event_t *e)
{
    (void)e;
    open_damage_screen(2);
}

static void event_back_main(lv_event_t *e)
{
    (void)e;
    back_to_main();
}

static void event_settings_back(lv_event_t *e)
{
    (void)e;
    settings_save();
    back_to_main();
}

static void event_show_main_menu(lv_event_t *e)
{
    (void)e;
    show_main_menu();
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
    back_to_main();
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

static void event_menu_turn_timer(lv_event_t *e)
{
    (void)e;
    turn_timer_start_fresh();
    hide_main_menu();
}

static void event_menu_dice(lv_event_t *e)
{
    (void)e;
    dice_result = (int)(esp_random() % 20U) + 1;
    hide_main_menu();
    open_dice_screen();
}

static void event_menu_cmd_damage(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
    open_select_screen();
}

static void event_menu_multiplayer(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
    open_multiplayer_screen();
}

static void event_cmd_main_circle(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (cmd_main_selected == i) {
        lv_obj_set_style_border_width(cmd_main_circle[i], 2, 0);
        cmd_main_selected = -1;
    } else {
        if (cmd_main_selected >= 0)
            lv_obj_set_style_border_width(cmd_main_circle[cmd_main_selected], 2, 0);
        cmd_main_selected = i;
        lv_obj_set_style_border_width(cmd_main_circle[i], 3, 0);
    }
    activity_kick();
}

static void event_cmd_mp_circle(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    int v = idx / 3, a = idx % 3;
    if (cmd_mp_selected_victim == v && cmd_mp_selected_attacker == a) {
        lv_obj_set_style_border_width(cmd_mp_circle[v][a], 2, 0);
        cmd_mp_selected_victim = -1;
        cmd_mp_selected_attacker = -1;
    } else {
        if (cmd_mp_selected_victim >= 0)
            lv_obj_set_style_border_width(cmd_mp_circle[cmd_mp_selected_victim][cmd_mp_selected_attacker], 2, 0);
        cmd_mp_selected_victim = v;
        cmd_mp_selected_attacker = a;
        lv_obj_set_style_border_width(cmd_mp_circle[v][a], 3, 0);
    }
    activity_kick();
}

static void event_menu_settings(lv_event_t *e)
{
    (void)e;
    hide_main_menu();
    open_settings_screen();
}

static void event_dice_tap(lv_event_t *e)
{
    (void)e;
    back_to_main();
    show_main_menu();
}

static void event_turn_tap(lv_event_t *e)
{
    (void)e;

    if (turn_number <= 0) {
        turn_number = 1;
        turn_elapsed_ms = 0;
    } else {
        turn_elapsed_ms = get_turn_elapsed_ms();
        turn_number++;
    }

    turn_started_ms = lv_tick_get();
    turn_timer_enabled = true;
    refresh_turn_ui();
}

static void event_multiplayer_select(lv_event_t *e)
{
    multiplayer_selected = (int)(intptr_t)lv_event_get_user_data(e);
    if (cmd_mp_selected_victim >= 0) {
        lv_obj_set_style_border_width(cmd_mp_circle[cmd_mp_selected_victim][cmd_mp_selected_attacker], 2, 0);
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
        if ((point.y - multiplayer_swipe_start.y) > 80 &&
            LV_ABS(point.x - multiplayer_swipe_start.x) < 90) {
            screen_before_menu = lv_scr_act();
            if (lv_scr_act() != screen_main) back_to_main();
            show_main_menu();
        }
    }
}

static void event_multiplayer_menu_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_screen();
}

static void event_multiplayer_menu_rename(lv_event_t *e)
{
    (void)e;
    open_multiplayer_name_screen();
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

    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_select_ui();
    refresh_damage_ui();
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

static void build_dice_screen()
{
    screen_dice = lv_obj_create(NULL);
    lv_obj_set_size(screen_dice, 360, 360);
    lv_obj_set_style_bg_color(screen_dice, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_dice, 0, 0);
    lv_obj_set_scrollbar_mode(screen_dice, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_dice, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_dice, event_dice_tap, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(screen_dice, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_dice, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    label_dice_result = lv_label_create(screen_dice);
    lv_label_set_text(label_dice_result, "--");
    lv_obj_set_style_text_color(label_dice_result, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_dice_result, &lv_font_montserrat_36, 0);
    lv_obj_align(label_dice_result, LV_ALIGN_CENTER, 0, -10);

    label_dice_hint = lv_label_create(screen_dice);
    lv_label_set_text(label_dice_hint, "tap to return");
    lv_obj_set_style_text_color(label_dice_hint, lv_color_hex(0x8A8A8A), 0);
    lv_obj_set_style_text_font(label_dice_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_dice_hint, LV_ALIGN_CENTER, 0, 42);
}

static void build_multiplayer_screen()
{
    static const char *player_names[MULTIPLAYER_COUNT] = {"P1", "P2", "P3", "P4"};
    static const lv_coord_t quad_x[MULTIPLAYER_COUNT] = {0, 180, 180, 0};
    static const lv_coord_t quad_y[MULTIPLAYER_COUNT] = {0, 0, 180, 180};
    int i;

    screen_multiplayer = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(screen_multiplayer, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_multiplayer, event_menu_swipe, LV_EVENT_RELEASED, NULL);

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

        if (i < 2) {
            /* P0, P1: canvas objects backed by PSRAM for mirror rotation */
            canvas_mp_name_buf[i] = (uint8_t *)heap_caps_malloc(
                CMP_NAME_BUF_SIZE, MALLOC_CAP_SPIRAM);
            canvas_mp_name[i] = lv_canvas_create(screen_multiplayer);
            lv_canvas_set_buffer(canvas_mp_name[i], canvas_mp_name_buf[i],
                                 CMP_NAME_W, CMP_NAME_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
            lv_img_set_pivot(canvas_mp_name[i], CMP_NAME_W / 2, CMP_NAME_H / 2);
            lv_obj_clear_flag(canvas_mp_name[i], LV_OBJ_FLAG_CLICKABLE);
            label_multiplayer_name[i] = NULL;

            canvas_mp_life_buf[i] = (uint8_t *)heap_caps_malloc(
                CMP_LIFE_BUF_SIZE, MALLOC_CAP_SPIRAM);
            canvas_mp_life[i] = lv_canvas_create(screen_multiplayer);
            lv_canvas_set_buffer(canvas_mp_life[i], canvas_mp_life_buf[i],
                                 CMP_LIFE_W, CMP_LIFE_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
            lv_img_set_pivot(canvas_mp_life[i], CMP_LIFE_W / 2, CMP_LIFE_H / 2);
            lv_obj_clear_flag(canvas_mp_life[i], LV_OBJ_FLAG_CLICKABLE);
            label_multiplayer_life[i] = NULL;
            label_multiplayer_delta[i] = NULL;
        } else {
            /* P2, P3: plain labels, no rotation needed */
            label_multiplayer_name[i] = lv_label_create(screen_multiplayer);
            lv_label_set_text(label_multiplayer_name[i], player_names[i]);
            lv_obj_set_style_text_color(label_multiplayer_name[i], lv_color_white(), 0);
            lv_obj_set_style_text_font(label_multiplayer_name[i], &lv_font_montserrat_14, 0);

            label_multiplayer_life[i] = lv_label_create(screen_multiplayer);
            lv_label_set_text(label_multiplayer_life[i], "40");
            lv_obj_set_style_text_color(label_multiplayer_life[i], lv_color_white(), 0);
            lv_obj_set_style_text_font(label_multiplayer_life[i], &lv_font_montserrat_36, 0);

            label_multiplayer_delta[i] = NULL;
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

    refresh_multiplayer_ui();

    lv_obj_t *batt_mp_bg = lv_obj_create(screen_multiplayer);
    lv_obj_set_size(batt_mp_bg, 84, 36);
    lv_obj_set_style_bg_color(batt_mp_bg, lv_color_hex(0x0D0B1A), 0);
    lv_obj_set_style_bg_opa(batt_mp_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(batt_mp_bg, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(batt_mp_bg, 1, 0);
    lv_obj_set_style_radius(batt_mp_bg, 6, 0);
    lv_obj_set_style_pad_all(batt_mp_bg, 0, 0);
    lv_obj_clear_flag(batt_mp_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(batt_mp_bg, LV_ALIGN_TOP_MID, 0, 6);

    bar_multiplayer_battery = lv_bar_create(screen_multiplayer);
    lv_obj_set_size(bar_multiplayer_battery, 62, 10);
    lv_obj_clear_flag(bar_multiplayer_battery, LV_OBJ_FLAG_CLICKABLE);
    lv_bar_set_range(bar_multiplayer_battery, 0, 100);
    lv_bar_set_value(bar_multiplayer_battery, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_multiplayer_battery, lv_color_hex(0x1E1B3A), LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_multiplayer_battery, lv_color_hex(0x7A6F99), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_multiplayer_battery, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_multiplayer_battery, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_multiplayer_battery, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_multiplayer_battery, lv_color_hex(0x4A4060), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_multiplayer_battery, 2, LV_PART_INDICATOR);
    lv_obj_align(bar_multiplayer_battery, LV_ALIGN_TOP_MID, 0, 11);

    label_multiplayer_battery = lv_label_create(screen_multiplayer);
    lv_label_set_text(label_multiplayer_battery, "--%");
    lv_obj_set_style_text_color(label_multiplayer_battery, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_multiplayer_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(label_multiplayer_battery, LV_ALIGN_TOP_MID, 0, 27);

    /* Commander damage circles — 34px, 10px gap
       Top row y=128 (18px above center divider): P0 left, P1 right
       Bottom row y=198 (18px below center divider): P2 right, P3 left
       Attacker colors match get_player_active_color(attacker_index) */
    {
        static const int mp_attackers[MULTIPLAYER_COUNT][3] = {
            {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}
        };
        static const lv_coord_t mp_cx[MULTIPLAYER_COUNT][3] = {
            {29,  73,  117}, /* P0 top-left  */
            {209, 253, 297}, /* P1 top-right */
            {209, 253, 297}, /* P2 bot-right */
            {29,  73,  117}, /* P3 bot-left  */
        };
        static const lv_coord_t mp_cy[MULTIPLAYER_COUNT] = {128, 128, 198, 198};
        int v, a;
        for (v = 0; v < MULTIPLAYER_COUNT; v++) {
            for (a = 0; a < 3; a++) {
                lv_color_t ac = get_player_active_color(mp_attackers[v][a]);
                lv_obj_t *c = lv_obj_create(screen_multiplayer);
                lv_obj_set_size(c, 34, 34);
                lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_bg_color(c, ac, 0);
                lv_obj_set_style_bg_opa(c, LV_OPA_30, 0);
                lv_obj_set_style_border_color(c, ac, 0);
                lv_obj_set_style_border_width(c, 2, 0);
                lv_obj_set_style_pad_all(c, 0, 0);
                lv_obj_set_style_shadow_width(c, 0, 0);
                lv_obj_set_pos(c, mp_cx[v][a], mp_cy[v]);
                lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(c, event_cmd_mp_circle, LV_EVENT_CLICKED,
                                    (void *)(intptr_t)(v * 3 + a));
                cmd_mp_circle[v][a] = c;
                if (v < 2) {
                    /* P0, P1: canvas on screen_multiplayer (no circular clip) */
                    canvas_cmd_mp_buf[v][a] = (uint8_t *)heap_caps_malloc(
                        CMP_CMD_BUF_SIZE, MALLOC_CAP_SPIRAM);
                    canvas_cmd_mp[v][a] = lv_canvas_create(screen_multiplayer);
                    lv_canvas_set_buffer(canvas_cmd_mp[v][a], canvas_cmd_mp_buf[v][a],
                                         CMP_CMD_W, CMP_CMD_H, LV_IMG_CF_TRUE_COLOR_ALPHA);
                    lv_img_set_pivot(canvas_cmd_mp[v][a], CMP_CMD_W / 2, CMP_CMD_H / 2);
                    lv_obj_set_pos(canvas_cmd_mp[v][a], mp_cx[v][a], mp_cy[v]);
                    lv_obj_clear_flag(canvas_cmd_mp[v][a], LV_OBJ_FLAG_CLICKABLE);
                    cmd_mp_label[v][a] = NULL;
                } else {
                    /* P2, P3: regular label inside circle */
                    lv_obj_t *lbl = lv_label_create(c);
                    lv_label_set_text(lbl, "0");
                    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
                    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
                    lv_obj_center(lbl);
                    cmd_mp_label[v][a] = lbl;
                }
            }
        }
    }
}

static void build_multiplayer_menu_screen()
{
    screen_multiplayer_menu = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_menu, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_menu, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_menu, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_menu, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_menu_title = lv_label_create(screen_multiplayer_menu);
    lv_obj_set_style_text_color(label_multiplayer_menu_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_menu_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_menu_title, LV_ALIGN_TOP_MID, 0, 26);

    lv_obj_t *btn = make_button(screen_multiplayer_menu, "Rename", 180, 46, event_multiplayer_menu_rename);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -36);

    btn = make_button(screen_multiplayer_menu, "Cmd.dmg", 180, 46, event_multiplayer_menu_cmd_damage);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 24);

    btn = make_button(screen_multiplayer_menu, "All.dmg", 180, 46, event_multiplayer_menu_all_damage);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 84);

    btn = make_button(screen_multiplayer_menu, "Back", 120, 46, event_multiplayer_menu_back);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -26);
}

static void build_multiplayer_name_screen()
{
    screen_multiplayer_name = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_name, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_name, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_multiplayer_name, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_name, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_name_title = lv_label_create(screen_multiplayer_name);
    lv_obj_set_style_text_color(label_multiplayer_name_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_name_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_name_title, LV_ALIGN_TOP_MID, 0, 18);

    textarea_multiplayer_name = lv_textarea_create(screen_multiplayer_name);
    lv_obj_set_size(textarea_multiplayer_name, 240, 44);
    lv_obj_align(textarea_multiplayer_name, LV_ALIGN_TOP_MID, 0, 56);
    lv_textarea_set_max_length(textarea_multiplayer_name, 15);
    lv_textarea_set_one_line(textarea_multiplayer_name, true);

    keyboard_multiplayer_name = lv_keyboard_create(screen_multiplayer_name);
    lv_obj_set_size(keyboard_multiplayer_name, 360, 170);
    lv_obj_align(keyboard_multiplayer_name, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard_multiplayer_name, textarea_multiplayer_name);

    lv_obj_t *btn = make_button(screen_multiplayer_name, "Save", 88, 38, event_multiplayer_name_save);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -24, 116);

    btn = make_button(screen_multiplayer_name, "Back", 88, 38, event_multiplayer_name_back);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 24, 116);
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
    lv_obj_set_style_text_font(label_multiplayer_cmd_select_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_cmd_select_title, LV_ALIGN_TOP_MID, 0, 22);

    for (i = 0; i < (MULTIPLAYER_COUNT - 1); i++) {
        button_multiplayer_cmd_target[i] = lv_btn_create(screen_multiplayer_cmd_select);
        lv_obj_set_size(button_multiplayer_cmd_target[i], 220, 46);
        lv_obj_align(button_multiplayer_cmd_target[i], LV_ALIGN_CENTER, 0, -42 + (i * 58));
        lv_obj_add_event_cb(button_multiplayer_cmd_target[i], event_multiplayer_cmd_target_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        label_multiplayer_cmd_target[i] = lv_label_create(button_multiplayer_cmd_target[i]);
        lv_obj_set_style_text_font(label_multiplayer_cmd_target[i], &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(label_multiplayer_cmd_target[i], lv_color_white(), 0);
        lv_obj_center(label_multiplayer_cmd_target[i]);
    }

    lv_obj_t *btn_back = make_button(screen_multiplayer_cmd_select, "Back", 120, 46, event_multiplayer_cmd_select_back);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
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
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_cmd_damage_title, LV_ALIGN_TOP_MID, 0, 26);

    label_multiplayer_cmd_damage_value = lv_label_create(screen_multiplayer_cmd_damage);
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_value, &lv_font_montserrat_36, 0);
    lv_obj_align(label_multiplayer_cmd_damage_value, LV_ALIGN_CENTER, 0, -8);

    label_multiplayer_cmd_damage_hint = lv_label_create(screen_multiplayer_cmd_damage);
    lv_label_set_text(label_multiplayer_cmd_damage_hint, "Turn knob for damage");
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_hint, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_multiplayer_cmd_damage_hint, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t *btn_back = make_button(screen_multiplayer_cmd_damage, "Back", 120, 46, event_multiplayer_cmd_damage_back);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
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
    lv_obj_set_style_text_font(label_multiplayer_all_damage_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_all_damage_title, LV_ALIGN_TOP_MID, 0, 26);

    label_multiplayer_all_damage_value = lv_label_create(screen_multiplayer_all_damage);
    lv_obj_set_style_text_color(label_multiplayer_all_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_value, &lv_font_montserrat_36, 0);
    lv_obj_align(label_multiplayer_all_damage_value, LV_ALIGN_CENTER, 0, -8);

    label_multiplayer_all_damage_hint = lv_label_create(screen_multiplayer_all_damage);
    lv_label_set_text(label_multiplayer_all_damage_hint, "Turn knob, then apply");
    lv_obj_set_style_text_color(label_multiplayer_all_damage_hint, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_multiplayer_all_damage_hint, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t *btn = make_button(screen_multiplayer_all_damage, "Apply", 120, 46, event_multiplayer_all_damage_apply);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -78);

    btn = make_button(screen_multiplayer_all_damage, "Back", 120, 46, event_multiplayer_all_damage_back);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -24);
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

    arc_life = lv_arc_create(screen_main);
    lv_obj_set_size(arc_life, 360, 360);
    lv_obj_center(arc_life);
    lv_arc_set_rotation(arc_life, 90);
    lv_arc_set_bg_angles(arc_life, 0, 360);
    lv_arc_set_range(arc_life, 0, 40);
    lv_arc_set_value(arc_life, get_arc_display_value(life_total));
    lv_obj_remove_style(arc_life, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_life, LV_OBJ_FLAG_CLICKABLE);



    life_hitbox = make_plain_box(screen_main, 320, 188);
    lv_obj_align(life_hitbox, LV_ALIGN_CENTER, 0, -8);
    lv_obj_add_flag(life_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(life_hitbox, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(life_hitbox, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    life_container = make_plain_box(screen_main, 290, 112);
    lv_obj_align(life_container, LV_ALIGN_CENTER, 0, -6);

    digit_box_sign = make_plain_box(life_container, 60, 112);
    create_digit(digit_box_sign, digit_sign);
    digit_sign_plus_vert = make_seg(digit_box_sign, 27, 34, 6, 40);
    lv_obj_set_style_radius(digit_sign_plus_vert, 2, 0);
    lv_obj_add_flag(digit_sign_plus_vert, LV_OBJ_FLAG_HIDDEN);

    digit_box_hundreds = make_plain_box(life_container, 60, 112);
    create_digit(digit_box_hundreds, digit_hundreds);

    digit_box_tens = make_plain_box(life_container, 60, 112);
    create_digit(digit_box_tens, digit_tens);

    digit_box_ones = make_plain_box(life_container, 60, 112);
    create_digit(digit_box_ones, digit_ones);

    label_life_delta = lv_label_create(screen_main);
    lv_label_set_text(label_life_delta, "+0");
    lv_obj_set_style_text_color(label_life_delta, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_text_font(label_life_delta, &lv_font_montserrat_22, 0);
    lv_obj_align(label_life_delta, LV_ALIGN_CENTER, 118, 10);
    lv_obj_add_flag(label_life_delta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_life_delta, LV_OBJ_FLAG_CLICKABLE);

    turn_container = make_plain_box(screen_main, 96, 96);
    lv_obj_align(turn_container, LV_ALIGN_CENTER, 110, -6);
    lv_obj_add_flag(turn_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(turn_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(turn_container, event_turn_tap, LV_EVENT_CLICKED, NULL);

    label_turn = lv_label_create(turn_container);
    lv_label_set_text(label_turn, "Turn");
    lv_obj_set_style_text_color(label_turn, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_turn, &lv_font_montserrat_22, 0);
    lv_obj_align(label_turn, LV_ALIGN_TOP_MID, 0, 10);

    label_turn_time = lv_label_create(turn_container);
    lv_label_set_text(label_turn_time, "0:00");
    lv_obj_set_style_text_color(label_turn_time, lv_color_hex(0xB0A3D4), 0);
    lv_obj_set_style_text_font(label_turn_time, &lv_font_montserrat_22, 0);
    lv_obj_align(label_turn_time, LV_ALIGN_TOP_MID, -8, 48);

    turn_live_dot = lv_obj_create(turn_container);
    lv_obj_remove_style_all(turn_live_dot);
    lv_obj_set_size(turn_live_dot, 12, 12);
    lv_obj_set_style_radius(turn_live_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(turn_live_dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(turn_live_dot, LV_OPA_COVER, 0);
    lv_obj_align(turn_live_dot, LV_ALIGN_TOP_MID, 28, 52);
    lv_obj_add_flag(turn_live_dot, LV_OBJ_FLAG_HIDDEN);


    menu_overlay = make_plain_box(screen_main, 360, 360);
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
        lv_obj_t *side;
        lv_obj_t *lbl;

        side = make_button(menu_overlay, "Reset", 50, 50, event_menu_reset);
        lv_obj_set_style_radius(side, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(side, lv_color_hex(0x1A1630), 0);
        lv_obj_set_style_bg_opa(side, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(side, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(side, 1, 0);
        lv_obj_set_style_shadow_width(side, 0, 0);
        lv_obj_set_style_pad_all(side, 0, 0);
        lv_obj_align(side, LV_ALIGN_CENTER, -148, 0);
        lbl = lv_obj_get_child(side, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xB0A3D4), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        }

        side = make_button(menu_overlay, "Back", 50, 50, event_menu_back);
        lv_obj_set_style_radius(side, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(side, lv_color_hex(0x1A1630), 0);
        lv_obj_set_style_bg_opa(side, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(side, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(side, 1, 0);
        lv_obj_set_style_shadow_width(side, 0, 0);
        lv_obj_set_style_pad_all(side, 0, 0);
        lv_obj_align(side, LV_ALIGN_CENTER, 148, 0);
        lbl = lv_obj_get_child(side, 0);
        if (lbl) {
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xB0A3D4), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        }
    }

    /* Menu items — ArcMind dark card style */
    {
        lv_obj_t *lbl;

        menu_button_multiplayer = make_button(menu_panel, "Multiplayer", 150, 34, event_menu_multiplayer);
        lv_obj_set_style_bg_color(menu_button_multiplayer, lv_color_hex(0x1E1B3A), 0);
        lv_obj_set_style_bg_opa(menu_button_multiplayer, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(menu_button_multiplayer, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(menu_button_multiplayer, 1, 0);
        lv_obj_set_style_radius(menu_button_multiplayer, 8, 0);
        lv_obj_set_style_shadow_width(menu_button_multiplayer, 0, 0);
        lv_obj_set_style_pad_all(menu_button_multiplayer, 0, 0);
        lv_obj_align(menu_button_multiplayer, LV_ALIGN_TOP_MID, 0, 50);
        lbl = lv_obj_get_child(menu_button_multiplayer, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0xC4B5FD), 0);

        menu_button_timer = make_button(menu_panel, "Timer", 150, 34, event_menu_turn_timer);
        lv_obj_set_style_bg_color(menu_button_timer, lv_color_hex(0x1E1B3A), 0);
        lv_obj_set_style_bg_opa(menu_button_timer, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(menu_button_timer, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(menu_button_timer, 1, 0);
        lv_obj_set_style_radius(menu_button_timer, 8, 0);
        lv_obj_set_style_shadow_width(menu_button_timer, 0, 0);
        lv_obj_set_style_pad_all(menu_button_timer, 0, 0);
        lv_obj_align(menu_button_timer, LV_ALIGN_TOP_MID, 0, 92);
        lbl = lv_obj_get_child(menu_button_timer, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0xC4B5FD), 0);

        btn = make_button(menu_panel, "Settings", 150, 34, event_menu_settings);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1B3A), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 134);
        lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0xC4B5FD), 0);
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
    lv_obj_align(menu_brightness_bar, LV_ALIGN_TOP_MID, 0, 174);

    menu_brightness_label = lv_label_create(menu_panel);
    lv_label_set_text(menu_brightness_label, "30%");
    lv_obj_set_style_text_color(menu_brightness_label, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(menu_brightness_label, &lv_font_montserrat_14, 0);
    lv_obj_align(menu_brightness_label, LV_ALIGN_TOP_MID, 0, 187);
    refresh_menu_brightness_ui();

    {
        lv_obj_t *batt_main_bg = lv_obj_create(screen_main);
        lv_obj_set_size(batt_main_bg, 84, 36);
        lv_obj_set_style_bg_color(batt_main_bg, lv_color_hex(0x0D0B1A), 0);
        lv_obj_set_style_bg_opa(batt_main_bg, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(batt_main_bg, lv_color_hex(0x4C1D95), 0);
        lv_obj_set_style_border_width(batt_main_bg, 1, 0);
        lv_obj_set_style_radius(batt_main_bg, 6, 0);
        lv_obj_set_style_pad_all(batt_main_bg, 0, 0);
        lv_obj_clear_flag(batt_main_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(batt_main_bg, LV_ALIGN_TOP_MID, 0, 6);
    }

    bar_main_battery = lv_bar_create(screen_main);
    lv_obj_set_size(bar_main_battery, 62, 10);
    lv_obj_clear_flag(bar_main_battery, LV_OBJ_FLAG_CLICKABLE);
    lv_bar_set_range(bar_main_battery, 0, 100);
    lv_bar_set_value(bar_main_battery, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_main_battery, lv_color_hex(0x1E1B3A), LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_main_battery, lv_color_hex(0x7A6F99), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_main_battery, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_main_battery, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_main_battery, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_main_battery, lv_color_hex(0x4A4060), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_main_battery, 2, LV_PART_INDICATOR);
    lv_obj_align(bar_main_battery, LV_ALIGN_TOP_MID, 0, 11);

    label_main_battery = lv_label_create(screen_main);
    lv_label_set_text(label_main_battery, "--%");
    lv_obj_set_style_text_color(label_main_battery, lv_color_hex(0xC4B5FD), 0);
    lv_obj_set_style_text_font(label_main_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(label_main_battery, LV_ALIGN_TOP_MID, 0, 27);

    /* Commander damage circles — P2(cyan), P3(yellow), P4(green)
       32px circles, 18px gap, centered at y=256+16=272 (92px below display center)
       Left edges: 114, 164, 214 */
    {
        static const uint32_t cmd_colors[3] = {0x4FC3F7, 0xFFEA61, 0xC8E6C9};
        static const char *cmd_names[3] = {"P2", "P3", "P4"};
        static const lv_coord_t cmd_cx[3] = {114, 164, 214};
        int ci;
        for (ci = 0; ci < 3; ci++) {
            lv_color_t cc = lv_color_hex(cmd_colors[ci]);
            lv_obj_t *c = lv_obj_create(screen_main);
            lv_obj_set_size(c, 32, 32);
            lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(c, cc, 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_30, 0);
            lv_obj_set_style_border_color(c, cc, 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_pad_all(c, 0, 0);
            lv_obj_set_style_shadow_width(c, 0, 0);
            lv_obj_set_pos(c, cmd_cx[ci], 241);
            lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(c, event_cmd_main_circle, LV_EVENT_CLICKED, (void *)(intptr_t)ci);
            lv_obj_t *val_lbl = lv_label_create(c);
            lv_label_set_text(val_lbl, "0");
            lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(val_lbl, lv_color_white(), 0);
            lv_obj_center(val_lbl);
            cmd_main_circle[ci] = c;
            cmd_main_label[ci] = val_lbl;
            lv_obj_t *name_lbl = lv_label_create(screen_main);
            lv_label_set_text(name_lbl, cmd_names[ci]);
            lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x7A6F99), 0);
            lv_obj_align_to(name_lbl, c, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
        }
    }
}

static void build_select_screen()
{
    int i;
    static lv_event_cb_t select_cb[ENEMY_COUNT] = {
        event_select_itze, event_select_atze, event_select_utze
    };

    screen_select = lv_obj_create(NULL);
    lv_obj_set_size(screen_select, 360, 360);
    lv_obj_set_style_bg_color(screen_select, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_select, 0, 0);
    lv_obj_set_scrollbar_mode(screen_select, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_select, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_select, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_select, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    label_select_title = lv_label_create(screen_select);
    lv_label_set_text(label_select_title, "Choose player");
    lv_obj_set_style_text_color(label_select_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_select_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_select_title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *btn_back = make_button(screen_select, "Back", 88, 42, event_back_main);
    lv_obj_align(btn_back, LV_ALIGN_TOP_MID, 0, 60);

    for (i = 0; i < ENEMY_COUNT; i++) {
        lv_obj_t *row = lv_btn_create(screen_select);
        lv_obj_set_size(row, 220, 46);
        lv_obj_align(row, LV_ALIGN_CENTER, 0, -30 + (i * 62));
        lv_obj_add_event_cb(row, select_cb[i], LV_EVENT_CLICKED, NULL);

        label_enemy_name[i] = lv_label_create(row);
        lv_obj_set_style_text_font(label_enemy_name[i], &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(label_enemy_name[i], lv_color_white(), 0);
        lv_obj_align(label_enemy_name[i], LV_ALIGN_LEFT_MID, 16, 0);

        label_enemy_damage[i] = lv_label_create(row);
        lv_obj_set_style_text_font(label_enemy_damage[i], &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(label_enemy_damage[i], lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_align(label_enemy_damage[i], LV_ALIGN_RIGHT_MID, -16, 0);
    }
}

static void build_damage_screen()
{
    screen_damage = lv_obj_create(NULL);
    lv_obj_set_size(screen_damage, 360, 360);
    lv_obj_set_style_bg_color(screen_damage, lv_color_hex(0x0A0518), 0);
    lv_obj_set_style_border_width(screen_damage, 0, 0);
    lv_obj_set_scrollbar_mode(screen_damage, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(screen_damage, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_damage, event_menu_swipe, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_damage, event_menu_swipe, LV_EVENT_RELEASED, NULL);

    label_damage_title = lv_label_create(screen_damage);
    lv_label_set_text(label_damage_title, "P1");
    lv_obj_set_style_text_color(label_damage_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_damage_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_damage_title, LV_ALIGN_TOP_MID, 0, 28);

    label_damage_value = lv_label_create(screen_damage);
    lv_label_set_text(label_damage_value, "Damage: 0");
    lv_obj_set_style_text_color(label_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_damage_value, &lv_font_montserrat_36, 0);
    lv_obj_align(label_damage_value, LV_ALIGN_CENTER, 0, -10);

    label_damage_hint = lv_label_create(screen_damage);
    lv_label_set_text(label_damage_hint, "Turn knob for damage");
    lv_obj_set_style_text_color(label_damage_hint, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_damage_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_damage_hint, LV_ALIGN_BOTTOM_MID, 0, -76);

    lv_obj_t *btn_back = make_button(screen_damage, "Back", 120, 52, event_back_main);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -22);
}

static void build_settings_screen()
{
    static const char *field_names[3] = {"Brightness", "Auto-dim", "Mirror"};
    static const lv_coord_t row_offsets_y[3] = {-55, 0, 55};
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
    lv_label_set_text(label_settings_title, "Settings");
    lv_obj_set_style_text_color(label_settings_title, lv_color_hex(0x7A6F99), 0);
    lv_obj_set_style_text_font(label_settings_title, &lv_font_montserrat_14, 0);
    lv_obj_align(label_settings_title, LV_ALIGN_TOP_MID, 0, 32);

    /* Setting rows */
    for (i = 0; i < 3; i++) {
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
        lv_obj_align(row, LV_ALIGN_CENTER, 0, row_offsets_y[i]);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, event_settings_select_row, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        settings_row[i] = row;

        lv_obj_t *name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, field_names[i]);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(0xB0A3D4), 0);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *val_lbl = lv_label_create(row);
        lv_label_set_text(val_lbl, "---");
        lv_obj_set_style_text_color(val_lbl, lv_color_hex(0x8B5CF6), 0);
        lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_14, 0);
        lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, 0, 0);
        settings_row_value[i] = val_lbl;
    }

    /* Back button */
    btn_back = make_button(screen_settings, "Back", 150, 34, event_settings_back);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x1E1B3A), 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_back, lv_color_hex(0x4C1D95), 0);
    lv_obj_set_style_border_width(btn_back, 1, 0);
    lv_obj_set_style_radius(btn_back, 8, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -22);
    lbl = lv_obj_get_child(btn_back, 0);
    if (lbl) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xC4B5FD), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    }

    refresh_settings_ui();
}

void knob_gui(void)
{
    nvs_flash_init();
    brightness_init();
    settings_load();
    brightness_apply();

    build_intro_screen();
    build_dice_screen();
    build_main_screen();
    build_multiplayer_screen();
    build_multiplayer_menu_screen();
    build_multiplayer_name_screen();
    build_multiplayer_cmd_select_screen();
    build_multiplayer_cmd_damage_screen();
    build_multiplayer_all_damage_screen();
    build_select_screen();
    build_damage_screen();
    build_settings_screen();

    refresh_main_ui();
    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_select_ui();
    refresh_damage_ui();
    refresh_multiplayer_all_damage_ui();
    refresh_select_ui();
    refresh_settings_ui();

    turn_timer = lv_timer_create(turn_timer_tick_cb, 1000, NULL);
    if (turn_timer != NULL) {
        lv_timer_ready(turn_timer);
    }

    turn_blink_timer = lv_timer_create(turn_blink_timer_cb, 500, NULL);
    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }

    intro_timer = lv_timer_create(intro_timer_cb, 2500, NULL);
    life_preview_timer = lv_timer_create(life_preview_commit_cb, 4000, NULL);
    if (life_preview_timer != NULL) {
        lv_timer_pause(life_preview_timer);
    }
    multiplayer_life_preview_timer = lv_timer_create(multiplayer_life_preview_commit_cb, 4000, NULL);
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
    life_delta_hide_timer = lv_timer_create(life_delta_hide_cb, 2000, NULL);
    if (life_delta_hide_timer != NULL) {
        lv_timer_pause(life_delta_hide_timer);
    }
    mp_delta_hide_timer = lv_timer_create(mp_delta_hide_cb, 2000, NULL);
    if (mp_delta_hide_timer != NULL) {
        lv_timer_pause(mp_delta_hide_timer);
    }
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

    if (lv_scr_act() == screen_intro)
    {
        return;
    }
    else if (lv_scr_act() == screen_main)
    {
        if ((menu_overlay != NULL) && !lv_obj_has_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN)) {
            if (k == KNOB_LEFT)       change_brightness(-1);
            else if (k == KNOB_RIGHT) change_brightness(+1);
            return;
        }

        if (cmd_main_selected >= 0) {
            if (k == KNOB_LEFT)       change_cmd_main_damage(-1);
            else if (k == KNOB_RIGHT) change_cmd_main_damage(+1);
        } else {
            if (k == KNOB_LEFT)       change_life(-1);
            else if (k == KNOB_RIGHT) change_life(+1);
        }
    }
    else if (lv_scr_act() == screen_damage)
    {
        if (k == KNOB_LEFT)      add_damage_to_selected_enemy(-1);
        else if (k == KNOB_RIGHT) add_damage_to_selected_enemy(+1);
    }
    else if (lv_scr_act() == screen_settings)
    {
        if (k == KNOB_LEFT)      change_settings_field(-1);
        else if (k == KNOB_RIGHT) change_settings_field(+1);
    }
    else if (lv_scr_act() == screen_multiplayer)
    {
        if (cmd_mp_selected_victim >= 0) {
            if (k == KNOB_LEFT)       change_cmd_mp_damage(-1);
            else if (k == KNOB_RIGHT) change_cmd_mp_damage(+1);
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
