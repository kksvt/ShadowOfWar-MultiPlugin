#pragma once
#include <Windows.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace sow::near_attack_gate {
using Address=std::uintptr_t;
using Decide=bool(__fastcall*)(Address raw,Address circle,Address context,Address role);
// Exact bool getter replayed at the final-gate +43E read only.
inline constexpr std::array<unsigned char,8> getter{0x33,0xC0,0x38,0x01,0x0F,0x95,0xC0,0xC3};
struct Image {
    std::vector<unsigned char> bytes;
    DWORD codeSize=0, unwindOffset=0, popOffset=0, retOffset=0;
};
inline Image Build(Decide decide) {
    Image out; auto& b=out.bytes;
    auto emit=[&](std::initializer_list<unsigned char> v){b.insert(b.end(),v);};
    auto u32=[&](std::uint32_t v){for(int i=0;i<4;++i)b.push_back(static_cast<unsigned char>(v>>(i*8)));};
    auto u64=[&](Address v){for(int i=0;i<8;++i)b.push_back(static_cast<unsigned char>(v>>(i*8)));};
    b.insert(b.end(),getter.begin(),getter.end()-1);
    emit({0x9c});                         // pushfq: preserve getter's output flags
    emit({0x48,0x81,0xec}); u32(0x260);  // 608 bytes; align RSP to 16 before CALL
    // 32 bytes shadow space, 512-byte aligned FXSAVE area, seven volatile GPRs.
    const unsigned char regs[]={0x84,0x8c,0x94,0x84,0x8c,0x94,0x9c};
    for(unsigned i=0;i<7;++i){emit({static_cast<unsigned char>(i<3?0x48:0x4c),0x89,regs[i],0x24});u32(0x220+i*8);}
    emit({0x48,0x0f,0xae,0x44,0x24,0x20}); // fxsave64: x87, MXCSR, XMM0..15
    emit({0x48,0x8b,0xcb});              // RCX=RBX raw candidate
    emit({0x48,0x8b,0xd6});              // RDX=RSI target circle
    emit({0x4c,0x8b,0xc5});              // R8=RBP selection context
    emit({0x4c,0x8b,0xcf});              // R9=RDI play role
    emit({0x48,0xb8});u64(reinterpret_cast<Address>(decide));emit({0xff,0xd0});
    emit({0x84,0xc0,0x74,0x0c});
    emit({0x48,0xc7,0x84,0x24});u32(0x220);u32(1); // override only saved RAX
    emit({0x48,0x0f,0xae,0x4c,0x24,0x20}); // fxrstor64
    for(unsigned i=0;i<7;++i){emit({static_cast<unsigned char>(i<3?0x48:0x4c),0x8b,regs[i],0x24});u32(0x220+i*8);}
    emit({0x48,0x81,0xc4});u32(0x260);out.popOffset=static_cast<DWORD>(b.size());emit({0x9d});
    out.retOffset=static_cast<DWORD>(b.size());emit({0xc3});
    out.codeSize=static_cast<DWORD>(b.size());
    while(b.size()%4)b.push_back(0);
    out.unwindOffset=static_cast<DWORD>(b.size());
    // Prologue: getter 7 bytes, PUSHFQ at 7, SUB RSP,608 at 8.
    // Volatile register saves need no unwind codes. Describe both allocations.
    emit({1,15,3,0, 15,1,76,0, 8,2,0,0});
    // Separate unwind regions after ADD RSP and after POPFQ.
    emit({1,0,1,0, 0,2,0,0});
    emit({1,0,0,0});
    return out;
}
inline bool ReadBytes(Address p,void* data,SIZE_T size) {
    SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),data,size,&n)&&n==size;
}
inline void* AllocateNear(Address site) {
    SYSTEM_INFO info{};GetSystemInfo(&info);
    const Address gran=info.dwAllocationGranularity;
    const Address low=(std::max)(reinterpret_cast<Address>(info.lpMinimumApplicationAddress),site>0x7fff0000?site-0x7fff0000:Address(0));
    const Address high=(std::min)(reinterpret_cast<Address>(info.lpMaximumApplicationAddress),site+0x7fff0000);
    for(Address p=(low+gran-1)&~(gran-1);p<high;) {
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)))break;
        const Address end=reinterpret_cast<Address>(m.BaseAddress)+m.RegionSize;
        if(m.State==MEM_FREE && end-p>=4096)
            if(auto memory=VirtualAlloc(reinterpret_cast<void*>(p),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))return memory;
        if(end<=p || end>high)break;
        p=(end+gran-1)&~(gran-1);
    }
    return nullptr;
}
struct Patch {
    Address site=0;
    void* memory=nullptr;
    std::array<RUNTIME_FUNCTION,3> functions{};
    std::array<unsigned char,5> original{},replacement{};
    bool registered=false, installed=false;
    const char* error="none";
    // Install/remove require quiescent game threads, like the other detours.
    bool Write(const std::array<unsigned char,5>& bytes) {
        DWORD old=0;
        if(!VirtualProtect(reinterpret_cast<void*>(site),5,PAGE_EXECUTE_READWRITE,&old))return false;
        std::memcpy(reinterpret_cast<void*>(site),bytes.data(),5);
        FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(site),5);
        DWORD unused=0;VirtualProtect(reinterpret_cast<void*>(site),5,old,&unused);
        return true;
    }
    bool Remove() {
        if(installed){
            std::array<unsigned char,5> current{};
            if(!ReadBytes(site,current.data(),5)||current!=replacement||!Write(original))return false;
            installed=false;
        }
        if(registered){if(!RtlDeleteFunctionTable(functions.data()))return false;registered=false;}
        if(memory){VirtualFree(memory,0,MEM_RELEASE);memory=nullptr;}
        site=0;return true;
    }
    bool Install(Address callSite,Address nativeGetter,Decide decide) {
        if(memory || installed || !decide)return false;
        site=callSite;
        std::array<unsigned char,8> actual{};
        std::array<unsigned char,8> tail{};
        if(!ReadBytes(nativeGetter,actual.data(),actual.size())||actual!=getter ||
           !ReadBytes(site,original.data(),5)||original[0]!=0xe8 ||
           !ReadBytes(site+5,tail.data(),tail.size()) ||
           tail!=std::array<unsigned char,8>{0x84,0xC0,0x75,0x14,0x48,0x8D,0x8B,0x3F}) {
            error="native signature mismatch";return false;
        }
        std::int32_t displacement=0;std::memcpy(&displacement,original.data()+1,4);
        if(site+5+displacement!=nativeGetter){error="call target mismatch";return false;}
        memory=AllocateNear(site);if(!memory){error="near allocation failed";return false;}
        const auto image=Build(decide);std::memcpy(memory,image.bytes.data(),image.bytes.size());
        DWORD old=0;
        if(!VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)){error="gateway protection failed";Remove();return false;}
        FlushInstructionCache(GetCurrentProcess(),memory,image.bytes.size());
        functions={RUNTIME_FUNCTION{0,image.popOffset,image.unwindOffset},
            RUNTIME_FUNCTION{image.popOffset,image.retOffset,image.unwindOffset+12},
            RUNTIME_FUNCTION{image.retOffset,image.codeSize,image.unwindOffset+20}};
        if(!RtlAddFunctionTable(functions.data(),static_cast<DWORD>(functions.size()),reinterpret_cast<DWORD64>(memory))){error="unwind registration failed";Remove();return false;}
        registered=true;
        const auto delta=static_cast<std::int64_t>(reinterpret_cast<Address>(memory))-static_cast<std::int64_t>(site+5);
        if(delta<INT32_MIN || delta>INT32_MAX){error="gateway out of range";Remove();return false;}
        replacement=original;const auto relative=static_cast<std::int32_t>(delta);
        std::memcpy(replacement.data()+1,&relative,4);
        // Reject another patch installed since the initial check.
        std::array<unsigned char,5> current{};
        if(!ReadBytes(site,current.data(),5)||current!=original||!Write(replacement)){
            error="call-site write failed";Remove();return false;
        }
        installed=true;return true;
    }
};
} // namespace sow::near_attack_gate
