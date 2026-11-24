#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
python3 - "$repo_root" <<'PY'
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib, os, re, subprocess, sys, zlib, struct

root=Path(sys.argv[1])
out=root/'.local/ui-runtime-test'
obj=out/'obj'
frames=out/'frames'
obj.mkdir(parents=True,exist_ok=True)
frames.mkdir(parents=True,exist_ok=True)
config=(root/'Arduino/libraries/lv_conf.h').read_text()
# Host supplies the clock; fail promptly on allocation assertions.
config,n=re.subn(r'#define LV_TICK_CUSTOM 1\b','#define LV_TICK_CUSTOM 0',config)
assert n==1,'LVGL tick configuration changed'
config=config.replace('#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>','#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>')
config=config.replace('#define LV_ASSERT_HANDLER while(1);','#define LV_ASSERT_HANDLER abort();')
(out/'lv_conf.h').write_text(config)
includes=['-I'+str(out),'-I'+str(root/'Arduino/libraries/lvgl'),'-I'+str(root/'Arduino/libraries/ui/src')]
flags=['-g','-fsanitize=address','-O0','-D_DEFAULT_SOURCE','-DLV_CONF_INCLUDE_SIMPLE']
sources=sorted((root/'Arduino/libraries/lvgl/src').rglob('*.c'))+sorted((root/'Arduino/libraries/ui/src').rglob('*.c'))
headers=hashlib.sha256(config.encode())
for folder in ('Arduino/libraries/lvgl','Arduino/libraries/ui/src'):
    for header in sorted((root/folder).rglob('*.h')):
        headers.update(str(header.relative_to(root)).encode()+b'\0'+header.read_bytes())
header_digest=headers.digest()
def compile_source(source):
    target=obj/(str(source.relative_to(root)).replace('/','__')+'.o')
    stamp=target.with_suffix('.sha256')
    digest=hashlib.sha256(header_digest+source.read_bytes()+repr(flags).encode()).hexdigest()
    if not target.exists() or not stamp.exists() or stamp.read_text()!=digest:
        subprocess.run([os.environ.get('CC','gcc'),*flags,'-std=c99',*includes,'-c',str(source),'-o',str(target)],check=True)
        stamp.write_text(digest)
    return str(target)
with ThreadPoolExecutor(max_workers=int(os.environ.get('JOBS','2'))) as pool:
    objects=list(pool.map(compile_source,sources))
binary=out/'ui_runtime_test'
subprocess.run([os.environ.get('CXX','g++'),*flags,'-std=c++17',*includes,'-I'+str(root/'Arduino/PillDispenser'),str(root/'tests/ui_runtime_test.cpp'),*objects,'-lm','-pthread','-o',str(binary)],check=True)
environment=os.environ.copy()
environment['ASAN_OPTIONS']='detect_leaks=0'
subprocess.run([str(binary),str(frames)],check=True,env=environment)
# PPM to PNG using only Python's standard library.
def chunk(kind,data):
    return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
for ppm in sorted(frames.glob('*.ppm')):
    magic,size,maximum,pixels=ppm.read_bytes().split(b'\n',3)
    assert magic==b'P6' and maximum==b'255'
    width,height=map(int,size.split())
    assert len(pixels)==width*height*3
    rows=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b'')
    ppm.with_suffix('.png').write_bytes(png)
print('Runtime screenshots:',frames)
PY
