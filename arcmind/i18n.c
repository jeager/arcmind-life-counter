#include "i18n.h"

static app_language_t current_language = LANG_EN;

static const char *strings[LANG_COUNT][I18N_COUNT] = {
    {
        "Settings",
        "Brightness",
        "Auto-dim",
        "Mirror",
        "Timer",
        "Table",
        "Language",
        "on",
        "off",
        "round",
        "rect",
        "Back",
        "Reset",
        "Multiplayer",
        "1 Player",
        "Pick 1st player",
        "Rename",
        "Cmd.dmg",
        "All.dmg",
        "Save",
        "Apply",
        "All players",
        "Damage: %d",
        "Turn knob for damage",
        "Turn knob, then apply",
        "%s menu",
        "Rename %s",
        "%s -> target",
        "battery  %d%%",
        "battery  --%",
        "no calibrated reading",
        "%.2fV calibrated",
        "go P%d",
        "TIME!",
    },
    {
        "Configurações",
        "Brilho",
        "Auto-diminuir",
        "Espelho",
        "Temporizador",
        "Mesa",
        "Idioma",
        "ligado",
        "desligado",
        "redonda",
        "retangular",
        "Voltar",
        "Zerar",
        "Multijogador",
        "1 Jogador",
        "Escolher 1º jogador",
        "Renomear",
        "Dano cmte.",
        "Dano a todos",
        "Salvar",
        "Aplicar",
        "Todos os jogadores",
        "Dano: %d",
        "Gire o botão para o dano",
        "Gire o botão e aplique",
        "Menu %s",
        "Renomear %s",
        "%s -> alvo",
        "bateria  %d%%",
        "bateria  --%",
        "leitura não calibrada",
        "%.2fV calibrado",
        "vai P%d",
        "TEMPO!",
    },
};

static const char *language_names[LANG_COUNT] = {
    "English",
    "Português (PT/BR)",
};

const char *i18n_get(i18n_id_t id)
{
    if (id < 0 || id >= I18N_COUNT) return "";
    return strings[current_language][id];
}

const char *i18n_language_name(int index)
{
    if (index < 0 || index >= LANG_COUNT) return "";
    return language_names[index];
}

app_language_t i18n_get_language(void)
{
    return current_language;
}

void i18n_set_language(app_language_t lang)
{
    if (lang < 0 || lang >= LANG_COUNT) return;
    current_language = lang;
}

void i18n_set_language_index(int index)
{
    i18n_set_language((app_language_t)index);
}

int i18n_get_language_index(void)
{
    return (int)current_language;
}
