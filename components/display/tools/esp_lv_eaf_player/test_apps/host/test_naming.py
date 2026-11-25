# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#!/usr/bin/env python3
"""Frozen Revision 1 naming/ID contract; independent of encoder implementation details."""
from __future__ import annotations
import ctypes
import importlib
import io
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from encoder import names
from encoder.eaf4 import decode, encode_565
from tools.eaf_cli import _encode_single_to_bytes, _inspect
from argparse import Namespace

CODECS=('RLE','HUFFMAN_RLE','JPEG','HUFFMAN','LOSSLESS','BOUNDED','JPEG_DELTA')
FRAMES=('FULL','XOR_BGRA8888','XOR_PLANAR_BGRA8888','TILE_XOR_PLANAR_BGRA8888',
        'TILE_XOR_PACKED_BGRA8888','TILE_JOINT_DELTA_BGRA8888','TILE_BOUNDED_BGRA8888',
        'TILE_DELTA_RGB565','TILE_JOINT_DELTA_RGB565','TILE_BOUNDED_RGB565',
        'HSUB_BGRA8888','TILE_SUB_PACKED_BGRA8888')
BLOCKS=('SKIP','SOLID','PALETTE2','PALETTE4','LITERAL')

class NamingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lib=ctypes.CDLL(str(ROOT/'build/libeaf_host.so'))
        for name in ('esp_eaf_codec_name','eaf4_frame_mode_name'):
            fn=getattr(cls.lib,name);fn.argtypes=[ctypes.c_int];fn.restype=ctypes.c_char_p

    def check_c_enum(self,path,prefix,expected):
        text=(ROOT/path).read_text()
        actual={name:int(value,0) for name,value in re.findall(r'\b('+prefix+r'\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)',text)}
        expected={prefix+name:i for i,name in enumerate(expected)}
        if prefix!='BOUNDED_BLOCK_':expected[prefix+'MAX']=len(expected)
        self.assertEqual(actual,expected)

    def test_fixed_codec_names_ids_and_c_logging(self):
        self.assertEqual(names.CODEC_NAMES,dict(enumerate(CODECS)))
        self.check_c_enum('include/esp_eaf_codec.h','ESP_EAF_CODEC_',CODECS)
        for i,name in enumerate(CODECS):
            self.assertEqual(getattr(names,'CODEC_'+name),i)
            self.assertEqual(self.lib.esp_eaf_codec_name(i).decode(),name)
        self.assertEqual(self.lib.esp_eaf_codec_name(-1),b'UNKNOWN')
        self.assertEqual(self.lib.esp_eaf_codec_name(len(CODECS)),b'UNKNOWN')

    def test_fixed_frame_names_ids_and_c_logging(self):
        self.assertEqual(names.FRAME_MODE_NAMES,dict(enumerate(FRAMES)))
        self.check_c_enum('include/eaf4.h','EAF4_FRAME_',FRAMES)
        for i,name in enumerate(FRAMES):
            self.assertEqual(getattr(names,'FRAME_'+name),i)
            self.assertEqual(self.lib.eaf4_frame_mode_name(i).decode(),name)
        self.assertEqual(self.lib.eaf4_frame_mode_name(-1),b'UNKNOWN')
        self.assertEqual(self.lib.eaf4_frame_mode_name(12),b'UNKNOWN')

    def test_fixed_bounded_block_names_and_ids(self):
        self.assertEqual(names.BOUNDED_BLOCK_NAMES,dict(enumerate(BLOCKS)))
        self.check_c_enum('src/codec/bounded.h','BOUNDED_BLOCK_',BLOCKS)
        for i,name in enumerate(BLOCKS):self.assertEqual(getattr(names,'BOUNDED_BLOCK_'+name),i)

    def test_old_symbols_and_modules_are_not_exported(self):
        eaf4=importlib.import_module('encoder.eaf4')
        for name in ('MODE_RAW','MODE_XOR','MODE_PLANAR','MODE_BTC8888_SAFE','MODE_BTC565_SAFE'):
            self.assertFalse(hasattr(eaf4,name))
        for module in ('encoder.btc8888','encoder.quad_jpeg'):
            with self.assertRaises(ModuleNotFoundError):importlib.import_module(module)
        header=(ROOT/'include/esp_eaf_codec.h').read_text()
        self.assertNotIn('ESP_EAF_MODE_',header)
        self.assertNotIn('esp_eaf_mode_t',header)
        self.assertNotIn('EAF4_SUBMODE_', (ROOT/'include/eaf4.h').read_text())

    def test_cli_has_exact_codec_choices_and_rejects_aliases(self):
        self.assertEqual(names.PUBLIC_CODEC_NAMES,('lossless','bounded','jpeg-delta'))
        cli=ROOT/'tools/eaf_cli.py'
        for old in ('video','btc-safe','quad-jpeg'):
            result=subprocess.run([sys.executable,str(cli),'encode','-o','unused','--codec',old,'unused.png'],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertIn('invalid choice',result.stderr)
        for old in ('565','8888'):
            result=subprocess.run([sys.executable,str(cli),'encode','-o','unused','--format',old,'unused.png'],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
        result=subprocess.run([sys.executable,str(cli),'encode','-o','unused','--btc-max-error','8','unused.png'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertIn('unrecognized arguments',result.stderr)

    def test_cli_alpha_cleanup_defaults_and_override(self):
        source=np.full((8,8,4),[30,60,90,0],np.uint8)
        source[:,2:4,3]=1
        source[:,4:6,3]=128
        source[:,6:,3]=255
        expected=source[..., [2,1,0,3]].copy()
        expected[expected[...,3]==0,:3]=0
        with tempfile.TemporaryDirectory() as tmp:
            image=Path(tmp)/'source.png';Image.fromarray(source,'RGBA').save(image)
            for codec in ('lossless','bounded'):
                blob,summary=_encode_single_to_bytes([str(image)],'bgra8888',codec,67,30,0)
                np.testing.assert_array_equal(decode(blob)[0],expected)
                self.assertTrue(summary['clean_alpha'])
                for command in ('encode','batch'):
                    if command=='batch':
                        clip=Path(tmp)/'inputs'/'clip';clip.mkdir(parents=True,exist_ok=True)
                        Image.fromarray(source,'RGBA').save(clip/'source.png')
                        inputs=clip.parent;output=Path(tmp)/'outputs'
                        encoded=output/'clip.eaf4'
                    else:
                        inputs=image;output=Path(tmp)/'exact.eaf4';encoded=output
                    result=subprocess.run([sys.executable,str(ROOT/'tools/eaf_cli.py'),command,str(inputs),'-o',str(output),'--codec',codec,'--max-error','0','--no-clean-alpha'],capture_output=True,text=True)
                    self.assertEqual(result.returncode,0,result.stderr)
                    np.testing.assert_array_equal(decode(encoded.read_bytes())[0],source[..., [2,1,0,3]])
            # Default cleanup must not break RGB565, RGB565A8 or opaque codecs.
            for fmt,codec in [('rgb565','lossless'),('rgb565a8','lossless'),('rgb565','bounded'),('rgb565','jpeg-delta')]:
                _encode_single_to_bytes([str(image)],fmt,codec,67,30,8)
        result=subprocess.run([sys.executable,str(ROOT/'tools/eaf_cli.py'),'encode','unused','-o','unused','--lossless-profile','v2'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertIn('unrecognized arguments',result.stderr)

    def test_cli_pixel_format_matches_header_and_inspector(self):
        source=np.random.default_rng(82).integers(0,256,(16,16,4),dtype=np.uint8)
        with tempfile.TemporaryDirectory() as tmp:
            image=Path(tmp)/'source.png';Image.fromarray(source,'RGBA').save(image)
            for fmt,value in [('bgra8888',0),('rgb565',1),('rgb565a8',2)]:
                blob,summary=_encode_single_to_bytes([str(image)],fmt,'lossless',67,30,8,clean_alpha=False)
                self.assertEqual(blob[:4],b'EAF4');self.assertEqual(blob[52],value)
                self.assertEqual(summary['format'],fmt.upper())
                self.assertEqual(summary['codec'],'LOSSLESS')
                self.assertEqual(blob[53],4 if fmt!='bgra8888' else 0)
                path=Path(tmp)/'out.eaf4';path.write_bytes(blob)
                stream=io.StringIO()
                with redirect_stdout(stream):self.assertEqual(_inspect(Namespace(file=str(path))),0)
                self.assertIn(fmt.upper(),stream.getvalue());self.assertIn('FULL:1',stream.getvalue())
            for codec,fmt in [('bounded','rgb565a8'),('jpeg-delta','bgra8888')]:
                with self.assertRaises(ValueError):_encode_single_to_bytes([str(image)],fmt,codec,67,30,8)

    def test_cli_rejects_unsupported_preprocessing_before_loading_inputs(self):
        for flag in ({'clean_alpha':True},{'temporal_deadzone':1}):
            for fmt,codec in [('rgb565','lossless'),('rgb565a8','lossless'),('rgb565','jpeg-delta')]:
                with self.subTest(flag=flag,fmt=fmt,codec=codec):
                    with self.assertRaises(ValueError):
                        _encode_single_to_bytes([],fmt,codec,67,30,8,**flag)
        for threshold in (-1,256):
            with self.assertRaises(ValueError):
                _encode_single_to_bytes([],'bgra8888','lossless',67,30,8,temporal_deadzone=threshold)
        with self.assertRaises(ValueError):
            _encode_single_to_bytes([],'bgra8888','bounded',67,30,8,temporal_deadzone=1)

    def test_hw_jpeg_is_independent_of_software_fallback(self):
        header=ROOT/'include/esp_lv_eaf_player_config.h'
        for hardware in (0,1):
            result=subprocess.run(
                ['cc','-E','-x','c','-','-I',str(header.parent),
                 f'-DCONFIG_SOC_JPEG_DECODE_SUPPORTED={hardware}',
                 '-DCONFIG_ESP_LV_EAF_ENABLE_SW_JPEG=0'],
                input=f'#include "esp_lv_eaf_player_config.h"\n#if ESP_LV_EAF_ENABLE_HW_JPEG != {hardware}\n#error incorrect backend selection\n#endif\n',
                capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_inspector_entrypoints_work_from_repository(self):
        for args in [('tools/eaf_inspect.py',),('-m','tools.eaf_inspector')]:
            result=subprocess.run([sys.executable,*args,'--help'],cwd=ROOT,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('--no-block-decode',result.stdout)

    def test_rgb565_lossless_has_no_hidden_alpha_plane(self):
        # Alpha-only motion must not enter a declared RGB565 stream.
        frame=np.full((8,16,4),120,dtype=np.uint8);changed=frame.copy();changed[...,3]=20
        blob,summary=encode_565([frame,changed],with_alpha=False)
        self.assertEqual(blob[52],1)
        self.assertEqual(struct.unpack_from('<I',blob,20)[0],16*8*2)
        restored=decode(blob)
        np.testing.assert_array_equal(restored[0],restored[1])
        self.assertTrue(np.all(restored[0][...,3]==255))
        self.assertEqual(summary['pixel_format'],1)

if __name__=='__main__':unittest.main(verbosity=2)
