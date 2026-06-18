#ifndef _DEVICE_LICENSE_H
#define _DEVICE_LICENSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define DEVICE_LICENSE_ID_LEN 36
#define DEVICE_LICENSE_KEY_HEX_LEN 64

bool device_license_is_provisioned(void);
bool device_license_get_id(char *out, size_t out_len);
bool device_license_get_mac(char *out, size_t out_len);
bool device_license_get_key_hex(char *out, size_t out_len);
bool device_license_provision(const char *device_id, const char *device_key_hex);
bool device_license_handle_command(const char *line, char *response, size_t response_len);

#ifdef __cplusplus
}
#endif

#endif
