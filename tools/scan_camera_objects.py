"""Read-only x86 object scan: PID BASE [RTTI_JSON] [MAX_CANDIDATES]."""
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import struct
import sys

class Region(c.Structure):
    _fields_ = [('BaseAddress', c.c_void_p), ('AllocationBase', c.c_void_p),
                ('AllocationProtect', w.DWORD), ('PartitionId', w.WORD),
                ('RegionSize', c.c_size_t), ('State', w.DWORD),
                ('Protect', w.DWORD), ('Type', w.DWORD)]

k = c.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.argtypes = [w.DWORD,w.BOOL,w.DWORD]; k.OpenProcess.restype=c.c_void_p
k.VirtualQueryEx.argtypes=[c.c_void_p,c.c_void_p,c.POINTER(Region),c.c_size_t]; k.VirtualQueryEx.restype=c.c_size_t
k.ReadProcessMemory.argtypes=[c.c_void_p,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]; k.ReadProcessMemory.restype=w.BOOL
k.CloseHandle.argtypes=[c.c_void_p]
pid, base = int(sys.argv[1]), int(sys.argv[2],0)
handle=k.OpenProcess(0x410,False,pid) # QUERY_INFORMATION | VM_READ only
if not handle: raise c.WinError(c.get_last_error())
types=json.loads(Path(sys.argv[3] if len(sys.argv)>3 else 'research/camera-rtti.json').read_text(encoding='utf-8-sig'))
names={'.?AVCamera@BHG@@','.?AVStaticCameraImplementation@BHG@@',
       '.?AVFreeCameraControllerInstance@BHG@@','.?AVShoulderCameraControllerInstance@BHG@@'}
if len(sys.argv)>3:names={t['name'] for t in types if t['object_offset']==0}
needles=[(t['name'],struct.pack('<I',base+int(t['vtable_rva'],0))) for t in types if t['name'] in names and t['object_offset']==0]
results=[]; scanned=0
limit=int(sys.argv[4]) if len(sys.argv)>4 else 200
if limit<1:raise ValueError('MAX_CANDIDATES must be positive')
def read(address,size):
    buffer=c.create_string_buffer(size); got=c.c_size_t()
    k.ReadProcessMemory(handle,address,buffer,size,c.byref(got))
    return buffer.raw[:got.value]
try:
    address=0
    while address<0x100000000 and len(results)<limit:
        region=Region()
        if not k.VirtualQueryEx(handle,address,c.byref(region),c.sizeof(region)):break
        start=region.BaseAddress or 0; end=start+region.RegionSize
        if end<=address:break
        # Writable committed private memory only; avoids files and inaccessible pages.
        if region.State==0x1000 and region.Type==0x20000 and region.Protect&0xff in (4,8,0x40,0x80) and not region.Protect&0x100:
            for chunk in range(start,end,1024*1024):
                if len(results)>=limit:break
                content=read(chunk,min(1024*1024+3,end-chunk));scanned+=len(content)
                for name,needle in needles:
                    if len(results)>=limit:break
                    offset=0
                    while (offset:=content.find(needle,offset))>=0:
                        if len(results)>=limit:break
                        candidate=chunk+offset
                        if candidate%4==0:
                            raw=read(candidate,0x400)
                            results.append({'type':name,'address':hex(candidate),'bytes_hex':raw.hex()})
                        offset+=4
        address=end
finally:k.CloseHandle(handle)
print(json.dumps({'pid':pid,'exe_base':hex(base),'bytes_scanned':scanned,'candidates':results},indent=2))
