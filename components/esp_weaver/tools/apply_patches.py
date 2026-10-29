#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
ESP-Weaver: Patch Application Script

This script applies patches to ESP-IDF components.
Called automatically by CMake during project configuration.

Usage:
    python apply_patches.py --patches-dir <path> --idf-path <path>
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

# Initialize colorama on Windows for proper ANSI code translation
try:
    import colorama
    if os.name == 'nt':
        colorama.init()
except ImportError:
    pass  # colorama not installed, ANSI codes may not render on older Windows terminals


class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    RESET = '\033[0m'


def log_info(msg):
    print(f'{Colors.CYAN}[ESP-Weaver]{Colors.RESET} {msg}')


def log_success(msg):
    print(f'{Colors.GREEN}[ESP-Weaver]{Colors.RESET} ✓ {msg}')


def log_warning(msg):
    print(f'{Colors.YELLOW}[ESP-Weaver]{Colors.RESET} ⚠ {msg}')


def log_error(msg):
    print(f'{Colors.RED}[ESP-Weaver]{Colors.RESET} ✗ {msg}')


def run_cmd(cmd, cwd, timeout=60):
    """Run a command and return (returncode, stdout, stderr).

    Timeout is set to prevent indefinite hangs (e.g., patch waiting for input).
    """
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            stdin=subprocess.DEVNULL,
            timeout=timeout
        )
        return result.returncode, result.stdout.strip(), result.stderr.strip()
    except subprocess.TimeoutExpired:
        return -1, '', 'Command timed out'
    except Exception as e:
        return -1, '', str(e)


def apply_patch(patch_file, target_dir, patch_level=1):
    """
    Apply a single patch file if not already applied.
    Uses 'patch' command which works correctly regardless of git repo structure.

    Returns:
        True if patch was applied or already applied
        False if there was a conflict
    """
    patch_name = os.path.basename(patch_file)

    # Check if patch can be applied (dry-run)
    ret, _, stderr = run_cmd(
        ['patch', '-p' + str(patch_level), '--dry-run', '-i', patch_file],
        cwd=target_dir
    )

    if ret == 0:
        # Patch can be applied - apply it
        log_info(f'Applying patch: {patch_name}')
        ret, _, stderr = run_cmd(
            ['patch', '-p' + str(patch_level), '-i', patch_file],
            cwd=target_dir
        )
        if ret == 0:
            log_success(f'Applied: {patch_name}')
            return True
        else:
            log_error(f'Failed to apply: {patch_name}')
            if stderr:
                print(f'    {stderr}')
            return False
    else:
        # Check if already applied (reverse dry-run)
        ret, _, stderr = run_cmd(
            ['patch', '-p' + str(patch_level), '--dry-run', '-R', '-i', patch_file],
            cwd=target_dir
        )
        if ret == 0:
            log_success(f'Already applied: {patch_name}')
            return True
        else:
            log_warning(f'Patch conflict: {patch_name}')
            if stderr:
                print(f'    {stderr}')
            return False


def apply_patches_in_dir(patch_dir, target_dir, patch_level=1, rollback_on_failure=True):
    """Apply all patches in a directory

    Args:
        patch_dir: Directory containing patch files
        target_dir: Target directory to apply patches
        patch_level: Patch level (default: 1)
        rollback_on_failure: If True, revert applied patches when a patch fails

    Returns:
        True if all patches applied successfully
    """
    if not os.path.isdir(patch_dir):
        return True

    patches = sorted(Path(patch_dir).glob('*.patch'))
    if not patches:
        return True

    applied_patches = []
    success = True

    for patch in patches:
        if not apply_patch(str(patch), target_dir, patch_level):
            success = False
            if rollback_on_failure and applied_patches:
                log_warning(f'Patch failed: {os.path.basename(patch)}, rolling back...')
                for applied_patch in reversed(applied_patches):
                    patch_name = os.path.basename(applied_patch)
                    log_info(f'Reverting patch: {patch_name}')
                    ret, _, stderr = run_cmd(
                        ['patch', '-p' + str(patch_level), '--dry-run', '-R', '-i', applied_patch],
                        cwd=target_dir
                    )
                    if ret == 0:
                        run_cmd(
                            ['patch', '-p' + str(patch_level), '-R', '-i', applied_patch],
                            cwd=target_dir
                        )
                        log_success(f'Reverted: {patch_name}')
                    else:
                        log_error(f'Failed to revert: {patch_name}')
            break
        applied_patches.append(str(patch))

    return success


def main():
    parser = argparse.ArgumentParser(
        description='ESP-Weaver Patch Application Script'
    )
    parser.add_argument(
        '--patches-dir',
        required=True,
        help='Path to the patches directory'
    )
    parser.add_argument(
        '--idf-path',
        help='Path to ESP-IDF (applies IDF patches)'
    )

    args = parser.parse_args()

    # Early check for patch command availability
    ret, _, _ = run_cmd(['patch', '--version'], cwd='.')
    if ret != 0:
        log_error("'patch' command is not installed or not accessible in PATH")
        return 1

    patches_dir = os.path.abspath(args.patches_dir)

    if not os.path.isdir(patches_dir):
        log_error(f'Patches directory not found: {patches_dir}')
        return 1

    success = True
    rollback_on_failure = True

    # Apply ESP-IDF patches
    if args.idf_path:
        idf_path = os.path.abspath(args.idf_path)
        if os.path.isdir(idf_path):
            log_info('Checking ESP-IDF patches...')
            idf_patch_dir = os.path.join(patches_dir, 'idf')
            if not apply_patches_in_dir(idf_patch_dir, idf_path, patch_level=1,
                                       rollback_on_failure=rollback_on_failure):
                log_error('Failed to apply ESP-IDF patches')
                success = False
        else:
            log_error(f'ESP-IDF path not found: {idf_path}')
            success = False

    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
