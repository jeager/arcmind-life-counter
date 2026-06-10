#ifndef _PINCFG_H_
#define _PINCFG_H_

#define TFT_BLK 47

#define TFT_RST 21
#define TFT_CS 14
#define TFT_SCK 13
#define TFT_SDA0 15
#define TFT_SDA1 16
#define TFT_SDA2 17
#define TFT_SDA3 18
#define BTN_PIN 0


#define BTN_PIN 0

#define TOUCH_PIN_NUM_I2C_SCL 12
#define TOUCH_PIN_NUM_I2C_SDA 11
#define TOUCH_PIN_NUM_INT 9
#define TOUCH_PIN_NUM_RST 10

#define ROTARY_ENC_PIN_A 8
#define ROTARY_ENC_PIN_B 7

#define SD_MMC_D0_PIN 15
#define SD_MMC_D1_PIN 16
#define SD_MMC_D2_PIN 17
#define SD_MMC_D3_PIN 18
#define SD_MMC_CLK_PIN 13
#define SD_MMC_CMD_PIN 14

// NOTE: GPIO 16,17,18 are shared with TFT_SDA1/2/3 (QSPI display lines).
// I2S audio cannot be used on these pins while the display is active.
// Set AUDIO_ENABLED to 1 only if the hardware has been rewired with dedicated audio pins.
#define AUDIO_ENABLED 0
#define AUDIO_I2S_MCK_IO -1 // MCK
#define AUDIO_I2S_BCK_IO 18 // BCK (conflicts with TFT_SDA3)
#define AUDIO_I2S_WS_IO 16  // LCK (conflicts with TFT_SDA1)
#define AUDIO_I2S_DO_IO 17  // DIN (conflicts with TFT_SDA2)
#define AUDIO_MUTE_PIN 48   // 低电平静音

#define MIC_I2S_WS 45
#define MIC_I2S_SD 46
#define MIC_I2S_SCK 42

#endif