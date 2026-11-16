# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import io
import ctypes as C
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import numpy as np
from test_host_core import ROOT, Player, classic_file, classic_frame, rle, huff, to565, frames
from encoder.eaf4 import encode, decode, _pack, INDEX
from tools.eaf_inspector.parser import parse_bytes

class RegressionTests(unittest.TestCase):
    def test_inspector_truecolor_and_empty_delta(self):
        raw=bytes([20,40,80])*16*8
        for mode in (0,1,3):
            payload=rle(raw) if mode==0 else huff(rle(raw)) if mode==1 else huff(raw)
            blob=classic_file([classic_frame([bytes([mode])+payload],width=16,height=8,block_height=8)])
            report=parse_bytes(blob)
            self.assertFalse(any(i.severity=='error' for i in report.all_issues()))
            self.assertEqual(report.frames[0].blocks_info[0].expected_size,len(raw))
        blank=classic_file([classic_frame([b''],width=16,height=8,block_height=8)])
        self.assertFalse(any(i.severity=='error' for i in parse_bytes(blank).all_issues()))

    def test_inspect_honors_declared_bounded_codec(self):
        frame=np.zeros((4,4,4),np.uint8);raw=frame.tobytes();payload=zlib.compress(raw)
        blob=_pack([(len(payload),len(raw),zlib.crc32(raw),0,zlib.crc32(payload))],[payload],4,4,67,codec_id=5)
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bounded.eaf4';p.write_bytes(blob)
            result=subprocess.run([sys.executable,str(ROOT/'tools/eaf_cli.py'),'inspect',str(p)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('0x05 EAF Bounded',result.stdout)

    def test_truncated_inspect_reports_error_without_traceback(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bad.eaf4';p.write_bytes(b'EAF4'+bytes(12))
            result=subprocess.run([sys.executable,str(ROOT/'tools/eaf_cli.py'),'inspect',str(p)],capture_output=True,text=True)
            self.assertEqual(result.returncode,1)
            self.assertIn('error:',result.stderr)
            self.assertNotIn('Traceback',result.stderr)

    def test_parallel_first_decode_in_fresh_process(self):
        result=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--parallel-worker'],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_obsolete_modes_rejected_before_io(self):
        from encoder.eaf565 import convert_to_eaf
        for mode in ('jpeg','strip-delta','invalid'):
            with self.assertRaisesRegex(ValueError,'Unknown mode'):
                convert_to_eaf(['/does/not/exist.png'],'/must/not/write.eaf',mode=mode)

def parallel_worker():
    source=frames();clips=[]
    for mode in (5,8):
        if mode==5:
            blob,_=encode(source,forced_mode=5,key_interval=3,clean_alpha=False);expected=[x.tobytes() for x in source]
        else:
            from encoder.eaf4 import encode_565
            blob,_=encode_565(source,with_alpha=True,key_interval=3)
            expected=[to565(x).tobytes()+x[...,3].tobytes() for x in source]
        clips.append((blob,expected))
    barrier=threading.Barrier(8)
    def run(i):
        blob,expected=clips[i%len(clips)];barrier.wait()
        for _ in range(4):
            with Player(blob) as p:
                for j,want in enumerate(expected):
                    assert p.seek(j)==0
                    assert p.pixels()==want
    with ThreadPoolExecutor(max_workers=8) as pool:list(pool.map(run,range(8)))

if __name__=='__main__':
    if '--parallel-worker' in sys.argv:parallel_worker()
    else:unittest.main(verbosity=2)
