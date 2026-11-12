/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "support/legacy_panel.h"
int main(void)
{
    for (unsigned driver = 0; driver < 2; ++driver) {
        for (unsigned declared = 0; declared < 2; ++declared) {
            struct fake_lcd panel;
            init_panel(&panel);
            panel.psram_direct = driver;
            esp_display_presenter_config_t input = config(&panel, false);
            input.width = input.height = 480;
            input.target.hw.panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
            input.target.hw.io = &panel;
            input.target.hw.spi_psram_dma_direct = declared;
            input.target.hw.te_enabled = true;
            input.target.hw.te_sync = (esp_display_present_te_sync_config_t) {
                .gpio_num = 1, .bus_freq_hz = 40000000, .data_lines = 4
            };
            input.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC;
            present_resources_t *resources = NULL;
            esp_err_t error = present_resources_create(&input, false, NULL, NULL, &resources);
#ifdef PRESENT_HOST_SPI_NO_MEMORY_INFO
            bool supported = declared;
#else
            bool supported = driver;
#endif
            assert(error == (supported ? ESP_OK : ESP_ERR_NOT_SUPPORTED));
            assert(panel.calls == 0);
            if (resources) {
                OK(present_resources_destroy(resources, 1000));
            }
            free_panel(&panel);
        }
    }
    return 0;
}
