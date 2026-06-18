#ifndef _I18N_H
#define _I18N_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef enum {
    LANG_EN = 0,
    LANG_PT = 1,
    LANG_COUNT = 2
} app_language_t;

typedef enum {
    I18N_SETTINGS,
    I18N_BRIGHTNESS,
    I18N_AUTO_DIM,
    I18N_MIRROR,
    I18N_TIMER,
    I18N_TABLE,
    I18N_PLAYERS,
    I18N_LANGUAGE,
    I18N_ON,
    I18N_OFF,
    I18N_ROUND,
    I18N_RECT,
    I18N_BACK,
    I18N_RESET,
    I18N_MULTIPLAYER,
    I18N_ONE_PLAYER,
    I18N_SELECT_FIRST_PLAYER,
    I18N_RENAME,
    I18N_PICK_COLOR,
    I18N_CMD_DMG,
    I18N_ALL_DMG,
    I18N_SAVE,
    I18N_APPLY,
    I18N_ALL_PLAYERS,
    I18N_DAMAGE_FMT,
    I18N_CMD_DAMAGE_HINT,
    I18N_ALL_DAMAGE_HINT,
    I18N_MENU_FMT,
    I18N_RENAME_FMT,
    I18N_TARGET_FMT,
    I18N_BATTERY_FMT,
    I18N_BATTERY_UNKNOWN,
    I18N_NO_CALIBRATION,
    I18N_VOLTAGE_FMT,
    I18N_GO_PLAYER_FMT,
    I18N_TIME_EXPIRED,
    I18N_COUNT
} i18n_id_t;

LV_FONT_DECLARE(lv_font_montserrat_14_pt);
LV_FONT_DECLARE(lv_font_montserrat_22_pt);

#define UI_FONT_14 (&lv_font_montserrat_14_pt)
#define UI_FONT_22 (&lv_font_montserrat_22_pt)

const char *i18n_get(i18n_id_t id);
const char *i18n_language_name(int index);
app_language_t i18n_get_language(void);
void i18n_set_language(app_language_t lang);
void i18n_set_language_index(int index);
int i18n_get_language_index(void);

#ifdef __cplusplus
}
#endif

#endif
