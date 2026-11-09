# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import sys

from setuptools import Extension, setup

sources = [
    'src/core.c',
    'src/ddelta_generate.c',
    'src/ddelta_apply.c',
    'third_party/libdivsufsort/lib/divsufsort.c',
    'third_party/libdivsufsort/lib/sssort.c',
    'third_party/libdivsufsort/lib/trsort.c',
    'third_party/libdivsufsort/lib/utils.c',
]

macros = [
    ('HAVE_CONFIG_H', '1'),
    ('DDELTA_NO_MAIN', '1'),
]
if sys.platform == 'win32':
    macros.append(('_CRT_SECURE_NO_WARNINGS', '1'))

setup(
    name='esp_ddelta',
    version='0.1.0',
    description='Uncompressed ddelta encode/decode for bootloader_support_plus',
    packages=['esp_ddelta'],
    license_files=['NOTICE'],
    ext_modules=[
        Extension(
            name='esp_ddelta._core',
            sources=sources,
            include_dirs=[
                'src',
                'third_party/libdivsufsort/include',
            ],
            define_macros=macros,
        )
    ],
    python_requires='>=3.8',
)
