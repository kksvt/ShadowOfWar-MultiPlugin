import ctypes as C
from ctypes import wintypes as W
import os
import struct

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
