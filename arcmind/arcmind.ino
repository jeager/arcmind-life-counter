#include <WiFi.h>
#include "esp_bt.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "scr_st77916.h"
#include <lvgl.h>
#include "hal/lv_hal.h"
#include "knob.h"
#include "device_license.h"

#if SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_ENABLED
#include "USB.h"
#endif

static const int BATTERY_ADC_PIN = 1;
/* Waveshare JC3636K518: 200k/100k divider -> Vcell = Vpin * 3 */
static const float BATTERY_DIVIDER_RATIO = 3.0f;
static const float BATTERY_CALIBRATION_SCALE = 1.0f;
static const float BATTERY_CALIBRATION_OFFSET = 0.0f;
static float battery_voltage_filtered = 0.0f;
static bool battery_voltage_has_value = false;
static uint32_t battery_pin_millivolts_last = 0;

static float battery_voltage_from_pin_mv(uint32_t pin_mv)
{
  float from_mv = ((float)pin_mv * BATTERY_DIVIDER_RATIO) / 1000.0f;
  float from_mv_x2 = ((float)pin_mv * 2.0f) / 1000.0f;

  if (from_mv >= 3.0f && from_mv <= 4.35f) {
    return from_mv;
  }

  /* Some JC3636K518 clones match ESPHome configs that use multiply: 2.0 */
  if (from_mv_x2 >= 2.8f && from_mv_x2 <= 4.25f) {
    return 3.30f + ((from_mv_x2 - 2.80f) * (4.20f - 3.30f) / (4.20f - 2.80f));
  }

  return from_mv;
}

static float battery_voltage_from_raw(uint32_t raw_avg)
{
  return ((float)raw_avg * 3.3f * BATTERY_DIVIDER_RATIO) / 4095.0f;
}

extern "C" float knob_read_battery_voltage(void)
{
  uint32_t sum_raw = 0;
  uint32_t sum_mv = 0;
  uint32_t min_raw = UINT32_MAX;
  uint32_t max_raw = 0;
  uint32_t min_mv = UINT32_MAX;
  uint32_t max_mv = 0;
  const int sample_count = 16;
  float measured_voltage = 0.0f;
  uint32_t avg_raw = 0;
  uint32_t avg_mv = 0;

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  for (int i = 0; i < sample_count; i++) {
    uint32_t sample_raw = (uint32_t)analogRead(BATTERY_ADC_PIN);
    uint32_t sample_mv = (uint32_t)analogReadMilliVolts(BATTERY_ADC_PIN);
    sum_raw += sample_raw;
    sum_mv += sample_mv;
    if (sample_raw < min_raw) min_raw = sample_raw;
    if (sample_raw > max_raw) max_raw = sample_raw;
    if (sample_mv < min_mv) min_mv = sample_mv;
    if (sample_mv > max_mv) max_mv = sample_mv;
    delayMicroseconds(300);
  }

  sum_raw -= min_raw;
  sum_raw -= max_raw;
  sum_mv -= min_mv;
  sum_mv -= max_mv;
  avg_raw = sum_raw / (sample_count - 2);
  avg_mv = sum_mv / (sample_count - 2);
  battery_pin_millivolts_last = avg_mv;

  measured_voltage = battery_voltage_from_pin_mv(avg_mv);
  if ((avg_mv < 50 && avg_raw > 0) ||
      (measured_voltage < 2.8f && avg_raw > 0)) {
    float raw_voltage = battery_voltage_from_raw(avg_raw);
    if (raw_voltage > measured_voltage) {
      measured_voltage = raw_voltage;
    }
  }

  if (measured_voltage < 0.05f) {
    return 0.0f;
  }

  measured_voltage = (measured_voltage * BATTERY_CALIBRATION_SCALE) + BATTERY_CALIBRATION_OFFSET;

  if (!battery_voltage_has_value) {
    battery_voltage_filtered = measured_voltage;
    battery_voltage_has_value = true;
  } else {
    battery_voltage_filtered = (battery_voltage_filtered * 0.7f) + (measured_voltage * 0.3f);
  }

  return battery_voltage_filtered;
}

extern "C" uint32_t knob_battery_pin_millivolts(void)
{
  return battery_pin_millivolts_last;
}

extern "C" bool knob_usb_power_present(void);

static char serial_line[192];
static size_t serial_line_len = 0;

static void device_serial_poll(void)
{
  char response[192];
  bool restart_after;

  while (Serial.available() > 0) {
    int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r') continue;
    if (ch == '\n') {
      serial_line[serial_line_len] = '\0';
      restart_after = device_license_handle_command(serial_line, response, sizeof(response));
      if (response[0] != '\0') {
        Serial.println(response);
      }
      serial_line_len = 0;
      if (restart_after) {
        delay(100);
        esp_restart();
      }
      continue;
    }
    if (serial_line_len + 1U >= sizeof(serial_line)) {
      serial_line_len = 0;
      continue;
    }
    serial_line[serial_line_len++] = (char)ch;
  }
}

extern "C" bool knob_usb_power_present(void)
{
#if SOC_USB_OTG_SUPPORTED && CONFIG_TINYUSB_ENABLED
  if ((bool)USB) return true;
#endif
  return false;
}

void setup()
{
  // 160 MHz verbraucht weniger Batterie, 240 MHz reduziert hier aber Darstellungsfehler und Hänger.
  setCpuFrequencyMhz(240);

  // Funk deaktivieren
  WiFi.mode(WIFI_OFF);
  btStop();

  delay(200);
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

  scr_lvgl_init();
  knob_gui();

  // Keep RTC8M clock alive during light sleep so LEDC PWM (backlight) continues
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON);
  gpio_sleep_sel_dis((gpio_num_t)TFT_BLK);

  // Configure light sleep wakeup sources
  gpio_wakeup_enable((gpio_num_t)TOUCH_PIN_NUM_INT, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(1000000); // 1 second fallback for LVGL timers
}

void loop()
{
  device_serial_poll();
  knob_process_pending();
  knob_update_battery();
  uint32_t time_till_next = lv_timer_handler();

  if (knob_is_dimmed()) {
    // Set knob pin wakeup to opposite of current level so any rotation wakes us
    uint8_t level_a = gpio_get_level((gpio_num_t)ROTARY_ENC_PIN_A);
    uint8_t level_b = gpio_get_level((gpio_num_t)ROTARY_ENC_PIN_B);
    gpio_wakeup_enable((gpio_num_t)ROTARY_ENC_PIN_A, level_a ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    gpio_wakeup_enable((gpio_num_t)ROTARY_ENC_PIN_B, level_b ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    esp_light_sleep_start();
  } else {
    // Disable knob pin wakeup when active so they don't interfere
    gpio_wakeup_disable((gpio_num_t)ROTARY_ENC_PIN_A);
    gpio_wakeup_disable((gpio_num_t)ROTARY_ENC_PIN_B);
    vTaskDelay(pdMS_TO_TICKS(time_till_next));
  }
}
