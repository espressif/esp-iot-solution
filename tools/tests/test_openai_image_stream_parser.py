# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Build and exercise the OpenAI image SSE parser in the host CI job."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / 'components' / 'openai'


class ImageStreamParserTests(unittest.TestCase):
    def test_parser_handles_fragmented_large_and_bad_events(self):
        with tempfile.TemporaryDirectory(prefix='openai-image-parser-') as directory:
            binary = Path(directory) / 'test_image_stream_parser'
            subprocess.run(
                [os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                 '-I', str(SOURCE_DIR), '-o', str(binary),
                 str(SOURCE_DIR / 'image_stream_parser.c'),
                 str(SOURCE_DIR / 'tests' / 'test_image_stream_parser.c')],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
