"""Read the loaded invasion opponent name. Windows x64, Python 3.9+, stdlib only.
No injection, native game calls or game-memory writes.
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
import unicodedata

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

# Steam 1.21 native invasion UI getter: 0x14188F3AC.
# It checks [GM+6D30]+47C, then passes inline UTF-16 at +340 to the
# UI truncation routine. Read the source, not its formatted/truncated output.
GETTER_RVA=0x188F3AC
GETTER=bytes.fromhex("48 83 ec 28 48 8b 05 f1 06 e7 00 48 8b 88 30 6d 00 00 32 c0 38 81 7c 04 00 00 74 0e 48 81 c1 40 03 00 00 e8 bc c9 47 ff b0 01 48 83 c4 28 c3")

def read_name(reader,address,units=128):
    raw=bytearray()
    for i in range(units):
        pair=reader.read(address+i*2,2)
        if pair is None:raise RuntimeError('Opponent name memory is unreadable; no name file was written.')
        if pair==b"\0\0":
            name=raw.decode('utf-16-le',errors='strict')
            if not name or any(unicodedata.category(c)=='Cc' for c in name):
                raise RuntimeError('Opponent name is empty or contains control characters; no name file was written.')
            return name,bytes(raw)
        raw.extend(pair)
    raise RuntimeError('Name has no terminator within the native 128-unit UI buffer bound.')

def capture(reader):
    if reader.read(reader.base+GETTER_RVA,len(GETTER))!=GETTER:
        raise RuntimeError('Unsupported executable: invasion name getter signature does not match Steam 1.21.')
    globals_ptr=reader.at(reader.base,0x26FFAA8)
    session=reader.at(globals_ptr,0x6D30)
    active=reader.at(session,0x47C,'B')
    if not globals_ptr or not session or not active:
        raise RuntimeError('No active invasion opponent name in the native UI source. Keep the conquest loaded and try again.')
    name,raw=read_name(reader,session+0x340)
    again,raw_again=read_name(reader,session+0x340)
    if (reader.at(reader.base,0x26FFAA8)!=globals_ptr or reader.at(globals_ptr,0x6D30)!=session or
        reader.at(session,0x47C,'B')!=active or name!=again or raw!=raw_again):
        raise RuntimeError('Online session/name changed during capture; retry without changing missions.')
    return {'utc':dt.datetime.now(dt.timezone.utc).isoformat(),'read_only':True,
        'source':'native invasion UI opponent-name source; untruncated by UI formatter',
        'pid':reader.pid,'module_base':hex(reader.base),'session':hex(session),
        'name_address':hex(session+0x340),'active_name_gate':active,'name':name,
        'utf16_le_hex':raw.hex(),'codepoints':['U+%04X'%ord(c) for c in name]}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pid',type=int,help='Optional ShadowOfWar.exe PID; otherwise auto-detected.')
    ap.add_argument('--output',type=Path,default=Path(__file__).with_name('defender-name.txt'),help='Exact UTF-8 name file; JSON evidence is written beside it.')
    args=ap.parse_args()
    reader=WindowsProcess(args.pid)
    try:result=capture(reader)
    finally:reader.close()
    output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    # No newline, stripping, normalization or replacement of Unicode characters.
    output.write_bytes(result['name'].encode('utf-8'))
    evidence=output.with_suffix(output.suffix+'.json')
    evidence.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Saved exact opponent name:',ascii(result['name']))
    print('Name file:',output);print('Evidence:',evidence)
    print('Keep both files. No game memory was changed.')

if __name__=='__main__':
    try:main()
    except (OSError,RuntimeError,UnicodeError) as exc:
        print('Capture failed:',str(exc),file=sys.stderr)
        if isinstance(exc,PermissionError):
            print('Run this script in your own Windows terminal outside Codex. If the game is elevated, use a terminal with matching elevation.',file=sys.stderr)
        sys.exit(1)
