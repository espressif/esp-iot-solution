# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""Add a per-display software-layer policy for object-style transforms.

This compatibility patch is for LVGL 9.5/9.6. It preserves the default matrix
path, including GPU backends, unless the application explicitly opts a display
into software object transforms. Screen matrix rotation remains independent.
The software bitmap path supports the rotation/scale styles of this test set;
this patch does not add skew or arbitrary custom-matrix software support.
Custom object matrices and skew retain the original matrix path: the bitmap
descriptor cannot preserve them, even when an orthogonal custom matrix would
otherwise be supported by the selected draw unit.
"""
import argparse
import os
from pathlib import Path
import re
import tempfile

SET_DECL = 'void lv_display_set_matrix_rotation(lv_display_t * disp, bool enable);'
POLICY_DECL = '''
/**
 * Render object-style rotation and scale through software bitmap layers.
 * Use this compatibility path with renderers that cannot apply general affine
 * matrices to draw tasks. Matrix-based screen rotation is not changed.
 * The default is false, preserving the matrix path for capable renderers.
 * The software layers can require additional memory and rendering time.
 * This does not add software support for skew or arbitrary custom matrices.
 * Set the policy before rendering, or invalidate the display after changing it.
 * @param disp      pointer to a display (NULL to use the default display)
 * @param enable    true: use software object layers; false: use the matrix path
 */
void lv_display_set_software_object_transforms(lv_display_t * disp, bool enable);

/**
 * Get whether object-style transforms use the software-layer compatibility path.
 * @param disp      pointer to a display (NULL to use the default display)
 * @return          true: software layers selected; false: matrix path selected
 */
bool lv_display_get_software_object_transforms(lv_display_t * disp);
'''

FIELD_ANCHOR = '''#if LV_USE_MEM_MONITOR
    lv_obj_t * mem_label;
#endif
'''
POLICY_FIELD = '\n    bool software_object_transforms; /**< Object-style transforms use software layers*/'

SOURCE_ANCHOR = 'void lv_display_set_theme(lv_display_t * disp, lv_theme_t * th)'
POLICY_IMPL = '''void lv_display_set_software_object_transforms(lv_display_t * disp, bool enable)
{
    if(disp == NULL) disp = lv_display_get_default();
    if(disp == NULL) return;
    disp->software_object_transforms = enable;
}

bool lv_display_get_software_object_transforms(lv_display_t * disp)
{
    if(disp == NULL) disp = lv_display_get_default();
    if(disp == NULL) return false;
    return disp->software_object_transforms;
}

'''

CORE_ANCHOR = '    else if(opa_layered >= LV_OPA_MAX && !refr_check_obj_clip_overflow(layer, obj)) {'
CORE_POLICY_V6 = '''    /* A display can keep screen matrix rotation while using the existing
     * bitmap-layer path for object-style rotation/scale. A task-level refusal
     * is too late: the software draw unit does not apply general matrices. */
    else if(!lv_display_get_software_object_transforms(disp_refr) &&
            opa_layered >= LV_OPA_MAX && !refr_check_obj_clip_overflow(layer, obj)) {'''
CORE_POLICY = '''    /* Keep screen matrix rotation while using bitmap layers for software
     * rotation/scale styles. Custom matrices and skew cannot be represented
     * by that fallback, so leave them on the renderer's original matrix path. */
    else if((!lv_display_get_software_object_transforms(disp_refr) ||
             lv_obj_get_transform(obj) != NULL ||
             lv_obj_get_style_transform_skew_x(obj, LV_PART_MAIN) != 0 ||
             lv_obj_get_style_transform_skew_y(obj, LV_PART_MAIN) != 0) &&
            opa_layered >= LV_OPA_MAX && !refr_check_obj_clip_overflow(layer, obj)) {'''


def insert_after(source, anchor, addition, label):
    if addition in source:
        if source.count(addition) != 1:
            raise ValueError(f'Duplicate {label}; refusing to modify LVGL')
        return source
    if 'software_object_transforms' in source:
        raise ValueError(f'Unrecognized or incomplete {label}; refusing to modify LVGL')
    if source.count(anchor) != 1:
        raise ValueError(f'Unrecognized LVGL {label} layout')
    return source.replace(anchor, anchor + '\n' + addition, 1)


def patch(lvgl_root):
    """Patch a component root, validating every supported layout before writes."""
    root = Path(lvgl_root)
    version_candidates = (root / 'include/lvgl/lv_version.h', root / 'lv_version.h')
    version_source = next((p.read_text() for p in version_candidates
                           if p.is_file() and '#define LVGL_VERSION_MAJOR' in p.read_text()), None)
    if version_source is None:
        raise ValueError('Cannot identify LVGL version; object transforms were not changed')
    version = []
    for part in ('MAJOR', 'MINOR'):
        found = re.search(r'^\s*#define\s+LVGL_VERSION_' + part + r'\s+(\d+)\b', version_source, re.M)
        if not found:
            raise ValueError('Cannot identify LVGL version; object transforms were not changed')
        version.append(int(found.group(1)))
    if tuple(version) not in ((9, 5), (9, 6)):
        raise ValueError(f'Unsupported LVGL {version[0]}.{version[1]}; expected 9.5 or 9.6')

    header_candidates = (root / 'include/lvgl/display/lv_display.h', root / 'src/display/lv_display.h')
    headers = [p for p in header_candidates if p.is_file() and SET_DECL in p.read_text()]
    if not headers:
        raise ValueError('Unrecognized LVGL public display header layout')
    edits = {}
    for header in headers:
        source = header.read_text()
        edits[header] = insert_after(source, SET_DECL, POLICY_DECL, 'object-transform declarations')

    private = root / 'src/display/lv_display_private.h'
    source = private.read_text()
    edits[private] = insert_after(source, FIELD_ANCHOR, POLICY_FIELD, 'display policy field')

    display = root / 'src/display/lv_display.c'
    source = display.read_text()
    if POLICY_IMPL in source:
        if source.count(POLICY_IMPL) != 1:
            raise ValueError('Duplicate object-transform policy implementation')
        edits[display] = source
    else:
        if 'software_object_transforms' in source or source.count(SOURCE_ANCHOR) != 1:
            raise ValueError('Unrecognized LVGL display policy implementation layout')
        edits[display] = source.replace(SOURCE_ANCHOR, POLICY_IMPL + SOURCE_ANCHOR, 1)

    core = root / 'src/core/lv_refr.c'
    source = core.read_text()
    if CORE_POLICY in source:
        if source.count(CORE_POLICY) != 1:
            raise ValueError('Duplicate object-transform refresh policy')
        edits[core] = source
    elif CORE_POLICY_V6 in source:
        # Upgrade the test-kit policy without duplicating declarations or
        # fields in a managed component which already contains that version.
        if source.count(CORE_POLICY_V6) != 1:
            raise ValueError('Duplicate V6 object-transform refresh policy')
        edits[core] = source.replace(CORE_POLICY_V6, CORE_POLICY, 1)
    else:
        if 'software_object_transforms' in source or source.count(CORE_ANCHOR) != 1:
            raise ValueError('Unrecognized LVGL object-matrix refresh layout')
        edits[core] = source.replace(CORE_ANCHOR, CORE_POLICY, 1)

    # LVGL zeroes a new display, so the policy starts false without changing
    # behavior or requiring an additional allocation.
    create = re.search(r'lv_display_t \* lv_display_create\([^\n]+\)\n\{.*?\n\}',
                       display.read_text(), re.S)
    if not create or create.group(0).count('    lv_memzero(disp, sizeof(lv_display_t));') != 1:
        raise ValueError('Unrecognized LVGL display allocation; cannot guarantee the default policy')

    changed = [(path, text) for path, text in edits.items() if path.read_text() != text]
    # Managed components can be hard-linked to the component cache. Replace
    # directory entries instead of writing through those shared hard links.
    for path, text in changed:
        mode = path.stat().st_mode & 0o777
        with tempfile.NamedTemporaryFile('w', dir=path.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write(text)
        try:
            temporary.chmod(mode)
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)
    return 'patched' if changed else 'already patched'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('lvgl_root', type=Path, help='LVGL component source directory')
    args = parser.parse_args()
    print(patch(args.lvgl_root))
