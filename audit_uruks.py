"""Read-only Shadow of War member audit. Windows x64, Python 3.9+, stdlib only.
No injection, hooks, remote calls, or process-memory writes.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import datetime as dt
import json
import os
from pathlib import Path
import struct
import sys
import time

CATALOG_PATH = Path(__file__).with_name('audit_catalog.json')
RESOLVER_SIGNATURE = bytes.fromhex('48 83 ec 28 83 7a 08 ff 4c 8b d2 75')
MAX_TRAITS = 512

class Reader:
    def read(self, address, size):
        raise NotImplementedError
    def number(self, address, fmt='Q'):
        raw = self.read(address, struct.calcsize('<'+fmt)) if address else None
        return struct.unpack('<'+fmt, raw)[0] if raw is not None else None
    def at(self, base, offset, fmt='Q'):
        return self.number(base+offset, fmt) if base else None
    def cstring(self, address, limit=192):
        if not address:
            return None
        result=bytearray()
        # Byte reads avoid crossing a readable page boundary after a terminator.
        for offset in range(limit):
            raw=self.read(address+offset,1)
            if raw is None:return None
            if raw==b'\0':
                try:return result.decode('utf-8')
                except UnicodeDecodeError:return None
            result.extend(raw)
        return None

def record(reader, address):
    result={'address':hex(address) if address else None,'name':None,'status':'unreadable-pointer' if address is None else 'null'}
    if not address:return result
    flags=reader.at(address,1,'B')
    if flags is None:result['status']='unreadable-record';return result
    if not flags & 2:result['status']='name-flag-absent';return result
    name_address=reader.at(address,0x20)
    name=reader.cstring(name_address)
    result.update(name=name,status='named' if name else 'unreadable-or-incomplete-name')
    return result

def record_field(reader, base, offset):
    return record(reader,reader.at(base,offset))

def vector(reader, member, offset):
    begin,end=reader.at(member,offset),reader.at(member,offset+8)
    result={'offset':hex(offset),'status':'unreadable-bounds','count':None,'records':[]}
    if begin is None or end is None:return result
    if begin==end==0:result.update(status='complete',count=0);return result
    if not begin or end<begin or (end-begin)%8 or (end-begin)//8>MAX_TRAITS:
        result['status']='invalid-or-over-bound';return result
    count=(end-begin)//8
    raw=reader.read(begin,count*8)
    if raw is None:result['status']='unreadable-vector';return result
    entries=struct.unpack('<'+'Q'*count,raw) if count else ()
    result.update(count=count,records=[record(reader,p) for p in entries])
    # Revalidate the vector bounds; changes invalidate completeness.
    stable=reader.at(member,offset)==begin and reader.at(member,offset+8)==end
    result['status']='complete' if stable and all(r['status']=='named' for r in result['records']) else 'changed-or-incomplete'
    return result

def resolve_reference(reader,manager,key,index):
    """Read-only equivalent of the inspected roster lookup, with conservative bounds.
    Linear bucket scan avoids calling the game's record-index/hash helpers.
    """
    if not manager or not key or index is None or index==0xffffffff or index>8191:return None
    table=manager+0x98
    owner=reader.at(table,0x10)
    if not owner or reader.at(key,0x18)!=owner:return None
    slots=reader.at(table,0)
    count=reader.at(table,8,'I')
    if not slots or count is None or count>4096:return None
    found=[]
    for slot in range(count):
        entry=slots+slot*0x290
        if reader.number(entry)!=key:continue
        # Native 18DE04 returns entry+8; 18DDD4 follows that object's +20.
        pool=reader.at(entry+8,0x20)
        begin,end=reader.at(pool,0x18),reader.at(pool,0x20)
        if begin is None or end is None or not begin or end<begin or (end-begin)%8:continue
        size=(end-begin)//8
        if size>8192 or index>=size:continue
        member=reader.number(begin+index*8)
        if member and reader.at(member,0x178)==key:found.append(member)
    unique=set(found)
    return next(iter(unique)) if len(unique)==1 else None

def hierarchy_member(reader,base,manager,selection):
    """Native UI 188CE30 -> 16E128: faction + byte row + byte seat.
    Only the inspected ordinary record-array representation is supported.
    Special/overridden schemas fail closed instead of guessing row order.
    """
    key=reader.number(selection);row=reader.at(selection,8,'B');seat=reader.at(selection,9,'B')
    diag={'faction':hex(key) if key else None,'row':row,'seat':seat,'status':'unavailable'}
    if reader.read(base+0x188CE30,15)!=bytes.fromhex('48 8b 05 d1 2c e7 00 48 83 78 10 00 74 1e 48'):
        diag['status']='hierarchy-signature-mismatch';return None,diag
    controller=reader.at(base,0x26FFB08)
    gate=reader.at(controller,0x10)
    diag['ui_gate']=hex(gate) if gate is not None else None
    if not gate:diag['status']='ui-gate-unavailable';return None,diag
    if not key or row is None or seat is None:return None,diag
    settings=reader.at(base,0x266B6F0)
    mode=reader.at(settings,0x420,'i');attr_hash=reader.at(settings,0x410,'I')
    diag['schema_mode']=mode
    if mode is None or mode>=2:
        diag['status']='alternate-row-schema-not-supported';return None,diag
    flags=reader.at(key,1,'B');count=reader.number(key,'B');attrs=reader.at(key,8)
    if flags is None or count is None or not attrs or attr_hash is None:return None,diag
    # 25180C / 251BD0: ordinary descriptor lookup uses sorted hash at +4
    # and the original attribute index at +9. Dynamic schema uses +0.
    descriptors=[]
    for n in range(count):
        entry=attrs+n*16
        if reader.at(entry,0 if flags&1 else 4,'I')==attr_hash:
            index=n if flags&1 else reader.at(entry,9,'B')
            if index is not None and index<count:descriptors.append(attrs+index*16)
    if len(set(descriptors))!=1:
        diag['status']='row-attribute-not-unique';return None,diag
    desc=descriptors[0];kind=reader.at(desc,0xA,'B');size=reader.at(desc,0xC,'H');start=reader.at(desc,0xE,'H')
    if kind is None or kind&0x3F!=10 or size is None or row>=size or flags&0x10 or start is None:
        diag['status']='unsupported-or-out-of-bounds-row-array';return None,diag
    values=reader.at(key,0x10);row_record=reader.at(values,(start+row)*8)
    diag['row_record']=hex(row_record) if row_record else None
    # 18DE04 and 1E4CD8: exact owner/key match, then row-record match in
    # bucket+8's 0x28-byte rows. Seat stores a u32 roster index.
    owner=reader.at(manager,0xA8);slots=reader.at(manager,0x98);buckets=reader.at(manager,0xA0,'I')
    if not owner or reader.at(key,0x18)!=owner or not slots or buckets is None or buckets>4096 or not row_record:
        diag['status']='faction-table-or-owner-invalid';return None,diag
    matches=[]
    for n in range(buckets):
        entry=slots+n*0x290
        if reader.number(entry)!=key:continue
        bucket=entry+8;begin=reader.at(bucket,0xF8);end=reader.at(bucket,0x100)
        if not begin or end is None or end<begin or (end-begin)%0x28 or (end-begin)//0x28>256:continue
        rows=reader.read(begin,end-begin)
        if rows is None:continue
        for pos in range(0,len(rows),0x28):
            if struct.unpack_from('<Q',rows,pos)[0]!=row_record:continue
            seats,last=struct.unpack_from('<QQ',rows,pos+8)
            if not seats or last<seats or (last-seats)%4 or (last-seats)//4>8192 or seat>=(last-seats)//4:continue
            index=reader.at(seats,seat*4,'I')
            member=resolve_reference(reader,manager,key,index)
            if member and reader.at(member,0x268)==row_record and reader.read(begin,end-begin)==rows and reader.at(seats,seat*4,'I')==index:
                matches.append(member)
        if reader.at(bucket,0xF8)!=begin or reader.at(bucket,0x100)!=end:
            diag['status']='rows-changed-during-read';return None,diag
    unique=set(matches)
    diag['status']='hierarchy-seat-resolved' if len(unique)==1 else 'no-unique-hierarchy-seat'
    return (next(iter(unique)) if len(unique)==1 else None),diag


def selected_member(reader,base):
    if reader.read(base+0x18D93C,len(RESOLVER_SIGNATURE))!=RESOLVER_SIGNATURE:
        return {'status':'resolver-signature-mismatch','member':None}
    globals_address=reader.at(base,0x26FFAA8)
    manager=reader.at(globals_address,0x6C68)
    selection=reader.at(globals_address,0x6C80)
    if not selection or not manager:return {'status':'selection-or-manager-unavailable','member':None}
    before=reader.read(selection,0x39)
    if before is None:return {'status':'selection-unreadable','member':None}
    if before==bytes(0x39):
        if reader.read(selection,0x39)!=before:
            return {'status':'selection-changed-during-read','member':None}
        return {'status':'global-selection-empty','member':None,'selection_kind':0,
                'selection_address':hex(selection),
                'next_step':'This menu has not populated this selection source. Use --member with the current CE Uruk pointer; do not interpret this as a captain validation result.'}
    matches=[]
    diagnostic={'selection_address':hex(selection),'manager_address':hex(manager),
                'raw_selection':before.hex(' '),'references':[]}
    # Native 16BCC0 copies this UI selection value. Inspect its two reference
    # positions; neither is accepted without an exact roster-key/owner/member match.
    for offset in (0,0x28):
        key=reader.at(selection,offset)
        index=reader.at(selection,offset+8,'I')
        member=resolve_reference(reader,manager,key,index)
        diagnostic['references'].append({'offset':hex(offset),'key':hex(key) if key else None,'index':index,'resolved':bool(member)})
        if member:matches.append({'member':member,'reference_offset':hex(offset),'index':index,'key':hex(key)})
    member,hierarchy=hierarchy_member(reader,base,manager,selection)
    diagnostic['hierarchy']=hierarchy
    if member:matches.append({'member':member,'lookup':'hierarchy-faction-row-seat'})
    if reader.read(selection,0x39)!=before:return {'status':'selection-changed-during-read','member':None}
    unique={m['member'] for m in matches}
    if len(unique)!=1:return {'status':'no-unique-resolved-member','member':None,'selection_kind':before[0x20],'diagnostic':diagnostic}
    result=next(m for m in matches if m['member'] in unique)
    result.update(status='member-resolved',selection_kind=before[0x20],diagnostic=diagnostic)
    return result

def snapshot(reader,member,catalog,source):
    identity_before=reader.read(member+0x168,0x18) if member else None
    fields={name:record_field(reader,member,offset) for name,offset in {
        'faction':0x178,'ai_combat_tree':0x188,'preset':0x190,'personality':0x270,
        'class_character_model':0x278,'role':0x318,'tribe':0x328,'location':0x330,
        'row_seat_record':0x268}.items()}
    storage=reader.at(member,0x2D8)
    fields.update({name:record_field(reader,storage,offset) for name,offset in {
        'visual_base_model':0x20,'visual_body_model':0x28,'visual_head_model':0x30}.items()})
    traits=vector(reader,member,0x198)
    markers=vector(reader,member,0x1B0)
    level=reader.at(member,0x4B8,'I')
    identity=bool(member and fields['faction']['status']=='named' and level is not None and 0<=level<=65535 and
                  any(fields[name]['status']=='named' for name in ['role','class_character_model','ai_combat_tree']))
    result={'utc':dt.datetime.now(dt.timezone.utc).isoformat(),'read_only':True,'source':source,
            'member':hex(member),'identity_plausible':identity,'scope':'unverified: not automatically proven to be an imported defender or Overlord',
            'level_raw':level,'level_display':level+1 if level is not None else None,
            'name_hash':reader.at(member,0x170,'I'),'fields':fields,
            'traits':traits,'markers':markers,'findings':[]}
    unknown=[key for key,value in fields.items() if value['status'] not in ('named','null')]
    result['incomplete_fields']=unknown
    names={r['name'] for v in (traits,markers) for r in v['records'] if r['status']=='named'}
    complete=traits['status']==markers['status']=='complete'
    for label,matched in [('fearless',set(catalog['fearless']) & names),('overwhelming_awe',{catalog['overwhelming_awe']['record']} & names)]:
        result[label]={'state':'present' if matched else 'absent' if complete else 'unknown','records':sorted(matched)}
    if identity_before is None or reader.read(member+0x168,0x18)!=identity_before:
        identity=False;result['identity_plausible']=False
        result['consistency']='identity-unreadable-or-changed-during-read'
    else:result['consistency']='identity-token-stable-best-effort'
    if not identity:
        result['assessment']='unknown-member-identity'
        return result
    def finding(code,field,reason):result['findings'].append({'code':code,'field':field,'reason':reason})
    tribe=fields['tribe']
    if tribe['status']=='null' or tribe['name']=='TribeDef_Tribeless':
        finding('missing-or-explicitly-tribeless','tribe','Structural concern for an Overlord; not independently proof of cheating or the reported throne-room failure.')
    category_by_field={'ai_combat_tree':'Combat/Tree','class_character_model':'Character/Models','role':'Faction/Roles/Definitions',
                       'visual_base_model':'Model','visual_body_model':'Model','visual_head_model':'Model'}
    for field,category in category_by_field.items():
        if fields[field]['name'] in catalog['special_character_records'].get(category,[]):
            finding('special-character-record',field,'Exact special/story-character identifier; incompatible with an ordinary Orc/Olog defender unless context explains it. Does not establish invulnerability.')
    foreign=set(n for group in catalog['foreign_traits'].values() for n in group)
    for name in sorted(names & foreign):
        finding('mercenary-context-trait',name,'Exact identifier from the DLC mercenary trait/marker family. Review in verified Orc/Olog conquest context; not a global blacklist.')
    result['assessment']='review-required' if result['findings'] else 'no-listed-signal-observed' if not unknown and complete else 'incomplete-no-listed-signal-observed'
    result['note']='No definitive cheat verdict. Ordinary immunity combinations, high level, cosmetics and Quirk_Fearless alone are not listed signals.'
    return result

class WindowsProcess(Reader):
    def __init__(self,pid=None):
        if os.name!='nt' or C.sizeof(C.c_void_p)!=8:raise RuntimeError('Requires Windows with 64-bit Python.')
        self.k=C.WinDLL('kernel32',use_last_error=True)
        self.u=C.WinDLL('user32',use_last_error=True)
        self.k.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];self.k.CreateToolhelp32Snapshot.restype=W.HANDLE
        self.k.CloseHandle.argtypes=[W.HANDLE];self.k.CloseHandle.restype=W.BOOL
        self.k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];self.k.OpenProcess.restype=W.HANDLE
        self.k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];self.k.ReadProcessMemory.restype=W.BOOL
        class ProcessEntry(C.Structure):
            _fields_=[('dwSize',W.DWORD),('cntUsage',W.DWORD),('pid',W.DWORD),('heap',C.c_size_t),('module',W.DWORD),('threads',W.DWORD),('parent',W.DWORD),('priority',W.LONG),('flags',W.DWORD),('exe',W.WCHAR*260)]
        class ModuleEntry(C.Structure):
            _fields_=[('dwSize',W.DWORD),('moduleId',W.DWORD),('pid',W.DWORD),('globalUsage',W.DWORD),('processUsage',W.DWORD),('base',C.c_void_p),('size',W.DWORD),('module',W.HMODULE),('name',W.WCHAR*256),('path',W.WCHAR*260)]
        self.k.Process32FirstW.argtypes=[W.HANDLE,C.POINTER(ProcessEntry)];self.k.Process32FirstW.restype=W.BOOL
        self.k.Process32NextW.argtypes=self.k.Process32FirstW.argtypes;self.k.Process32NextW.restype=W.BOOL
        self.k.Module32FirstW.argtypes=[W.HANDLE,C.POINTER(ModuleEntry)];self.k.Module32FirstW.restype=W.BOOL
        self.u.GetForegroundWindow.restype=W.HWND
        self.u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
        self.u.GetAsyncKeyState.argtypes=[C.c_int];self.u.GetAsyncKeyState.restype=C.c_short
        if pid is None:
            handle=self.k.CreateToolhelp32Snapshot(2,0)
            if handle==C.c_void_p(-1).value:raise C.WinError(C.get_last_error())
            try:
                entry=ProcessEntry();entry.dwSize=C.sizeof(entry);matches=[]
                more=self.k.Process32FirstW(handle,C.byref(entry))
                while more:
                    if entry.exe.casefold()=='shadowofwar.exe':matches.append(entry.pid)
                    more=self.k.Process32NextW(handle,C.byref(entry))
            finally:self.k.CloseHandle(handle)
            if len(matches)!=1:raise RuntimeError('Start the game first, or use --pid if several instances exist.')
            pid=matches[0]
        self.pid=pid
        self.handle=self.k.OpenProcess(0x0010|0x1000,False,pid) # VM_READ | QUERY_LIMITED_INFORMATION only
        if not self.handle:raise C.WinError(C.get_last_error())
        try:
            handle=self.k.CreateToolhelp32Snapshot(0x08|0x10,pid)
            if handle==C.c_void_p(-1).value:raise C.WinError(C.get_last_error())
            try:
                entry=ModuleEntry();entry.dwSize=C.sizeof(entry)
                if not self.k.Module32FirstW(handle,C.byref(entry)):raise C.WinError(C.get_last_error())
                if entry.name.casefold()!='shadowofwar.exe':raise RuntimeError('The selected PID is not ShadowOfWar.exe.')
                self.base=entry.base
            finally:self.k.CloseHandle(handle)
        except Exception:
            self.close();raise
    def read(self,address,size):
        if not address or size<0 or size>65536:return None
        if size==0:return b''
        buffer=C.create_string_buffer(size);read=C.c_size_t()
        ok=self.k.ReadProcessMemory(self.handle,C.c_void_p(address),buffer,size,C.byref(read))
        return buffer.raw if ok and read.value==size else None
    def foreground(self):
        pid=W.DWORD();self.u.GetWindowThreadProcessId(self.u.GetForegroundWindow(),C.byref(pid));return pid.value==self.pid
    def close(self):
        if self.handle:self.k.CloseHandle(self.handle);self.handle=None

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid',type=int)
    parser.add_argument('--member',type=lambda s:int(s,0),help='Exact CE Uruk/member address, e.g. 0x12345678; bypass UI selection discovery.')
    parser.add_argument('--once',action='store_true',help='Read immediately instead of waiting for F8.')
    parser.add_argument('--label',default='',help='User description of this comparison sample, not trusted classification.')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.member is not None and args.member<=0:parser.error('--member must be a positive address')
    catalog=json.loads(CATALOG_PATH.read_text(encoding='utf-8'))
    process=WindowsProcess(args.pid)
    path=args.output or Path(os.environ.get('TEMP','.'))/('SoW-UrukAudit-'+dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.jsonl')
    try:
        print('Read-only audit v2 (hierarchy seat lookup). No DLL changes, game-memory writes, repair, or native calls.')
        print('PID:',process.pid,'module:',hex(process.base),'output:',path)
        def capture():
            selection={'status':'explicit-CE-member-address','member':args.member} if args.member else selected_member(process,process.base)
            member=selection.get('member')
            if member:
                result=snapshot(process,member,catalog,selection['status'])
            else:result={'utc':dt.datetime.now(dt.timezone.utc).isoformat(),'read_only':True,'assessment':'selection-unresolved','selection':selection}
            result.update(pid=process.pid,module_base=hex(process.base),user_label=args.label,selection=selection)
            with path.open('a',encoding='utf-8') as file:file.write(json.dumps(result,ensure_ascii=True)+'\n')
            print(json.dumps(result,indent=2,ensure_ascii=True),flush=True)
        if args.once:capture();return
        print('Open an Overlord/captain detail card in the siege overview, then press F8 while the game is focused. Ctrl+C exits.')
        previous=False
        while True:
            down=bool(process.u.GetAsyncKeyState(0x77)&0x8000)
            if down and not previous and process.foreground():capture()
            previous=down
            time.sleep(.05)
    finally:process.close()

if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:pass
    except (OSError,RuntimeError,ValueError) as error:
        print('Audit error:',error,file=sys.stderr);sys.exit(1)
