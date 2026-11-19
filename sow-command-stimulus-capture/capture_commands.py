"""Read-only command stimulus capture. Windows x64 Python, standard library only."""
import argparse
import datetime as dt
import json
import math
import os
from pathlib import Path
import re
import struct
import time
from process_reader import WindowsProcess, record

QUERY=re.compile(r'perception query focus=(\d+) batch=(\d+) source=([0-9A-Fa-f]+) target=([0-9A-Fa-f]+)')
# The CircleTarget entry is patched by v10.20. Validate two inspected helpers
# that our plugin does not hook instead of rejecting its legitimate detour.
SIGNATURES={
    0x114A308:bytes.fromhex('33 c0 38 81 b9 00 00 00 0f 95 c0 c3'),
    0x190F1C:bytes.fromhex('48 8b 91 a8 26 00 00 33 c0 48 85 d2 74 08 38 82 a0 04 00 00 75 01 c3 b8 01 00 00 00 c3'),
}

def validate_build(process):
    for rva,expected in SIGNATURES.items():
        actual=process.read(process.base+rva,len(expected))
        if actual!=expected:
            observed=actual.hex(' ') if actual is not None else '<unreadable>'
            raise RuntimeError(f'Build check failed at RVA {rva:#x}: observed {observed}; expected {expected.hex(" ")}. Refusing unknown offsets.')

def actor_state(r,actor):
    stats=r.at(actor,0x26A8)
    out={'actor':hex(actor),'stats':hex(stats) if stats else stats,
         'identity_valid':bool(stats and r.at(stats,0x28)==actor)}
    out['health']=r.at(stats,0x42C,'f')
    if out['health'] is not None and not math.isfinite(out['health']):out['health']=None
    out['stats4A0']=r.at(stats,0x4A0,'B')
    for offset,fmt in [(0x2F19,'B'),(0x2F10,'B'),(0x2F0C,'I'),(0x2DD4,'B'),(0x2DD8,'f'),(0x2C68,'d'),(0x2BF8,'I'),
                       (0xE9,'B'),(0xEA,'B'),(0x2F18,'B'),(0x2F1B,'B'),(0x2F1C,'B'),(0x2F1E,'B'),(0x2E80,'B'),(0x2EB8,'Q')]:
        value=r.at(actor,offset,fmt)
        out[hex(offset)]=value if not isinstance(value,float) or math.isfinite(value) else None
    out['handles']={hex(off):r.at(actor,off) for off in (0x21B0,0x21D8)}
    ai=r.at(actor,0x2CE8)
    out['planner']={name:r.at(ai,off,fmt) for name,off,fmt in [('selectedStats',0x28,'Q'),('attackable',0xB9,'B'),('permission',0x710,'B'),
                         ('flag615',0x615,'B'),('flag648',0x648,'B'),('flag670',0x670,'B'),('stateDD4',0xDD4,'I')]}
    out['identity_stable']=stats is not None and r.at(actor,0x26A8)==stats and (not stats or r.at(stats,0x28)==actor)
    return out

def block(r,address,size):
    parts=[]
    for off in range(0,size,32768):
        chunk=r.read(address+off,min(32768,size-off))
        if chunk is None:return None
        parts.append(chunk)
    return b''.join(parts)

BUDGET_FIELDS={'MaxAmbientAI':1156809244,'MaxHighPriority':2796700546,
               'MaxMedPriority':3008847012,'MaxLowPriority':4199753612,
               'MaxSimulatedAnimations':134719747,'RunSimulation':2533195218}

def budget_fields(r,address):
    out={'status':'unreadable','fields':{}}
    flags=r.at(address,1,'B');count=r.number(address,'B')
    attrs=r.at(address,8);values=r.at(address,0x10)
    if None in (flags,count,attrs,values) or not attrs or not values:return out
    if flags&0x10:out['status']='unsupported-override-schema';return out
    raw=r.read(attrs,count*16)
    if raw is None:return out
    wanted={value:key for key,value in BUDGET_FIELDS.items()}
    for i in range(count):
        off=i*16;hashed=struct.unpack_from('<I',raw,off+(0 if flags&1 else 4))[0]
        if hashed not in wanted:continue
        index=i if flags&1 else raw[off+9]
        if index>=count:continue
        desc=raw[index*16:index*16+16];kind=desc[10]&0x3F
        size,start=struct.unpack_from('<HH',desc,12)
        item={'kind':kind,'count':size,'raw_u32':None}
        if size==1 and kind==3:item['raw_u32']=r.at(values,start*8,'I')
        out['fields'][wanted[hashed]]=item
    out['status']='best-effort' if r.at(address,8)==attrs and r.at(address,0x10)==values and r.read(attrs,count*16)==raw else 'changed-during-read'
    return out

def selection(r,base,actors):
    # 201DE8 returns this static object's address, NOT a pointer read from it.
    manager=base+0x2A62FF0
    raw=r.read(manager+0x188,256*0x30)
    config=r.at(manager,0x28)
    out={'manager':manager,'status':'unreadable','configuration':record(r,config),
         'runtime_configuration':budget_fields(r,config),
         'system_enabled':r.at(base,0x2678758,'B'),'actors':{}}
    if raw is None:return out
    for actor in actors:
        entries=[]
        for slot in range(256):
            row=raw[slot*48:slot*48+48]
            if struct.unpack_from('<Q',row)[0]!=actor:continue
            entries.append({'slot':slot,'address':manager+0x188+slot*48,
                            'requested_level':struct.unpack_from('<I',row,12)[0],
                            'native_slot':struct.unpack_from('<I',row,20)[0],
                            'suspended36':row[36],'flag37':row[37],'force_full38':row[38],
                            'raw':row.hex()})
        out['actors'][hex(actor)]=entries
    stable=r.at(manager,0x28)==config and r.read(manager+0x188,256*0x30)==raw
    out['status']='complete-best-effort' if stable else 'changed-during-read'
    return out

def registry(r,base,source,target,source_slot=None):
    manager=r.at(base,0x26FFC38)
    out={'status':'unreadable-manager','manager':manager,'matches':[]}
    if not manager:return out
    bounds=r.read(manager,16)
    if bounds is None:return out
    begin,end=struct.unpack('<QQ',bounds)
    out.update(begin=begin,end=end,status='invalid-bounds')
    if end<begin or (end-begin)%0xA0 or (end-begin)//0xA0>4096 or (end!=begin and not begin):return out
    raw=block(r,begin,end-begin)
    if raw is None:out['status']='unreadable-envelopes';return out
    out['count']=(end-begin)//0xA0
    unreadable=0
    for offset in range(0,len(raw),0xA0):
        env=raw[offset:offset+0xA0];stimulus=struct.unpack_from('<Q',env,0x90)[0]
        header=r.read(stimulus,0x90) if stimulus else None
        if header is None:unreadable+=1;continue
        primary,secondary=struct.unpack_from('<Q',header,0x48)[0],struct.unpack_from('<Q',header,0x70)[0]
        if source not in (primary,secondary) and target not in (primary,secondary):continue
        kind=header[0x88];effective=primary if kind==1 else secondary if kind==2 else None
        match={'stimulus':stimulus,'primary':primary,'secondary':secondary,'kind':kind,
               'effective':effective,'captain_match':effective==target,'source_match':effective==source,
               'record':record(r,struct.unpack_from('<Q',header,0x20)[0]),
               'deadline':struct.unpack_from('<d',env,0x80)[0],
               'interval':struct.unpack_from('<f',env,0x88)[0],
               'required_primary':bool(env[0x7D]),'required_secondary':bool(env[0x7E]),
               'priority':struct.unpack_from('<I',env,0x10)[0],
               'header_stable':r.read(stimulus,0x90)==header}
        if source_slot is not None and 0<=source_slot<256:
            cache=r.read(stimulus+0x90+source_slot*16,16)
            match['source_cache']=None if cache is None else dict(zip(('stamp','version','pending'),struct.unpack_from('<dIB',cache)))
        for key in ('deadline','interval'):
            if not math.isfinite(match[key]):match[key]=None
        if match.get('source_cache') and not math.isfinite(match['source_cache']['stamp']):match['source_cache']['stamp']=None
        out['matches'].append(match)
    stable=(r.at(base,0x26FFC38)==manager and r.read(manager,16)==bounds and block(r,begin,end-begin)==raw)
    out.update(unreadable=unreadable,vector_stable=stable)
    out['status']='changed-during-read' if not stable else 'incomplete' if unreadable or any(not m['header_stable'] for m in out['matches']) else 'complete-best-effort'
    # Even stable bounds/content cannot make an external multi-read snapshot atomic.
    return out

class LogTail:
    def __init__(self,path):
        self.path=Path(path);self.pending=b''
        try:self.offset=self.path.stat().st_size
        except FileNotFoundError:self.offset=0
    def poll(self):
        try:
            with self.path.open('rb') as f:
                size=f.seek(0,2)
                if self.offset is None or size<self.offset:
                    self.offset=max(0,size-262144);self.pending=b''
                    f.seek(self.offset)
                    if self.offset:f.readline();self.offset=f.tell()
                f.seek(self.offset);data=f.read(262144);self.offset=f.tell()
        except FileNotFoundError:return []
        lines=(self.pending+data).split(b'\n');self.pending=lines.pop()
        if len(self.pending)>16384:self.pending=b''
        return [line.decode('utf-8',errors='replace') for line in lines]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid',type=int)
    parser.add_argument('--log',type=Path,default=Path(os.environ.get('TEMP','.'))/'SoW-GruntCommand-active.log')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--interval',type=float,default=0.5)
    args=parser.parse_args()
    if not math.isfinite(args.interval) or not 0.2<=args.interval<=5:parser.error('--interval must be 0.2 through 5 seconds')
    process=WindowsProcess(args.pid)
    try:
        validate_build(process)
        stamp=dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
        path=args.output or Path(os.environ.get('LOCALAPPDATA','.'))/'SoWPlugin'/'Diagnostics'/f'CommandStimulus-{stamp}-{process.pid}.jsonl'
        path.parent.mkdir(parents=True,exist_ok=True)
        tail=LogTail(args.log);focus=None;last_seen=0;last_key=None;samples=0
        print('READ ONLY: no injection, hooks, remote calls or memory writes.')
        print('Watching:',args.log,'\nOutput:',path.resolve())
        print('Use Shift+X normally. Captures follow the focused source in the existing log. Ctrl+C stops.')
        with path.open('x',encoding='utf-8') as output:
            output.write(json.dumps({'type':'session','capture_version':'1.2','pid':process.pid,'base':hex(process.base),'read_only':True,'log':str(args.log),'interval':args.interval})+'\n');output.flush()
            while True:
                now=time.monotonic()
                for line in tail.poll():
                    match=QUERY.search(line)
                    if match:focus=tuple(int(v,10 if i<2 else 16) for i,v in enumerate(match.groups()));last_seen=now
                if focus and now-last_seen<=30:
                    epoch,batch,source,target=focus
                    src=actor_state(process,source);dst=actor_state(process,target)
                    manager=process.at(process.base,0x2702598);world=process.at(manager,8)
                    seconds=process.at(world,0x20,'d')
                    row={'type':'sample','utc':dt.datetime.now(dt.timezone.utc).isoformat(),
                         'monotonic':now,'focus':epoch,'batch':batch,'log_age_seconds':now-last_seen,
                         'world_seconds':seconds if seconds is not None and math.isfinite(seconds) else None,
                         'source':src,'target':dst,'registry':registry(process,process.base,source,target,src.get('0x2bf8')),
                         'selection':selection(process,process.base,(source,target))}
                    output.write(json.dumps(row,allow_nan=False)+'\n');output.flush();samples+=1
                    if focus!=last_key:
                        print(f'Following batch={batch} focus={epoch} source={source:X} target={target:X}');last_key=focus
                    if samples%20==0:print('Samples saved:',samples)
                time.sleep(args.interval)
    except KeyboardInterrupt:print('\nCapture stopped.')
    finally:process.close()

if __name__=='__main__':main()
