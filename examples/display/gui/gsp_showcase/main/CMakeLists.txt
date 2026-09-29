# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "app_main.c" "showcase_ui.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp-gsp esp_lcd esp_timer hw_init
    EMBED_FILES "runtime_badge.png"
)

if(NOT CONFIG_EXAMPLE_DISPLAY_ROTATION_0)
    message(FATAL_ERROR "gsp_showcase currently supports display rotation 0 only")
endif()

if(CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI)
    set(showcase_scene_dir "../scenes")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_RGB)
    set(showcase_scene_dir "../scenes/800")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_QSPI)
    set(showcase_scene_dir "../scenes/360")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM)
    set(showcase_scene_dir "../scenes/320")
elseif(CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM)
    set(showcase_scene_dir "../scenes/240")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SHOWCASE_KNOB_UI=1)
    # Runtime PNG replacement needs decoded-image storage on no-PSRAM targets.
    set(showcase_bundle_options IMAGE_CACHE_BYTES 65536)
else()
    message(FATAL_ERROR "gsp_showcase requires a common/hw_init LCD interface")
endif()

gsp_add_bundle(${COMPONENT_LIB}
    SCENES
        ${showcase_scene_dir}/controls.json
        ${showcase_scene_dir}/media.json
        ${showcase_scene_dir}/motion.json
        ${showcase_scene_dir}/data.json
    PIXEL_FORMAT rgb565
    ${showcase_bundle_options})
