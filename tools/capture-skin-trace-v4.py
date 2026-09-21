"""Opt-in bounded V4 capture. Writes diagnostic deadline only; no game inputs.

Writes .draw.bin and .cpu.bin packed record streams plus .json schema/summary.
CPU serial candidates are NOT validated object associations. Full native/solved/
child bones and layouts are retained for subsequent bind/palette investigation.
"""
import argparse
import collections
import ctypes as c
import json
from pathlib import Path
import struct
import time


def packed(name, fields):
    return type(name, (c.LittleEndianStructure,), {'_pack_': 4, '_fields_': fields})


U, Q, F = c.c_uint32, c.c_uint64, c.c_float
Bone = packed('Bone', [('position', F*3), ('positionW', F), ('quaternion', F*4), ('opaque', c.c_ubyte*16)])
CPU = packed('CPU', [('trace', c.c_ubyte*500)] + [(n,Q) for n in ('serial','qpc','solveId')]
    + [(n,U) for n in ('origin','thread','asset','sourceBuffer','count','childAsset','childBuffer','childCount','modes','center')]
    + [(n,Q) for n in ('nativeHash','solvedHash','childHash','layoutHash','bodyBefore','bodyAfter','visualBefore','visualAfter')]
    + [('rootWorldRaw',Bone),('childWorldBefore',Bone),('ids',U*64),('childIds',U*64),
       ('parents',c.c_int16*64),('childParents',c.c_int16*64),('original',Bone*64),('solved',Bone*64),('child',Bone*64)])
DRAW = packed('DRAW', [(n,U) for n in ('frame','worldFrame','skinFrame','vertexBuffer','indexBuffer','stride','count','flags',
    'worldBuffer','skinBuffer','vertexShader','pixelShader','ordinal','association','candidateCount','thread')]
    + [(n,U) for n in ('worldBytes','skinBytes','worldBindFlags','skinBindFlags')]
    + [(n,Q) for n in ('tick','cameraTick','worldSerial','skinSerial')]
    + [('cpuSerials',Q*16),('world',F*64),('skin',F*936),('vp',F*16)])
HEADER = packed('HEADER', [(n,U) for n in ('version','pid','drawStride','drawCapacity','published','dropped')]
    + [('until',Q)] + [(n,U) for n in ('cpuStride','cpuSlots','cpuDepth','cpuPublished')]+[('counters',U*32)])
CPU_COUNT, DRAW_COUNT = 64,128
CPU_OFFSET = c.sizeof(HEADER)
DRAW_OFFSET = CPU_OFFSET + CPU_COUNT*c.sizeof(CPU)
SIZE = DRAW_OFFSET + DRAW_COUNT*c.sizeof(DRAW)
COUNTERS = ('draws','deferred','missing_world','missing_skin','cpu_published','cpu_evicted','not_selected',
    'published','evicted','world_uploads','skin_uploads','pending_full','upload_budget','update_hook_calls','cpu_dropped','upload_outside_window','selected','world_unbound','skin_unbound','world_query_failed','skin_query_failed',
    'world_size_rejected','skin_size_rejected','watched_world','watched_skin','map_calls','map_unwatched','unmap_miss','copy_calls','copy_unwatched','world_type_rejected','skin_type_rejected')


def summary(draws, cpus):
    groups=collections.defaultdict(lambda:dict(draws=0,camera_matches=0,ambiguous=0,unassociated=0,missing_cpu_candidates=0,ordinals=[],world_payloads=0,skin_payloads=0,binding_sizes=[]))
    serials={CPU.from_buffer_copy(raw).serial for raw in cpus}
    for raw in draws:
        d=DRAW.from_buffer_copy(raw)
        key=f'{d.vertexBuffer:08x}/{d.indexBuffer:08x}/{d.vertexShader:08x}/{d.pixelShader:08x}/{d.count}'
        g=groups[key];g['draws']+=1;g['camera_matches']+=bool(d.flags&1)
        g['ambiguous']+=d.association==1;g['unassociated']+=d.association==0
        g['world_payloads']+=bool(d.flags&2);g['skin_payloads']+=bool(d.flags&4)
        sizes=[d.worldBytes,d.skinBytes]
        if sizes not in g['binding_sizes']:g['binding_sizes'].append(sizes)
        g['missing_cpu_candidates']+=sum(s not in serials for s in list(d.cpuSerials)[:min(d.candidateCount,16)])
        if d.ordinal not in g['ordinals']:g['ordinals'].append(d.ordinal)
    sources=[]
    for raw in cpus:
        r=CPU.from_buffer_copy(raw);trace=struct.unpack_from('<8I6Q',bytes(r.trace))
        sources.append(dict(serial=r.serial,solveId=r.solveId,origin={1:'remap',2:'getter'}.get(r.origin,'unknown'),
            frame=trace[1],stage=trace[2],flags=trace[3],root=trace[4],object=trace[5],slot_or_bone=trace[7],tick=trace[8],
            qpc=r.qpc,thread=r.thread,asset=r.asset,count=r.count,childCount=r.childCount,center=r.center,modes=r.modes,
            nativeHash=f'{r.nativeHash:016x}',solvedHash=f'{r.solvedHash:016x}',childHash=f'{r.childHash:016x}',layoutHash=f'{r.layoutHash:016x}',
            bodyBefore=r.bodyBefore,bodyAfter=r.bodyAfter,visualBefore=r.visualBefore,visualAfter=r.visualAfter,
            root_scale_raw_bytes=bytes(r.rootWorldRaw.opaque).hex()))
    return dict(draw_records=len(draws),cpu_records=len(cpus),candidate_draw_groups=dict(groups),cpu_evidence=sources,
        association='No verified player association: recent per-object CPU serials are candidates only. Bind inverses and native palette mapping still need validation.',
        limitations='Metadata-only records may have no matrices: flags 2=world payload, 4=skin payload, 1=camera match. Never interpret zero payload arrays as transforms. Rotating 24-draw windows (two frames each); 48 world and 48 skin upload reads/Present max. Missing/overwritten evidence is not a passing result. Capture overhead may affect timing. Raw scale bytes are not interpreted as unit scale.')


def self_test():
    assert (c.sizeof(Bone),c.sizeof(CPU),c.sizeof(DRAW),c.sizeof(HEADER))==(48,10708,4304,176)
    assert HEADER.until.offset==24 and DRAW.skin.offset==496
    a=CPU();a.serial=7;a.origin=1;a.count=3
    d=DRAW();d.candidateCount=1;d.cpuSerials[0]=7;d.ordinal=130
    result=summary([bytes(d)],[bytes(a)]);g=next(iter(result['candidate_draw_groups'].values()))
    assert g['unassociated']==1 and g['missing_cpu_candidates']==0 and g['ordinals']==[130]
    d.association=1;d.candidateCount=2;d.cpuSerials[1]=8
    g=next(iter(summary([bytes(d)],[bytes(a)])['candidate_draw_groups'].values()))
    assert g['ambiguous']==1 and g['missing_cpu_candidates']==1
    d.flags=2;d.worldBytes=256;d.skinBytes=0
    g=next(iter(summary([bytes(d)],[])['candidate_draw_groups'].values()))
    assert g['world_payloads']==1 and g['skin_payloads']==0 and g['binding_sizes']==[[256,0]]
    d.flags=0;d.skinBytes=4096
    g=next(iter(summary([bytes(d)],[])['candidate_draw_groups'].values()))
    assert g['world_payloads']==0 and g['skin_payloads']==0 and g['binding_sizes']==[[256,4096]]
    print('PASS: V4 binary layout, control offset, non-association, missing CPU evidence and metadata-only/world-only payload flags')


def capture(seconds, output):
    kernel=c.WinDLL('kernel32',use_last_error=True)
    for name,args,result in [('OpenFileMappingW',[U,c.c_int,c.c_wchar_p],c.c_void_p),
        ('OpenMutexW',[U,c.c_int,c.c_wchar_p],c.c_void_p),('MapViewOfFile',[c.c_void_p,U,U,U,c.c_size_t],c.c_void_p),
        ('WaitForSingleObject',[c.c_void_p,U],U),('ReleaseMutex',[c.c_void_p],c.c_int),
        ('UnmapViewOfFile',[c.c_void_p],c.c_int),('CloseHandle',[c.c_void_p],c.c_int),('GetTickCount64',[],Q)]:
        getattr(kernel,name).argtypes=args;getattr(kernel,name).restype=result
    mapping=kernel.OpenFileMappingW(6,False,'Local\\AmalurSkinTraceV4')
    mutex=kernel.OpenMutexW(0x100001,False,'Local\\AmalurSkinTraceMutexV4')
    memory=kernel.MapViewOfFile(mapping,6,0,0,SIZE) if mapping else None
    draws=[];cpus=[];seen=set();enabled=False;missed_draws=0
    try:
        if not memory or not mutex:raise RuntimeError('No V4 mapping. This reader cannot read body021/V2.')
        if kernel.WaitForSingleObject(mutex,1000) not in (0,128):raise RuntimeError('Diagnostic mutex unavailable')
        try:
            initial=HEADER.from_buffer_copy(c.string_at(memory,CPU_OFFSET))
            if (initial.version,initial.drawStride,initial.drawCapacity,initial.cpuStride,initial.cpuSlots,initial.cpuDepth)!=(4,4304,128,10708,16,4):
                raise RuntimeError('Unexpected V4 schema')
            if initial.until>kernel.GetTickCount64():raise RuntimeError('Another capture is active')
            # Ignore old retained histories from previous capture epochs.
            old=c.string_at(memory+CPU_OFFSET,CPU_COUNT*c.sizeof(CPU))
            seen={CPU.from_buffer_copy(old,i*c.sizeof(CPU)).serial for i in range(CPU_COUNT)}
            Q.from_address(memory+HEADER.until.offset).value=kernel.GetTickCount64()+int(seconds*1000)+500
            enabled=True;previous=initial.published;current=initial
        finally:kernel.ReleaseMutex(mutex)
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline and len(draws)<12000 and len(cpus)<12000:
            if kernel.WaitForSingleObject(mutex,0) in (0,128):
                try:
                    current=HEADER.from_buffer_copy(c.string_at(memory,CPU_OFFSET))
                    if current.pid!=initial.pid:raise RuntimeError('Writer process changed')
                    raw_cpu=c.string_at(memory+CPU_OFFSET,CPU_COUNT*c.sizeof(CPU))
                    delta=(current.published-previous)&0xffffffff;missed_draws+=max(0,delta-DRAW_COUNT)
                    for age in range(min(delta,DRAW_COUNT)-1,-1,-1):
                        index=((current.published-1-age)&0xffffffff)%DRAW_COUNT
                        draws.append(c.string_at(memory+DRAW_OFFSET+index*c.sizeof(DRAW),c.sizeof(DRAW)))
                    previous=current.published
                finally:kernel.ReleaseMutex(mutex)
                for i in range(CPU_COUNT):
                    raw=raw_cpu[i*c.sizeof(CPU):(i+1)*c.sizeof(CPU)];serial=CPU.from_buffer_copy(raw).serial
                    if serial and serial not in seen:seen.add(serial);cpus.append(raw)
            time.sleep(.02)
    finally:
        if enabled and kernel.WaitForSingleObject(mutex,1000) in (0,128):
            Q.from_address(memory+HEADER.until.offset).value=0;kernel.ReleaseMutex(mutex)
        if memory:kernel.UnmapViewOfFile(memory)
        if mapping:kernel.CloseHandle(mapping)
        if mutex:kernel.CloseHandle(mutex)
    output.parent.mkdir(parents=True,exist_ok=True)
    output.with_suffix('.draw.bin').write_bytes(b''.join(draws));output.with_suffix('.cpu.bin').write_bytes(cpus and b''.join(cpus) or b'')
    result=summary(draws,cpus)
    result.update(pid=initial.pid,missed_draw_records=missed_draws,
        missed_cpu_records=max(0,((current.cpuPublished-initial.cpuPublished)&0xffffffff)-len(cpus)),
        writer_drops=(current.dropped-initial.dropped)&0xffffffff,
        counters=dict(zip(COUNTERS,[(b-a)&0xffffffff for a,b in zip(initial.counters,current.counters)])),
        schema={kind:{n:dict(offset=getattr(t,n).offset,bytes=c.sizeof(ft)) for n,ft in t._fields_}
            for kind,t in [('cpu',CPU),('draw',DRAW)]})
    output.with_suffix('.json').write_text(json.dumps(result,indent=2))
    print(json.dumps({k:v for k,v in result.items() if k not in ('cpu_evidence','schema','candidate_draw_groups')},indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds',type=float,default=10)
    parser.add_argument('--output',type=Path,default=Path('skin-trace-v4'))
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:self_test()
    elif not .1<=args.seconds<=59:parser.error('--seconds must be between 0.1 and 59')
    else:capture(args.seconds,args.output)
