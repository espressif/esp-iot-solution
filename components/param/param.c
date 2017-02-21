#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "param.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

static const char* TAG = "param";

esp_err_t param_save(uint16_t start_sec, void *param, uint16_t len)
{
    uint32_t flag;
    POINT_ASSERT(TAG, param);
    ERR_ASSERT(TAG, spi_flash_read((start_sec + 2) * SPI_FLASH_SEC_SIZE, &flag, sizeof(flag)) );
    if (flag == 0) {
        ERR_ASSERT(TAG, spi_flash_erase_sector(start_sec + 1) );
        ERR_ASSERT(TAG, spi_flash_write((start_sec + 1) * SPI_FLASH_SEC_SIZE, param, len) );
        flag = 1;
        ERR_ASSERT(TAG, spi_flash_erase_sector(start_sec + 2) );
        ERR_ASSERT(TAG, spi_flash_write((start_sec + 2) * SPI_FLASH_SEC_SIZE, &flag, sizeof(flag)) );
        spi_flash_read((start_sec + 2) * SPI_FLASH_SEC_SIZE, &flag, sizeof(flag));
    }
    else {
        ERR_ASSERT(TAG, spi_flash_erase_sector(start_sec+ 0) );
        ERR_ASSERT(TAG, spi_flash_write((start_sec + 0) * SPI_FLASH_SEC_SIZE, param, len) );
        flag = 0;
        ERR_ASSERT(TAG, spi_flash_erase_sector(start_sec + 2) );
        ERR_ASSERT(TAG, spi_flash_write((start_sec + 2) * SPI_FLASH_SEC_SIZE, &flag, sizeof(flag)) );
        spi_flash_read((start_sec + 2) * SPI_FLASH_SEC_SIZE, &flag, sizeof(flag));
    }
    return ESP_OK;
}

esp_err_t param_load(uint16_t start_sec, void *dest, uint16_t len)
{
    uint32_t flag;
    POINT_ASSERT(TAG, dest);
    ERR_ASSERT(TAG, spi_flash_read((start_sec + 2) * SPI_FLASH_SEC_SIZE, &flag, sizeof(flag)) );
    if (flag == 0) {
        ERR_ASSERT(TAG, spi_flash_read((start_sec + 0) * SPI_FLASH_SEC_SIZE, dest, len) );
    }
    else {
        ERR_ASSERT(TAG, spi_flash_read((start_sec + 1) * SPI_FLASH_SEC_SIZE, dest, len) );
    }
    return ESP_OK;    
}