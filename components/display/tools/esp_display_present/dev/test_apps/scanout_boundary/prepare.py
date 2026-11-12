# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Create isolated, fail-closed instrumented IDF/V2 components. Never edits inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def patch(path, old, new, expected=1):
    source = path.read_text()
    count = source.count(old)
    if count != expected:
        raise RuntimeError(f'{path}: expected {expected} anchors, found {count}: {old[:90]!r}')
    path.write_text(source.replace(old, new))


def prepare(template, idf, output):
    component = template.parents[2]
    boards = template.parent / 'boards'
    if output.exists():
        raise RuntimeError(f'Output already exists: {output}; use a new directory')
    output.mkdir(parents=True)
    for name in ('CMakeLists.txt', 'main'):
        source = template / name
        if source.is_dir():
            shutil.copytree(source, output / name)
        else:
            shutil.copy2(source, output / name)
    for name in ('board.h', 'board_s3.c', 'board_p4.c'):
        shutil.copy2(boards / 'main' / name, output / 'main' / name)
    for name in ('sdkconfig.defaults', 'sdkconfig.defaults.esp32s3', 'sdkconfig.defaults.esp32p4'):
        shutil.copy2(boards / name, output / name)
    cmake = output / 'CMakeLists.txt'
    cmake.write_text(cmake.read_text().replace('${BOUNDARY_BOARD_DEPS}', str(boards / 'deps')))
    overlay = output / 'overlay'
    shutil.copytree(idf / 'components/esp_lcd', overlay / 'esp_lcd', ignore=shutil.ignore_patterns('test_apps', '__pycache__'))
    v2 = overlay / 'esp_display_present'
    v2.mkdir()
    for name in ('src', 'include'):
        shutil.copytree(component / name, v2 / name)
    shutil.copy2(component / 'CMakeLists.txt', v2 / 'CMakeLists.txt')
    if (component / 'compat').exists():
        shutil.copytree(component / 'compat', v2 / 'compat')
    # Hooks are linked only in this generated test project.
    for directory in (overlay / 'esp_lcd/include', v2 / 'include'):
        shutil.copy2(template / 'main/boundary_trace.h', directory / 'boundary_trace.h')
    rgb = overlay / 'esp_lcd/rgb/esp_lcd_panel_rgb.c'
    dpi = overlay / 'esp_lcd/dsi/esp_lcd_panel_dpi.c'
    fb = v2 / 'src/backend/present_backend_fb.c'
    adapter = v2 / 'src/platform/lcd/present_lcd_fb_scanout.c'
    originals = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (rgb, dpi, fb, adapter)}
    for path in (rgb, dpi, fb, adapter):
        source = path.read_text()
        path.write_text('#include "boundary_trace.h"\n' + source)
    patch(rgb, '        memcpy(buffer, &panel->fbs[panel->bb_fb_index][panel->bounce_pos_px * bytes_per_pixel], panel->bb_size);',
          '        if (panel->bounce_pos_px == 0) boundary_emit(BT_READ_BEGIN, (uintptr_t)panel->fbs[panel->bb_fb_index], 0);\n'
          '        memcpy(buffer, &panel->fbs[panel->bb_fb_index][panel->bounce_pos_px * bytes_per_pixel], panel->bb_size);\n'
          '        if (panel->bounce_pos_px + panel->bb_size / bytes_per_pixel >= panel->fb_size / bytes_per_pixel)\n'
          '            boundary_emit(BT_READ_END, (uintptr_t)panel->fbs[panel->bb_fb_index], panel->bounce_pos_px);')
    patch(rgb, '        panel->bb_fb_index = panel->cur_fb_index;',
          '        boundary_emit(BT_FRAME_DONE, (uintptr_t)panel->fbs[panel->bb_fb_index], 0);\n'
          '        panel->bb_fb_index = panel->cur_fb_index;\n'
          '        boundary_emit(BT_SELECT, (uintptr_t)panel->fbs[panel->bb_fb_index], 0);')
    patch(rgb, '        int bb = rgb_panel->bb_eof_count % RGB_LCD_PANEL_BOUNCE_BUF_NUM;',
          '        boundary_emit(BT_BOUNCE_EOF, 0, rgb_panel->bb_eof_count);\n'
          '        int bb = rgb_panel->bb_eof_count % RGB_LCD_PANEL_BOUNCE_BUF_NUM;')
    patch(rgb, '    if (intr_status & LCD_LL_EVENT_VSYNC_END) {',
          '    if (intr_status & LCD_LL_EVENT_VSYNC_END) {\n'
          '        boundary_emit(BT_VSYNC, 0, rgb_panel->bb_eof_count);')
    patch(rgb, '    bool do_restart = false;', '    bool do_restart = false;\n    uint32_t boundary_restart_reason = 0;')
    patch(rgb, '#if CONFIG_LCD_RGB_RESTART_IN_VSYNC\n    do_restart = true;',
          '#if CONFIG_LCD_RGB_RESTART_IN_VSYNC\n    boundary_restart_reason |= 1;\n    do_restart = true;')
    patch(rgb, '    if (panel->flags.need_restart) {',
          '    if (panel->flags.need_restart) {\n        boundary_restart_reason |= 2;')
    patch(rgb, '    if (panel->bb_eof_count < panel->expect_eof_count) {',
          '    if (panel->bb_eof_count < panel->expect_eof_count) {\n        boundary_restart_reason |= 4;')
    patch(rgb, '    if (!do_restart) {\n        return;\n    }',
          '    if (!do_restart) {\n        return;\n    }\n'
          '    boundary_emit(BT_RESTART, (uintptr_t)panel->fbs[panel->bb_size ? panel->bb_fb_index : panel->cur_fb_index], boundary_restart_reason);')
    patch(rgb, '        ESP_EARLY_LOGE(TAG, "LCD underrun");',
          '        boundary_emit(BT_UNDERRUN, 0, 0);\n        ESP_EARLY_LOGE(TAG, "LCD underrun");')
    patch(rgb, '    (void)dma_chan;\n    (void)event_data;',
          '    boundary_emit(BT_LINK_SWITCH, 0, 0);\n    (void)dma_chan;\n    (void)event_data;')
    patch(rgb, '            gdma_link_concat(rgb_panel->dma_fb_links[i], -1, rgb_panel->dma_fb_links[rgb_panel->cur_fb_index], 0);',
          '            boundary_emit(BT_CHAIN, (uintptr_t)rgb_panel->fbs[i], rgb_panel->cur_fb_index);\n'
          '            gdma_link_concat(rgb_panel->dma_fb_links[i], -1, rgb_panel->dma_fb_links[rgb_panel->cur_fb_index], 0);')
    # Record topology: bit31 = bounce, bit30 = continuous; low bits = node count.
    patch(rgb, '        // mount bounce buffers to the DMA link list\n        gdma_buffer_mount_config_t',
          '        boundary_emit(BT_CHAIN, 0, 0x80000000u | (rgb_panel->flags.stream_mode ? 0x40000000u : 0) | link_cfg.num_items);\n'
          '        // mount bounce buffers to the DMA link list\n        gdma_buffer_mount_config_t')
    patch(rgb, '        gdma_buffer_mount_config_t mount_cfg = {',
          '        boundary_emit(BT_CHAIN, 0, (rgb_panel->flags.stream_mode ? 0x40000000u : 0) | num_dma_nodes);\n'
          '        gdma_buffer_mount_config_t mount_cfg = {')
    # On-demand/non-bounce EOF observation. No safety interpretation is added.
    patch(rgb, '        // Once the preload has already done, the buffer complete callback is not reliable.',
          '        boundary_emit(BT_FRAME_DONE, (uintptr_t)rgb_panel->fbs[rgb_panel->cur_fb_index], 1);\n'
          '        // Once the preload has already done, the buffer complete callback is not reliable.')
    patch(dpi, '    uint8_t cur_fb_index;', '    uint8_t boundary_active_fb;\n    uint8_t cur_fb_index;')
    patch(dpi, '    uint8_t fb_index = dpi_panel->cur_fb_index;',
          '    boundary_emit(BT_READ_END, (uintptr_t)dpi_panel->fbs[dpi_panel->boundary_active_fb], 0);\n'
          '    uint8_t fb_index = dpi_panel->cur_fb_index;\n'
          '    dpi_panel->boundary_active_fb = fb_index;\n'
          '    boundary_emit(BT_SELECT, (uintptr_t)dpi_panel->fbs[fb_index], 0);\n'
          '    boundary_emit(BT_READ_BEGIN, (uintptr_t)dpi_panel->fbs[fb_index], 0);')
    patch(dpi, '    dpi_panel->cur_fb_index = 0;',
          '    boundary_emit(BT_CHAIN, (uintptr_t)dpi_panel->fbs[0], 1);\n'
          '    boundary_emit(BT_SELECT, (uintptr_t)dpi_panel->fbs[0], 0);\n'
          '    boundary_emit(BT_READ_BEGIN, (uintptr_t)dpi_panel->fbs[0], 0);\n'
          '    dpi_panel->cur_fb_index = 0;')
    patch(dpi, '    if (dpi_panel->on_frame_buf_complete) {',
          '    boundary_emit(BT_FRAME_DONE, (uintptr_t)dpi_panel->fbs[fb_index], 0);\n'
          '    if (dpi_panel->on_frame_buf_complete) {')
    patch(dpi, '    if (intr_status & MIPI_DSI_BRG_LL_EVENT_VSYNC) {',
          '    if (intr_status & MIPI_DSI_BRG_LL_EVENT_VSYNC) {\n        boundary_emit(BT_VSYNC, 0, 0);')
    patch(dpi, '        ESP_DRAM_LOGE(TAG, "can\'t fetch data from external memory fast enough, underrun happens");',
          '        boundary_emit(BT_UNDERRUN, 0, 0);\n'
          '        ESP_DRAM_LOGE(TAG, "can\'t fetch data from external memory fast enough, underrun happens");')
    patch(adapter, '.on_vsync = scanout->config.on_vsync ? dsi_vsync : NULL', '.on_vsync = dsi_vsync')
    patch(dpi, '#if !MIPI_DSI_BRG_LL_EVENT_VSYNC\n',
          '#if !MIPI_DSI_BRG_LL_EVENT_VSYNC\n    boundary_emit(BT_VSYNC, 0, 1);\n')
    patch(adapter, '    error = esp_lcd_panel_draw_bitmap(scanout->config.panel, 0, 0, area->width, area->height, surface->pixels);',
          '    boundary_emit(BT_SUBMIT, (uintptr_t)surface->pixels, (uint32_t)cookie);\n'
          '    error = esp_lcd_panel_draw_bitmap(scanout->config.panel, 0, 0, area->width, area->height, surface->pixels);')
    patch(adapter, '    unsigned needed = scanout->required_boundaries;',
          '    boundary_emit(BT_CALLBACK, 0, scanout->armed);\n'
          '    unsigned needed = scanout->required_boundaries;')
    patch(adapter, '    scanout->armed = error == ESP_OK;',
          '    scanout->armed = error == ESP_OK;\n'
          '    boundary_emit(BT_ARM, (uintptr_t)surface->pixels, (uint32_t)cookie);')
    patch(adapter, '    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED, .transaction_cookie = cookie};',
          '    boundary_emit(BT_CONFIRM, 0, (uint32_t)cookie);\n'
          '    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED, .transaction_cookie = cookie};')
    patch(fb, '            backend->slots[backend->scanning].state = FB_SLOT_FREE;',
          '            boundary_emit(BT_RELEASE, (uintptr_t)(backend->scan_storage ? backend->slots[backend->scanning].scan.pixels : backend->slots[backend->scanning].surface.pixels), 0);\n'
          '            backend->slots[backend->scanning].state = FB_SLOT_FREE;')
    patch(fb, '    if (prefill && !backend->config.async_copy) {',
          '    boundary_emit(BT_WRITE_BEGIN, (uintptr_t)(backend->scan_storage ? backend->slots[slot].scan.pixels : backend->slots[slot].surface.pixels), 0);\n'
          '    if (prefill && !backend->config.async_copy) {')
    patch(fb, '        error = present_image_commit(&backend->image);',
          '        boundary_emit(BT_WRITE_END, (uintptr_t)(backend->scan_storage ? slot->scan.pixels : slot->surface.pixels), 0);\n'
          '        error = present_image_commit(&backend->image);')
    board = output / 'main/board_s3.c'
    patch(board, '.num_fbs = buffer_count, .bounce_buffer_size_px = WIDTH * 10,',
          '.num_fbs = buffer_count,\n'
          '#if CONFIG_BOUNDARY_NO_BOUNCE\n        .bounce_buffer_size_px = 0,\n#else\n        .bounce_buffer_size_px = WIDTH * 10,\n#endif\n'
          '#if CONFIG_BOUNDARY_ON_DEMAND\n        .flags.refresh_on_demand = true,\n#endif')
    manifest = {'idf': str(idf), 'component': str(component), 'original_sha256': originals,
                'instrumented_sha256': {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (rgb, dpi, fb, adapter)}}
    (output / 'instrumentation_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idf', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(Path(__file__).resolve().parent, args.idf.resolve(), args.output.resolve())
