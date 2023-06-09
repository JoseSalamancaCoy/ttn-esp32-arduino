/*******************************************************************************
 *
 * ttn-esp32 - The Things Network device library for ESP-IDF / SX127x
 *
 * Copyright (c) 2018-2021 Manuel Bleichenbacher
 *
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 *
 * Functions for storing and retrieving TTN communication state from NVS.
 *******************************************************************************/
#include "esp_log.h"
#include "esp_system.h"
#include "hal/hal_esp32.h"
#include "lmic/lmic.h"
#include "nvs_flash.h"
#include "ttn_rtc.h"
#include <string.h>

#define LMIC_OFFSET(field) __builtin_offsetof(struct lmic_t, field)
#define LMIC_DIST(field1, field2) (LMIC_OFFSET(field2) - LMIC_OFFSET(field1))

#define TAG "ttn_nvs"
#define NVS_FLASH_PARTITION "ttn"
#define NVS_FLASH_KEY_CHUNK_1 "chunk1"
#define NVS_FLASH_KEY_CHUNK_2 "chunk2"
#define NVS_FLASH_KEY_CHUNK_3 "chunk3"
#define NVS_FLASH_KEY_TIME "time"

void ttn_nvs_save()
{
    size_t len1, len2,len3;
    nvs_handle handle = 0;
    esp_err_t res = nvs_open(NVS_FLASH_PARTITION, NVS_READWRITE, &handle);
    if (res == ESP_ERR_NVS_NOT_INITIALIZED)
        ESP_LOGW(TAG, "NVS storage is not initialized. Call 'nvs_flash_init()' first.");
    if (res != ESP_OK)
        goto done;

    // Save LMIC struct except client, osjob, pendTxData and frame
     len1 = LMIC_DIST(radio, pendTxData);
    res = nvs_set_blob(handle, NVS_FLASH_KEY_CHUNK_1, &LMIC.radio, len1);
    if (res != ESP_OK)
        goto done;

     len2 = LMIC_DIST(pendTxData, frame) - MAX_LEN_PAYLOAD;
    res = nvs_set_blob(handle, NVS_FLASH_KEY_CHUNK_2, (u1_t *)&LMIC.pendTxData + MAX_LEN_PAYLOAD, len2);
    if (res != ESP_OK)
        goto done;

     len3 = sizeof(struct lmic_t) - LMIC_OFFSET(frame) - MAX_LEN_FRAME;
    res = nvs_set_blob(handle, NVS_FLASH_KEY_CHUNK_3, (u1_t *)&LMIC.frame + MAX_LEN_FRAME, len3);
    if (res != ESP_OK)
        goto done;

    res = nvs_set_u32(handle, NVS_FLASH_KEY_TIME, hal_esp32_get_time());
    if (res != ESP_OK)
        goto done;

    res = nvs_commit(handle);
    if (res != ESP_OK)
        goto done;

    done:
    nvs_close(handle);

    ESP_ERROR_CHECK(res);
}

bool ttn_nvs_restore(int off_duration)
{
    uint32_t time_val;
    size_t len1, len2, len3;
    nvs_handle handle = 0;
    esp_err_t res = nvs_open(NVS_FLASH_PARTITION, NVS_READWRITE, &handle);
    if (res == ESP_ERR_NVS_NOT_INITIALIZED)
        ESP_LOGW(TAG, "NVS storage is not initialized. Call 'nvs_flash_init()' first.");
    if (res != ESP_OK)
        goto done;

    res = nvs_get_u32(handle, NVS_FLASH_KEY_TIME, &time_val);
    if (res != ESP_OK)
        goto done;

    len1 = LMIC_DIST(radio, pendTxData);
    res = nvs_get_blob(handle, NVS_FLASH_KEY_CHUNK_1, &LMIC.radio, &len1);
    if (res != ESP_OK)
        goto done;

    memset(LMIC.pendTxData, 0, MAX_LEN_PAYLOAD);

    len2 = LMIC_DIST(pendTxData, frame) - MAX_LEN_PAYLOAD;
    res = nvs_get_blob(handle, NVS_FLASH_KEY_CHUNK_2, (u1_t *)&LMIC.pendTxData + MAX_LEN_PAYLOAD, &len2);
    if (res != ESP_OK)
        goto done;

    memset(LMIC.frame, 0, MAX_LEN_FRAME);

    len3 = sizeof(struct lmic_t) - LMIC_OFFSET(frame) - MAX_LEN_FRAME;
    res = nvs_get_blob(handle, NVS_FLASH_KEY_CHUNK_3, (u1_t *)&LMIC.frame + MAX_LEN_FRAME, &len3);
    if (res != ESP_OK)
        goto done;

    // invalidate data
    res = nvs_erase_key(handle, NVS_FLASH_KEY_TIME);
    if (res != ESP_OK)
        goto done;

    res = nvs_commit(handle);
    if (res != ESP_OK)
        goto done;

    if (off_duration != 0)
        hal_esp32_set_time(time_val + off_duration * 60);

done:
    nvs_close(handle);

    if (res != ESP_OK)
        memset(&LMIC.radio, 0, sizeof(LMIC) - LMIC_OFFSET(radio));

    return res == ESP_OK;
}
