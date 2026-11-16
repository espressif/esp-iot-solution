# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#!/usr/bin/env python3
"""Exercise the production player, classic EAF parser and codec lifecycle on Host."""
from __future__ import annotations
import ctypes as C
import io
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from encoder.eaf4 import (encode, encode_565, decode, _pack, _encode_565a8_frame_candidates)
from encoder.eaf565_lossless_rgba import encode_scene, Deflater
from encoder.eaf565 import convert_to_eaf

class Config(C.Structure):
    _fields_ = [('swap', C.c_bool), ('key', C.c_int32), ('output', C.c_int)]
class Info(C.Structure):
    _fields_ = [('container', C.c_int), ('format', C.c_int), ('width', C.c_uint16),
                ('height', C.c_uint16), ('count', C.c_uint32), ('period', C.c_uint32), ('alpha', C.c_bool)]
class Rect(C.Structure):
    _fields_ = [(n, C.c_uint16) for n in ('x1','y1','x2','y2')] + [('valid', C.c_bool)]
class Frame(C.Structure):
    _fields_ = [('pixels', C.c_void_p), ('alpha', C.c_void_p), ('size', C.c_size_t),
                ('stride', C.c_size_t), ('index', C.c_uint32), ('dirty', Rect)]

LIB = C.CDLL(str(ROOT / 'build/libeaf_host.so'))
for name, args, restype in [
    ('esp_eaf_player_open_memory',[C.c_void_p,C.c_size_t,C.POINTER(Config),C.POINTER(C.c_void_p)],C.c_int),
    ('esp_eaf_player_open_file',[C.c_char_p,C.POINTER(Config),C.POINTER(C.c_void_p)],C.c_int),
    ('esp_eaf_player_get_info',[C.c_void_p],C.POINTER(Info)),
    ('esp_eaf_player_get_frame',[C.c_void_p],C.POINTER(Frame)),
    ('esp_eaf_player_seek',[C.c_void_p,C.c_uint32],C.c_int),
    ('esp_eaf_player_next',[C.c_void_p],C.c_int),
    ('esp_eaf_player_close',[C.c_void_p],None),
    ('esp_eaf_player_release_cache',[C.c_void_p],None),
    ('eaf_rle_decode',[C.c_void_p,C.c_size_t,C.c_void_p,C.POINTER(C.c_size_t),C.c_bool],C.c_int),
    ('eaf_huffman_decode',[C.c_void_p,C.c_size_t,C.c_void_p,C.POINTER(C.c_size_t),C.c_bool],C.c_int),
]:
    fn=getattr(LIB,name);fn.argtypes=args;fn.restype=restype

class Player:
    def __init__(self, blob, config=None, path=None):
        self.memory=C.create_string_buffer(bytes(blob))
        self.handle=C.c_void_p()
        cfg=config or Config(False,-1,0)
        if path is None:
            rc=LIB.esp_eaf_player_open_memory(self.memory,len(blob),C.byref(cfg),C.byref(self.handle))
        else:
            rc=LIB.esp_eaf_player_open_file(os.fsencode(path),C.byref(cfg),C.byref(self.handle))
        if rc: raise ValueError(f'open failed: {rc:#x}')
    @property
    def info(self): return LIB.esp_eaf_player_get_info(self.handle).contents
    @property
    def frame(self): return LIB.esp_eaf_player_get_frame(self.handle).contents
    def pixels(self):
        f=self.frame
        return C.string_at(f.pixels,f.size)+(C.string_at(f.alpha,self.info.width*self.info.height) if f.alpha else b'')
    def seek(self,index): return LIB.esp_eaf_player_seek(self.handle,index)
    def close(self):
        LIB.esp_eaf_player_close(self.handle);self.handle=None
    def __enter__(self):return self
    def __exit__(self,*exc):self.close()

def classic_frame(blocks, depth=24, palette=b'', width=16, height=8, block_height=4):
    return b'_S\x0000V1.0'+bytes([depth])+struct.pack('<HHHH',width,height,len(blocks),block_height)+b''.join(struct.pack('<I',len(b)) for b in blocks)+palette+b''.join(blocks)

def classic_file(frames, magic=b'EAF'):
    blobs=[b'ZZ'+f for f in frames]; offset=0; table=bytearray()
    for b in blobs:table+=struct.pack('<II',len(b),offset);offset+=len(b)
    raw=bytes(table)+b''.join(blobs)
    return b'\x89'+magic+struct.pack('<III',len(frames),sum(raw)&0xffffffff,len(raw))+raw

def rle(data):return b''.join(bytes([1,b]) for b in data)
def huff(data):
    dictionary=b'\0'+b''.join(bytes([v,8,v]) for v in sorted(set(data)))
    return struct.pack('<H',len(dictionary))+dictionary+data

def to565(bgra):
    v=bgra.astype(np.uint16)
    return (((v[...,2]>>3)<<11)|((v[...,1]>>2)<<5)|(v[...,0]>>3)).astype('<u2')

def frames():
    rng=np.random.default_rng(71)
    first=rng.integers(10,240,(8,16,4),dtype=np.uint8)
    out=[first]
    for i in range(1,7):
        f=out[-1].copy(); f[4:8,4:8]=(f[4:8,4:8].astype(int)+1)%256;out.append(f)
    out.append(out[-1].copy())
    return out

class HostCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixtures=ROOT/'build/host_fixtures'
        fixtures.mkdir(exist_ok=True)
        for path in fixtures.glob('clip_*.bin'):path.unlink()

    def check_sequence(self,blob,expected,config=None):
        with Player(blob,config) as p:
            self.assertEqual(p.info.count,len(expected))
            for _ in range(2):
                for i,want in enumerate(expected):
                    self.assertEqual(p.seek(i),0)
                    self.assertEqual(p.pixels(),want)
            for i in [len(expected)-1,0,len(expected)//2,1,1,0]:
                self.assertEqual(p.seek(i),0);self.assertEqual(p.pixels(),expected[i])
            self.assertNotEqual(p.seek(len(expected)),0)
            self.assertEqual(p.seek(0),0)
            self.assertEqual(LIB.esp_eaf_player_next(p.handle),0)
            self.assertEqual(p.pixels(),expected[1 % len(expected)])
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'clip.bin';path.write_bytes(blob)
            with Player(blob,config,path) as p:self.assertEqual(p.pixels(),expected[0])
        fixture=ROOT/'build/host_fixtures';fixture.mkdir(exist_ok=True)
        (fixture/f'clip_{zlib.crc32(blob):08x}.bin').write_bytes(blob)

    def test_all_bgra_submodes_and_error_bounds(self):
        fs=frames()
        for mode in (*range(7), 10, 11):
            with self.subTest(mode=mode):
                blob,_=encode(fs,forced_mode=mode,key_interval=3,max_error=8,clean_alpha=False)
                expected=decode(blob)
                for src,dst in zip(fs,expected):
                    self.assertLessEqual(int(np.abs(src.astype(int)-dst.astype(int)).max()),8 if mode==6 else 0)
                self.check_sequence(blob,[f.tobytes() for f in expected])

    def test_sub_modes_wrap_channels_and_partial_mask(self):
        rng=np.random.default_rng(20261111)
        for h,w in [(4,4),(4,12),(12,12)]:
            first=rng.integers(0,256,(h,w,4),dtype=np.uint8)
            changed=((first.astype(int)+[1,127,128,255])%256).astype(np.uint8)
            sparse=changed.copy();sparse[-4:,-4:]=0
            fs=[first,first.copy(),changed,sparse,first]
            for mode in (10,11):
                with self.subTest(shape=(h,w),mode=mode):
                    blob,_=encode(fs,forced_mode=mode,key_interval=3,clean_alpha=False)
                    for src,dst in zip(fs,decode(blob)):np.testing.assert_array_equal(src,dst)
                    self.check_sequence(blob,[f.tobytes() for f in fs])

    def test_alpha_cleanup_is_reported_and_preserves_input(self):
        src=np.full((8,16,4),[30,60,90,0],np.uint8)
        original=src.copy()
        blob,summary=encode([src,src])
        np.testing.assert_array_equal(src,original)
        np.testing.assert_array_equal(decode(blob)[0],np.zeros_like(src))
        self.assertFalse(summary['roundtrip_exact'])
        self.assertTrue(summary['clean_alpha'])
        self.assertEqual(summary['temporal_deadzone'],0)
        self.check_sequence(blob,[np.zeros_like(src).tobytes()]*2)
        blob,summary=encode([src],clean_alpha=False)
        self.assertTrue(summary['roundtrip_exact'])
        np.testing.assert_array_equal(decode(blob)[0],src)

    def test_temporal_deadzone_bounds_use_reconstructed_reference(self):
        fs=[np.full((16,16,4),[40+i,70+i,100+i,255],np.uint8) for i in range(8)]
        blob,summary=encode(fs,temporal_deadzone=1,key_interval=3,clean_alpha=False)
        restored=decode(blob)
        self.assertFalse(summary['roundtrip_exact'])
        self.assertEqual(summary['temporal_deadzone'],1)
        self.assertTrue(any(not np.array_equal(a,b) for a,b in zip(fs,restored)))
        for i,(src,dst) in enumerate(zip(fs,restored)):
            self.assertLessEqual(int(np.abs(src.astype(int)-dst.astype(int)).max()),1)
            if i%3==0:np.testing.assert_array_equal(src,dst)
        self.check_sequence(blob,[f.tobytes() for f in restored])
        for threshold in (-1,256):
            with self.assertRaises(ValueError):encode(fs,temporal_deadzone=threshold,clean_alpha=False)
        with self.assertRaises(ValueError):encode(fs,codec='bounded',temporal_deadzone=1,clean_alpha=False)

    def test_565a8_modes_seven_and_eight(self):
        fs=frames();cs=[to565(f) for f in fs];aa=[f[...,3] for f in fs]
        for mode in (7,8):
            with self.subTest(mode=mode):
                expected=[];records=[];payload=[]
                for i,(color,alpha) in enumerate(zip(cs,aa)):
                    raw=color.tobytes()+alpha.tobytes();expected.append(raw)
                    if i%3==0:m=0;blob=zlib.compress(raw);n=len(raw)
                    else:
                        candidates=_encode_565a8_frame_candidates(color,alpha,cs[i-1],aa[i-1])
                        m,blob,n=next((v for v in candidates if v[0]==mode),candidates[0])
                    records.append((len(blob),n,zlib.crc32(raw),m,zlib.crc32(blob)));payload.append(blob)
                blob=_pack(records,payload,16,8,45,pixel_format=2,codec_id=4)
                self.check_sequence(blob,expected)
                with Player(blob) as p:self.assertEqual(p.info.period,45);self.assertTrue(p.info.alpha)

    def test_rgb565_lossless_without_alpha(self):
        source=frames()
        blob,_=encode_565(source,with_alpha=False,key_interval=3)
        self.check_sequence(blob,[to565(f).tobytes() for f in source])
        with Player(blob) as p:
            self.assertFalse(p.info.alpha)
            self.assertFalse(p.frame.alpha)

    def test_565_bounded_and_temporal_bound(self):
        blob,_=encode_565(frames(),codec='bounded',key_interval=3)
        expected=[to565(f).tobytes() for f in decode(blob)]
        self.check_sequence(blob,expected)
        with Player(blob) as p:self.assertFalse(p.info.alpha)

    def test_palette_modes_multiblock_tail_alpha_and_byte_swap(self):
        for depth in (4,8):
            palette=b''.join(bytes([i*7%256,i*13%256,i*23%256,i*17%256]) for i in range(1<<depth))
            indices=(np.arange(16*7)%(1<<depth)).astype(np.uint8)
            for mode in (0,1,3):
                with self.subTest(depth=depth,mode=mode):
                    blocks=[]
                    for idx in (indices[:64],indices[64:]):
                        raw=idx.tobytes() if depth==8 else bytes((int(idx[i])<<4)|int(idx[i+1]) for i in range(0,len(idx),2))
                        blocks.append(bytes([mode])+(rle(raw) if mode==0 else huff(rle(raw)) if mode==1 else huff(raw)))
                    blob=classic_file([classic_frame(blocks,depth,palette,height=7)]*2)
                    pal=np.frombuffer(palette,np.uint8).reshape(-1,4);colors=to565(pal[indices].reshape(7,16,4));colors[pal[indices,3].reshape(7,16)==0]=0
                    expected=colors.tobytes()+pal[indices,3].tobytes()
                    self.check_sequence(blob,[expected]*2)
                    swapped=colors.byteswap().tobytes()+pal[indices,3].tobytes()
                    self.check_sequence(blob,[swapped]*2,Config(True,-1,0))
                    if depth==8:self.check_sequence(blob,[pal[indices].tobytes()]*2,Config(False,-1,2))

    def test_aaf_and_truecolor_blocks(self):
        raw=bytes([20,40,80])*64;frame=classic_frame([b'\0'+rle(raw)]*2)
        blob=classic_file([frame]*2,b'AAF')
        expected=to565(np.tile([20,40,80,255],(8,16,1))).tobytes()
        self.check_sequence(blob,[expected]*2)

    def test_classic_lossless_joint_and_skipped_first_stripe(self):
        fs=frames();colors=[to565(f) for f in fs];alphas=[f[...,3] for f in fs]
        with tempfile.TemporaryDirectory() as d:
            # Encoder emits joint delta blocks and an empty first stripe on inter frames.
            blob=encode_scene(colors,alphas,4,Deflater(True,1))
        expected=[c.tobytes()+a.tobytes() for c,a in zip(colors,alphas)]
        self.check_sequence(blob,expected)

    def test_classic_bounded_and_lossless_cli_payloads(self):
        fs=frames()
        with tempfile.TemporaryDirectory() as d:
            paths=[]
            for i,f in enumerate(fs):
                p=Path(d)/f'{i}.png';Image.fromarray(f[...,[2,1,0,3]]).save(p);paths.append(str(p))
            for mode in ('lossless','bounded'):
                dest=Path(d)/'out.eaf';convert_to_eaf(paths,str(dest),mode=mode,keyframe_interval=3)
                blob=dest.read_bytes()
                if mode=='lossless':expected=[to565(f).tobytes()+f[...,3].tobytes() for f in fs]
                else:
                    with Player(blob) as p:
                        expected=[]
                        for i in range(len(fs)):
                            self.assertEqual(p.seek(i),0);expected.append(p.pixels())
                            a=np.frombuffer(p.pixels(),'<u2');b=to565(fs[i]).ravel()
                            for shift,mask,scale in [(11,31,8),(5,63,4),(0,31,8)]:
                                self.assertLessEqual(int(np.abs(((a>>shift)&mask).astype(int)*scale-((b>>shift)&mask).astype(int)*scale).max()),8)
                self.check_sequence(blob,expected)

    def test_jpeg_blocks_and_state_reset(self):
        blocks=[]
        for rgb in ([200,30,40],[20,180,60]):
            buf=io.BytesIO();Image.new('RGB',(16,4),tuple(rgb)).save(buf,format='JPEG',quality=90)
            blocks.append(b'\2'+buf.getvalue())
        blob=classic_file([classic_frame(blocks)]*2)
        with Player(blob) as p:expected=p.pixels()
        self.check_sequence(blob,[expected]*2)
        with Player(blob,Config(True,-1,0)) as p:
            self.assertEqual(p.pixels(),np.frombuffer(expected,'<u2').byteswap().tobytes())

    def test_jpeg_delta_strip_padding_and_shared_tables(self):
        from jpeg_fixtures import split_jpeg_header_and_scan, rebuild_abbreviated_jpeg
        for subsampling in (0, 2):
            with self.subTest(subsampling=subsampling):
                first=io.BytesIO();Image.new('RGB',(32,16),(10,30,80)).save(first,format='JPEG',quality=85,subsampling=subsampling)
                header,sof,_=split_jpeg_header_and_scan(first.getvalue())
                mcus=3 if subsampling==0 else 2
                atlas_pixels=np.random.default_rng(81).integers(0,256,(16,16*mcus,3),dtype=np.uint8)
                atlas=io.BytesIO();Image.fromarray(atlas_pixels).save(atlas,format='JPEG',quality=85,subsampling=subsampling)
                _,_,scan=split_jpeg_header_and_scan(atlas.getvalue())
                base=np.array(Image.open(io.BytesIO(first.getvalue())).convert('RGBA'))[...,[2,1,0,3]]
                strip=np.array(Image.open(io.BytesIO(rebuild_abbreviated_jpeg(header,sof,16*mcus,16,scan))).convert('RGBA'))[...,[2,1,0,3]]
                out=base.copy();out[:16,:16]=strip[:16,:16];out[:8,16:24]=strip[:8,16:24]
                f0=classic_frame([b'\2'+first.getvalue()],width=32,height=16,block_height=16)
                f1=classic_frame([b'\6\3\x1f'+scan],width=32,height=16,block_height=16)
                blob=classic_file([f0,f1,f1])
                self.check_sequence(blob,[to565(base).tobytes(),to565(out).tobytes(),to565(out).tobytes()])

    def test_experimental_ids_are_rejected(self):
        for mode in (8,9,10,11,12,255):
            with self.subTest(mode=mode):
                blob=classic_file([classic_frame([bytes([mode])+bytes(8)]*2)])
                src=C.create_string_buffer(blob);handle=C.c_void_p()
                self.assertEqual(LIB.esp_eaf_player_open_memory(src,len(blob),None,C.byref(handle)),0x106)
                self.assertFalse(handle.value)

    def test_entropy_rejects_truncation_and_overflow(self):
        for fn,data,capacity in [
            ('eaf_rle_decode',b'\1',4),('eaf_rle_decode',b'\5\0',4),
            ('eaf_huffman_decode',huff(bytes(8)),4),
            ('eaf_huffman_decode',b'\1\0\xff\0',4),
        ]:
            src=C.create_string_buffer(data);out=C.create_string_buffer(capacity);n=C.c_size_t(capacity)
            self.assertNotEqual(getattr(LIB,fn)(src,len(data),out,C.byref(n),False),0)

    def test_eaf4_corruption_and_recovery(self):
        blob,_=encode(frames(),forced_mode=4,key_interval=3,clean_alpha=False)
        for offset in (0,8,40,64,len(blob)-1):
            broken=bytearray(blob);broken[offset]^=0x40
            if offset==len(blob)-1:
                with Player(broken) as p:self.assertNotEqual(p.seek(len(frames())-1),0)
            else:
                with self.assertRaises(ValueError):Player(broken)
        with Player(blob) as p:
            record=64+24;payload=struct.unpack_from('<I',blob,record)[0]
            address=C.addressof(p.memory);byte=C.c_uint8.from_address(address+payload);saved=byte.value;byte.value^=1
            self.assertNotEqual(p.seek(1),0)
            byte.value=saved
            self.assertEqual(p.seek(1),0)
            self.assertEqual(p.pixels(),frames()[1].tobytes())

    def test_independent_players_and_no_change_dirty(self):
        blob,_=encode(frames(),forced_mode=4,key_interval=30,clean_alpha=False)
        with Player(blob) as a,Player(blob) as b:
            self.assertEqual(a.seek(7),0);self.assertEqual(a.pixels(),frames()[7].tobytes())
            self.assertEqual(b.pixels(),frames()[0].tobytes())
            self.assertEqual(a.seek(6),0);self.assertEqual(a.seek(7),0)
            self.assertFalse(a.frame.dirty.valid)

    def test_release_cache_preserves_playback(self):
        blob,_=encode(frames(),forced_mode=4,key_interval=3,clean_alpha=False)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'clip.eaf4';path.write_bytes(blob)
            for file_path in (None,path):
                with Player(blob,path=file_path) as p:
                    self.assertEqual(p.seek(5),0)
                    LIB.esp_eaf_player_release_cache(p.handle)
                    self.assertEqual(p.seek(6),0);self.assertEqual(p.pixels(),frames()[6].tobytes())
                    self.assertEqual(p.seek(1),0);self.assertEqual(p.pixels(),frames()[1].tobytes())

    def test_inspector_rejects_experimental_ids_without_decoding(self):
        from tools.eaf_inspector.parser import parse_bytes,decode_block_payload
        for mode in (8,9,12,255):
            blob=classic_file([classic_frame([bytes([mode])+bytes(4)])])
            for verify in (False,True):
                with self.subTest(mode=mode,verify=verify):
                    report=parse_bytes(blob,verify_blocks=verify)
                    self.assertTrue(any(issue.severity=='error' and 'codec ID' in issue.message
                                        for issue in report.all_issues()))
            self.assertFalse(decode_block_payload(mode,bytes(8),4)[1])
        # Bounded is structurally inspected, never decoded as historical RAW.
        self.assertEqual(decode_block_payload(5,bytes(8),4),(0,None,[]))

    def test_singleton_huffman_and_trailing_configuration(self):
        palette=bytes([0,0,0,128])*256
        block=b'\3'+struct.pack('<H',3)+bytes([0,0,0])
        image=classic_frame([block,block],8,palette)
        blob=classic_file([image,image,b'_C'+bytes(16)])
        expected=bytes(16*8*2)+bytes([128])*(16*8)
        self.check_sequence(blob,[expected,expected])

    def test_header_validation(self):
        blob,_=encode_565(frames(),codec='lossless')
        for field,value,fmt in [(52,9,'B'),(53,7,'B'),(8,3,'H'),(12,0,'I')]:
            bad=bytearray(blob);struct.pack_into('<'+fmt,bad,field,value);bad[40:44]=bytes(4);struct.pack_into('<I',bad,40,zlib.crc32(bad[:64]))
            with self.assertRaises(ValueError):Player(bad)
        for count in (0,3,15,30):
            with self.assertRaises(ValueError):Player(blob[:count])

if __name__=='__main__':unittest.main(verbosity=2)
