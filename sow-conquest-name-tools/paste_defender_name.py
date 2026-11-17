"""Paste a saved Unicode defender name into the focused SoW search box.
No game-memory access; no automatic Enter/search submission. Windows, stdlib only.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import os
from pathlib import Path
import sys
import time
import unicodedata

class MOUSEINPUT(C.Structure):
    _fields_=[('dx',W.LONG),('dy',W.LONG),('mouseData',W.DWORD),('dwFlags',W.DWORD),('time',W.DWORD),('dwExtraInfo',C.c_size_t)]
class KEYBDINPUT(C.Structure):
    _fields_=[('wVk',W.WORD),('wScan',W.WORD),('dwFlags',W.DWORD),('time',W.DWORD),('dwExtraInfo',C.c_size_t)]
class HARDWAREINPUT(C.Structure):
    _fields_=[('uMsg',W.DWORD),('wParamL',W.WORD),('wParamH',W.WORD)]
class INPUTUNION(C.Union):
    _fields_=[('mi',MOUSEINPUT),('ki',KEYBDINPUT),('hi',HARDWAREINPUT)]
class INPUT(C.Structure):
    _anonymous_=('u',)
    _fields_=[('type',W.DWORD),('u',INPUTUNION)]

def load_name(path):
    # Read exactly the UTF-8 bytes written by the dump script; preserve whitespace.
    name=path.read_bytes().decode('utf-8')
    if not name or len(name.encode('utf-16-le'))>256 or any(unicodedata.category(c)=='Cc' for c in name):
        raise ValueError('Name file must contain only the name, up to 128 UTF-16 units, without a newline/control characters.')
    return name

def key(vk=0,scan=0,flags=0):
    result=INPUT();result.type=1;result.ki=KEYBDINPUT(vk,scan,flags,0,0);return result

def unicode_events(name):
    raw=name.encode('utf-16-le');events=[]
    for i in range(0,len(raw),2):
        unit=int.from_bytes(raw[i:i+2],'little');events.extend([key(scan=unit,flags=4),key(scan=unit,flags=6)])
    return events

def shortcut(vk):return [key(0x11),key(vk),key(vk,flags=2),key(0x11,flags=2)]

class Windows:
    def __init__(self):
        if os.name!='nt':raise RuntimeError('Windows is required.')
        self.u=C.WinDLL('user32',use_last_error=True);self.k=C.WinDLL('kernel32',use_last_error=True)
        self.u.GetForegroundWindow.restype=W.HWND
        self.u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
        self.u.GetAsyncKeyState.argtypes=[C.c_int];self.u.GetAsyncKeyState.restype=C.c_short
        self.u.SendInput.argtypes=[W.UINT,C.POINTER(INPUT),C.c_int];self.u.SendInput.restype=W.UINT
        self.k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];self.k.OpenProcess.restype=W.HANDLE
        self.k.CloseHandle.argtypes=[W.HANDLE];self.k.CloseHandle.restype=W.BOOL
        self.k.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,C.POINTER(W.DWORD)]
        self.k.QueryFullProcessImageNameW.restype=W.BOOL
    def down(self,vk):return bool(self.u.GetAsyncKeyState(vk)&0x8000)
    def game_window(self):
        hwnd=self.u.GetForegroundWindow();pid=W.DWORD();self.u.GetWindowThreadProcessId(hwnd,C.byref(pid))
        handle=self.k.OpenProcess(0x1000,False,pid.value)
        if not handle:return None
        try:
            text=C.create_unicode_buffer(32768);length=W.DWORD(len(text))
            if not self.k.QueryFullProcessImageNameW(handle,0,text,C.byref(length)):return None
            return (hwnd,pid.value) if Path(text.value).name.casefold()=='shadowofwar.exe' else None
        finally:self.k.CloseHandle(handle)
    def send(self,events,target):
        if self.game_window()!=target:raise RuntimeError('Game focus changed; input cancelled.')
        array=(INPUT*len(events))(*events)
        sent=self.u.SendInput(len(array),array,C.sizeof(INPUT))
        if sent!=len(array):
            # Release shortcut modifiers if the OS accepted only a partial batch.
            release=(INPUT*2)(key(0x11,flags=2),key(0x56,flags=2))
            self.u.SendInput(2,release,C.sizeof(INPUT))
            raise RuntimeError('Windows did not accept all input. Use matching elevation if the game is elevated; inspect the field before retrying.')
    def clipboard(self,name):
        u,k=self.u,self.k
        u.CreateWindowExW.argtypes=[W.DWORD,W.LPCWSTR,W.LPCWSTR,W.DWORD,C.c_int,C.c_int,C.c_int,C.c_int,W.HWND,W.HMENU,W.HINSTANCE,C.c_void_p];u.CreateWindowExW.restype=W.HWND
        u.DestroyWindow.argtypes=[W.HWND];u.DestroyWindow.restype=W.BOOL
        u.OpenClipboard.argtypes=[W.HWND];u.OpenClipboard.restype=W.BOOL
        u.EmptyClipboard.restype=W.BOOL;u.CloseClipboard.restype=W.BOOL
        u.SetClipboardData.argtypes=[W.UINT,W.HANDLE];u.SetClipboardData.restype=W.HANDLE
        k.GlobalAlloc.argtypes=[W.UINT,C.c_size_t];k.GlobalAlloc.restype=W.HGLOBAL
        k.GlobalLock.argtypes=[W.HGLOBAL];k.GlobalLock.restype=C.c_void_p
        k.GlobalUnlock.argtypes=[W.HGLOBAL];k.GlobalUnlock.restype=W.BOOL
        k.GlobalFree.argtypes=[W.HGLOBAL];k.GlobalFree.restype=W.HGLOBAL
        owner=u.CreateWindowExW(0,'STATIC','SoW name clipboard',0,0,0,0,0,W.HWND(-3),None,None,None)
        if not owner:raise C.WinError(C.get_last_error())
        handle=None;opened=False
        try:
            for _ in range(20):
                if u.OpenClipboard(owner):opened=True;break
                time.sleep(.05)
            if not opened:raise RuntimeError('Clipboard is busy; try again.')
            raw=name.encode('utf-16-le')+b'\0\0';handle=k.GlobalAlloc(2,len(raw))
            if not handle:raise C.WinError(C.get_last_error())
            pointer=k.GlobalLock(handle)
            if not pointer:raise C.WinError(C.get_last_error())
            try:C.memmove(pointer,raw,len(raw))
            finally:k.GlobalUnlock(handle)
            if not u.EmptyClipboard() or not u.SetClipboardData(13,handle):raise C.WinError(C.get_last_error())
            handle=None # Windows owns the allocation after SetClipboardData succeeds.
        finally:
            if opened:u.CloseClipboard()
            if handle:k.GlobalFree(handle)
            u.DestroyWindow(owner)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('file',type=Path,nargs='?',default=Path(__file__).with_name('defender-name.txt'))
    ap.add_argument('--method',choices=('clipboard','unicode'),default='clipboard',help='Default: Unicode clipboard + Ctrl+V. If the game ignores paste, retry with unicode input.')
    ap.add_argument('--replace',action='store_true',help='Send Ctrl+A before pasting; otherwise appends at the caret.')
    ap.add_argument('--clipboard-only',action='store_true',help='Copy name to clipboard, then exit without sending input.')
    args=ap.parse_args();name=load_name(args.file);win=Windows()
    if args.method=='clipboard' or args.clipboard_only:win.clipboard(name)
    print('Loaded exact name:',ascii(name))
    if args.clipboard_only:print('Copied to the Unicode clipboard.');return
    print('Open Friendly Conquest, click the player-name search box, then press and RELEASE F10.')
    print('The script checks that ShadowOfWar.exe is focused. It does not submit the search. Ctrl+C cancels.')
    while win.down(0x79):time.sleep(.03) # Ignore an F10 already held at startup.
    while True:
        if win.down(0x79):
            target=win.game_window()
            while win.down(0x79):time.sleep(.03)
            if not target or win.game_window()!=target:
                print('F10 ignored: ShadowOfWar.exe must be focused.');continue
            if any(win.down(vk) for vk in (0x10,0x11,0x12,0x5B,0x5C)):
                print('Release Shift/Ctrl/Alt/Windows keys, then retry F10.');continue
            if args.replace:
                win.send(shortcut(0x41),target);time.sleep(.1)
            win.send(shortcut(0x56) if args.method=='clipboard' else unicode_events(name),target)
            print('Input sent. Check the field, then submit the search yourself.');return
        time.sleep(.03)

if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:print('\nCancelled.');sys.exit(130)
    except (OSError,RuntimeError,ValueError,UnicodeError) as exc:print('Paste failed:',str(exc),file=sys.stderr);sys.exit(1)
