/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Reduced FreeType module list for LVGL runtime fonts.
 *
 * This file intentionally has no include guard because FreeType includes
 * FT_CONFIG_MODULES_H twice with different FT_USE_MODULE definitions.
 *
 * Keep TrueType and CFF/OpenType support together with their dependencies,
 * plus the anti-aliased renderer used by LVGL. Legacy drivers and extra
 * renderers are intentionally omitted.
 */
FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)
FT_USE_MODULE(FT_Driver_ClassRec, cff_driver_class)
FT_USE_MODULE(FT_Module_Class, psaux_module_class)
FT_USE_MODULE(FT_Module_Class, psnames_module_class)
FT_USE_MODULE(FT_Module_Class, pshinter_module_class)
FT_USE_MODULE(FT_Module_Class, sfnt_module_class)
FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)
