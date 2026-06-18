#include "device_license.h"

#include "esp_mac.h"
#include "esp_system.h"
#include "mbedtls/md.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define LICENSE_NVS_NAMESPACE "arcmind_lic"
#define LICENSE_NVS_ID_KEY "device_id"
#define LICENSE_NVS_KEY_KEY "device_key"

static bool hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    size_t hex_len;
    size_t i;

    if (hex == NULL || out == NULL) return false;
    hex_len = strlen(hex);
    if (hex_len != out_len * 2U) return false;

    for (i = 0; i < out_len; i++) {
        char hi = hex[i * 2U];
        char lo = hex[i * 2U + 1U];
        int hi_val;
        int lo_val;

        if (!isxdigit((unsigned char)hi) || !isxdigit((unsigned char)lo)) return false;
        hi_val = (hi >= 'a') ? (hi - 'a' + 10) : ((hi >= 'A') ? (hi - 'A' + 10) : (hi - '0'));
        lo_val = (lo >= 'a') ? (lo - 'a' + 10) : ((lo >= 'A') ? (lo - 'A' + 10) : (lo - '0'));
        out[i] = (uint8_t)((hi_val << 4) | lo_val);
    }

    return true;
}

static bool bytes_to_hex(const uint8_t *bytes, size_t byte_len, char *hex_out, size_t hex_out_len)
{
    size_t i;

    if (bytes == NULL || hex_out == NULL || hex_out_len < (byte_len * 2U + 1U)) return false;
    for (i = 0; i < byte_len; i++) {
        snprintf(hex_out + (i * 2U), 3, "%02x", bytes[i]);
    }
    hex_out[byte_len * 2U] = '\0';
    return true;
}

static bool license_open(nvs_open_mode_t mode, nvs_handle_t *handle_out)
{
    if (handle_out == NULL) return false;
    return nvs_open(LICENSE_NVS_NAMESPACE, mode, handle_out) == ESP_OK;
}

bool device_license_is_provisioned(void)
{
    nvs_handle_t handle;
    char id_buf[DEVICE_LICENSE_ID_LEN + 1];
    char key_buf[DEVICE_LICENSE_KEY_HEX_LEN + 1];
    size_t id_len = sizeof(id_buf);
    size_t key_len = sizeof(key_buf);

    if (!license_open(NVS_READONLY, &handle)) return false;

    if (nvs_get_str(handle, LICENSE_NVS_ID_KEY, id_buf, &id_len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    key_len = sizeof(key_buf);
    if (nvs_get_str(handle, LICENSE_NVS_KEY_KEY, key_buf, &key_len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_close(handle);

    return id_buf[0] != '\0' && key_buf[0] != '\0';
}

bool device_license_get_id(char *out, size_t out_len)
{
    nvs_handle_t handle;
    size_t len;

    if (out == NULL || out_len == 0) return false;
    out[0] = '\0';
    if (!license_open(NVS_READONLY, &handle)) return false;

    len = out_len;
    if (nvs_get_str(handle, LICENSE_NVS_ID_KEY, out, &len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_close(handle);
    return out[0] != '\0';
}

bool device_license_get_mac(char *out, size_t out_len)
{
    uint8_t mac[6];

    if (out == NULL || out_len < 18) return false;
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return false;
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

bool device_license_get_key_hex(char *out, size_t out_len)
{
    nvs_handle_t handle;
    size_t len;

    if (out == NULL || out_len == 0) return false;
    out[0] = '\0';
    if (!device_license_is_provisioned()) return false;
    if (!license_open(NVS_READONLY, &handle)) return false;

    len = out_len;
    if (nvs_get_str(handle, LICENSE_NVS_KEY_KEY, out, &len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_close(handle);
    return out[0] != '\0';
}

bool device_license_provision(const char *device_id, const char *device_key_hex)
{
    nvs_handle_t handle;
    size_t id_len;
    size_t key_len;

    if (device_id == NULL || device_key_hex == NULL) return false;
    if (device_license_is_provisioned()) return false;

    id_len = strlen(device_id);
    key_len = strlen(device_key_hex);
    if (id_len == 0 || id_len > DEVICE_LICENSE_ID_LEN) return false;
    if (key_len != DEVICE_LICENSE_KEY_HEX_LEN) return false;

    {
        uint8_t key_bytes[32];
        if (!hex_to_bytes(device_key_hex, key_bytes, sizeof(key_bytes))) return false;
    }

    if (!license_open(NVS_READWRITE, &handle)) return false;
    if (nvs_set_str(handle, LICENSE_NVS_ID_KEY, device_id) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    if (nvs_set_str(handle, LICENSE_NVS_KEY_KEY, device_key_hex) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    if (nvs_commit(handle) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_close(handle);
    return true;
}

bool device_license_sign_challenge(const char *challenge_hex, char *sig_hex_out, size_t sig_hex_out_len)
{
    char key_hex[DEVICE_LICENSE_KEY_HEX_LEN + 1];
    uint8_t key_bytes[32];
    uint8_t challenge_bytes[32];
    uint8_t sig_bytes[32];
    const mbedtls_md_info_t *md_info;
    size_t challenge_hex_len;

    if (challenge_hex == NULL || sig_hex_out == NULL) return false;
    challenge_hex_len = strlen(challenge_hex);
    if (challenge_hex_len == 0 || (challenge_hex_len % 2U) != 0U) return false;
    if (challenge_hex_len / 2U > sizeof(challenge_bytes)) return false;
    if (!device_license_get_key_hex(key_hex, sizeof(key_hex))) return false;
    if (!hex_to_bytes(key_hex, key_bytes, sizeof(key_bytes))) return false;
    if (!hex_to_bytes(challenge_hex, challenge_bytes, challenge_hex_len / 2U)) return false;

    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) return false;
    if (mbedtls_md_hmac(md_info, key_bytes, sizeof(key_bytes),
                        challenge_bytes, challenge_hex_len / 2U,
                        sig_bytes) != 0) {
        return false;
    }

    return bytes_to_hex(sig_bytes, sizeof(sig_bytes), sig_hex_out, sig_hex_out_len);
}

static void reply_info(char *response, size_t response_len)
{
    char mac[18];
    char id[DEVICE_LICENSE_ID_LEN + 1];

    if (!device_license_get_mac(mac, sizeof(mac))) {
        snprintf(response, response_len, "ARC_ERR mac_unavailable");
        return;
    }

    id[0] = '\0';
    (void)device_license_get_id(id, sizeof(id));
    snprintf(response, response_len, "ARC_INFO mac=%s id=%s provisioned=%s",
             mac, id, device_license_is_provisioned() ? "yes" : "no");
}

static void reply_key(char *response, size_t response_len)
{
    char key_hex[DEVICE_LICENSE_KEY_HEX_LEN + 1];

    if (!device_license_get_key_hex(key_hex, sizeof(key_hex))) {
        snprintf(response, response_len, "ARC_ERR not_provisioned");
        return;
    }
    snprintf(response, response_len, "ARC_KEY key=%s", key_hex);
}

static void reply_sign(const char *challenge_hex, char *response, size_t response_len)
{
    char sig_hex[65];

    if (challenge_hex == NULL || challenge_hex[0] == '\0') {
        snprintf(response, response_len, "ARC_ERR invalid_challenge");
        return;
    }
    if (!device_license_sign_challenge(challenge_hex, sig_hex, sizeof(sig_hex))) {
        snprintf(response, response_len, "ARC_ERR sign_failed");
        return;
    }
    snprintf(response, response_len, "ARC_SIG sig=%s", sig_hex);
}

static bool handle_provision(const char *args, char *response, size_t response_len)
{
    char device_id[DEVICE_LICENSE_ID_LEN + 1];
    char device_key[DEVICE_LICENSE_KEY_HEX_LEN + 1];
    const char *id_prefix = "id=";
    const char *key_prefix = "key=";
    const char *id_start;
    const char *key_start;
    const char *space;
    size_t id_len;
    size_t key_len;

    if (args == NULL) {
        snprintf(response, response_len, "ARC_ERR invalid_provision");
        return false;
    }

    id_start = strstr(args, id_prefix);
    key_start = strstr(args, key_prefix);
    if (id_start == NULL || key_start == NULL) {
        snprintf(response, response_len, "ARC_ERR invalid_provision");
        return false;
    }

    id_start += strlen(id_prefix);
    space = strchr(id_start, ' ');
    if (space == NULL || space <= id_start) {
        snprintf(response, response_len, "ARC_ERR invalid_provision");
        return false;
    }
    id_len = (size_t)(space - id_start);
    if (id_len == 0 || id_len > DEVICE_LICENSE_ID_LEN) {
        snprintf(response, response_len, "ARC_ERR invalid_provision");
        return false;
    }
    memcpy(device_id, id_start, id_len);
    device_id[id_len] = '\0';

    key_start += strlen(key_prefix);
    key_len = strlen(key_start);
    if (key_len != DEVICE_LICENSE_KEY_HEX_LEN) {
        snprintf(response, response_len, "ARC_ERR invalid_provision");
        return false;
    }
    memcpy(device_key, key_start, key_len);
    device_key[key_len] = '\0';

    if (!device_license_provision(device_id, device_key)) {
        snprintf(response, response_len, "ARC_ERR provision_failed");
        return false;
    }

    snprintf(response, response_len, "ARC_PROVISION_OK");
    return true;
}

bool device_license_handle_command(const char *line, char *response, size_t response_len)
{
    if (line == NULL || response == NULL || response_len == 0) return false;
    response[0] = '\0';

    if (strcmp(line, "ARC_PING") == 0) {
        snprintf(response, response_len, "ARC_PONG");
        return false;
    }
    if (strcmp(line, "ARC_GET_INFO") == 0) {
        reply_info(response, response_len);
        return false;
    }
    if (strcmp(line, "ARC_GET_KEY") == 0) {
        reply_key(response, response_len);
        return false;
    }
    if (strncmp(line, "ARC_SIGN ", 9) == 0) {
        reply_sign(line + 9, response, response_len);
        return false;
    }
    if (strncmp(line, "ARC_PROVISION ", 14) == 0) {
        return handle_provision(line + 14, response, response_len);
    }

    return false;
}
