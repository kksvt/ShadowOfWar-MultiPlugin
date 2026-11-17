#pragma once
#include "sow_entities.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <charconv>
#include <cstdio>
namespace sow::defender_provenance {
// This journal records this plugin's positive diagnoses, NOT arbitrary native
// Awe/vulnerability slots. Identity is scoped to a verified online invasion ID.
struct Context {std::string cloud,mode,region;Address session=0;bool valid=false;const char* status="unavailable";};
struct Key {std::string cloud,faction;std::uint64_t seed=0;std::uint32_t name=0;bool valid=false;};
struct Entry {Key key;unsigned mask=0;};
inline std::vector<Entry> entries;
inline std::mutex mutex;
inline PTP_WORK work=nullptr;
inline HMODULE queuedModule=nullptr;
inline bool enabled=false,scheduled=false,stopping=false,contextVerified=false;
inline std::uint64_t generation=0,savedGeneration=0;
inline ULONGLONG nextRetry=0;
inline bool writeFailed=false,loadFailed=false;
inline std::wstring path;
inline constexpr std::size_t maxEntries=4096,maxFile=1024*1024;
inline bool Imported(const std::string& name){
    for(const auto* n:{"InvasionFaction_Island","InvasionFaction_MtnPass","InvasionFaction_SnowMt","InvasionFaction_desert","InvasionFaction_Volcano","InvasionFaction_MinasIthil"})if(name==n)return true;return false;
}
inline bool Cloud(const std::string& s){
    if(s.size()<16 || s.size()>32)return false;bool nonzero=false;
    for(unsigned char c:s){if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_'))return false;nonzero|=c!='0';}return nonzero;
}
inline bool Same(const Key& a,const Key& b){return a.cloud==b.cloud && a.faction==b.faction && a.seed==b.seed && a.name==b.name;}
inline bool Valid(const Key& k){return k.valid && Cloud(k.cloud) && Imported(k.faction) && k.seed && k.name;}
inline std::string CString(Address address,unsigned bound){
    if(!bound || bound>33)return {};const auto bytes=Read<std::array<char,33>>(address);if(!bytes.readable)return {};
    for(unsigned i=0;i<bound;++i)if(!bytes.value[i])return std::string(bytes.value.data(),i);return {};
}
inline Context ReadContext(Address image){
    Context out;if(!contextVerified){out.status="online-layout-not-verified";return out;}
    const auto global=At<Address>(image,0x26FFAA8),session=At<Address>(global.value,0x6D30);
    const auto gate=At<unsigned char>(session.value,0x47C);
    if(!global.readable || !session.readable || !session.value || !gate.readable || !gate.value){out.status="no-active-online-context";return out;}
    const auto mode=ReadRecord(At<Address>(session.value,0x118).value),region=ReadRecord(At<Address>(session.value,0x130).value);
    if(!mode.nameComplete || !(mode.Named("ConquestRanked") || mode.Named("ConquestFriend") || mode.Named("Vendetta"))){out.status="unsupported-online-mode";return out;}
    out.cloud=CString(session.value+0x168,33);out.mode=mode.name.data();out.region=region.nameComplete?region.name.data():"";out.session=session.value;
    if(!Cloud(out.cloud)){out.status="missing-or-invalid-invasion-cloud-id";return out;}
    const auto g=At<Address>(image,0x26FFAA8),s=At<Address>(g.value,0x6D30);const auto gateAfter=At<unsigned char>(session.value,0x47C);
    if(!g.readable || !s.readable || !gateAfter.readable || g.value!=global.value || s.value!=session.value || gateAfter.value!=gate.value ||
       CString(session.value+0x168,33)!=out.cloud || At<Address>(session.value,0x118).value!=mode.address){out.status="online-context-changed";return out;}
    out.valid=true;out.status="verified-online-context";return out;
}
inline bool Unique(Address image,Address member,Address faction,std::uint64_t seed,std::uint32_t name){
    const auto g=At<Address>(image,0x26FFAA8),manager=At<Address>(g.value,0x6C68);
    const auto table=At<Address>(manager.value,0x98),owner=At<Address>(manager.value,0xA8),tag=At<Address>(faction,0x18);
    const auto count=At<unsigned>(manager.value,0xA0);
    if(!table.readable || !table.value || !owner.readable || !owner.value || !tag.readable || owner.value!=tag.value || !count.readable || count.value>4096)return false;
    Address pool=0,matchedEntry=0;unsigned pools=0;
    for(unsigned i=0;i<count.value;++i){const auto p=Read<Address>(table.value+Address(i)*0x290);if(!p.readable)return false;if(p.value==faction){++pools;matchedEntry=table.value+Address(i)*0x290;pool=Read<Address>(matchedEntry+0x28).value;}}
    const auto begin=At<Address>(pool,0x18),end=At<Address>(pool,0x20);
    if(pools!=1 || !pool || !begin.readable || !end.readable || end.value<begin.value || (end.value-begin.value)%8 || (end.value-begin.value)/8>8192 || !begin.value)return false;
    Address match=0;
    for(Address p=begin.value;p<end.value;p+=8){const auto m=Read<Address>(p);if(!m.readable)return false;if(!m.value)continue;
        const auto token=At<std::array<Address,3>>(m.value,0x168);if(!token.readable || token.value[2]!=faction)return false;
        if(token.value[0]==seed && std::uint32_t(token.value[1])==name){if(match && match!=m.value)return false;match=m.value;}}
    const auto b=At<Address>(pool,0x18),e=At<Address>(pool,0x20),gg=At<Address>(image,0x26FFAA8),mm=At<Address>(gg.value,0x6C68);
    return match==member && b.readable && e.readable && mm.readable && b.value==begin.value && e.value==end.value && mm.value==manager.value && At<Address>(manager.value,0x98).value==table.value && At<unsigned>(manager.value,0xA0).value==count.value && Read<Address>(matchedEntry).value==faction && Read<Address>(matchedEntry+0x28).value==pool;
}
inline Key ReadKey(Address image,Address member){
    Key out;const auto token=At<std::array<Address,3>>(member,0x168);if(!token.readable || !ReadRecord(At<Address>(member,0x268).value).nameComplete)return out;
    const auto faction=ReadRecord(token.value[2]);if(!faction.nameComplete || !Imported(faction.name.data()))return out;
    const auto ctx=ReadContext(image);if(!ctx.valid)return out;
    out={ctx.cloud,faction.name.data(),token.value[0],std::uint32_t(token.value[1]),true};
    if(!Valid(out) || !Unique(image,member,token.value[2],out.seed,out.name))out.valid=false;
    const auto again=At<std::array<Address,3>>(member,0x168);
    const auto globalAfter=At<Address>(image,0x26FFAA8),sessionAfter=At<Address>(globalAfter.value,0x6D30);
    const auto modeAfter=ReadRecord(At<Address>(ctx.session,0x118).value);
    if(!again.readable || again.value!=token.value || !sessionAfter.readable || sessionAfter.value!=ctx.session || !modeAfter.Named(ctx.mode.c_str()) ||
       CString(ctx.session+0x168,33)!=ctx.cloud || !At<unsigned char>(ctx.session,0x47C).value)out.valid=false;
    return out;
}
inline std::uint64_t Hash(const std::string& value){std::uint64_t h=14695981039346656037ull;for(unsigned char c:value){h^=c;h*=1099511628211ull;}return h;}
inline std::string Hex(std::uint64_t value){char s[24]{};std::snprintf(s,sizeof(s),"%llx",static_cast<unsigned long long>(value));return s;}
inline bool ParseHex(const std::string& value,std::uint64_t& result){if(value.empty() || value.size()>16)return false;auto r=std::from_chars(value.data(),value.data()+value.size(),result,16);return r.ec==std::errc{} && r.ptr==value.data()+value.size();}
inline std::string Encode(const std::vector<Entry>& values){
    std::string out="SoWRepairProvenance1\n";
    for(const auto& e:values){const auto row=e.key.cloud+'\t'+e.key.faction+'\t'+Hex(e.key.seed)+'\t'+Hex(e.key.name)+'\t'+Hex(e.mask);
        out+=row+'\t'+Hex(Hash(row))+'\n';}return out;
}
inline bool Decode(const std::string& text,std::vector<Entry>& out){
    const std::string header="SoWRepairProvenance1\n";if(text.size()>maxFile || text.compare(0,header.size(),header))return false;
    std::vector<Entry> parsed;std::size_t pos=header.size();
    while(pos<text.size()){
        const auto end=text.find('\n',pos);if(end==std::string::npos || end-pos>256 || end==pos || parsed.size()>=maxEntries)return false;
        const auto line=text.substr(pos,end-pos);std::array<std::string,6> f;std::size_t start=0;
        for(unsigned i=0;i<5;++i){const auto tab=line.find('\t',start);if(tab==std::string::npos)return false;f[i]=line.substr(start,tab-start);start=tab+1;}f[5]=line.substr(start);
        std::uint64_t seed=0,name=0,mask=0,checksum=0;
        if(!ParseHex(f[2],seed) || !ParseHex(f[3],name) || name>UINT32_MAX || !ParseHex(f[4],mask) || mask>255 || !ParseHex(f[5],checksum) || checksum!=Hash(line.substr(0,start-1)))return false;
        Entry e{{f[0],f[1],seed,static_cast<std::uint32_t>(name),true},static_cast<unsigned>(mask)};if(!Valid(e.key))return false;
        for(const auto& prior:parsed)if(Same(prior.key,e.key))return false;parsed.push_back(e);pos=end+1;
    }
    out=std::move(parsed);return true;
}
inline bool ReadFileEntries(const std::wstring& filename,std::vector<Entry>& out){
    HANDLE file=CreateFileW(filename.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
    LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size) && size.QuadPart>=0 && size.QuadPart<=static_cast<LONGLONG>(maxFile);
    std::string data;if(ok){data.resize(static_cast<std::size_t>(size.QuadPart));DWORD got=0;ok=ReadFile(file,data.data(),static_cast<DWORD>(data.size()),&got,nullptr) && got==data.size();}CloseHandle(file);
    return ok && Decode(data,out);
}
inline bool WriteFileEntries(const std::wstring& filename,const std::vector<Entry>& values){
    const auto data=Encode(values);const auto temporary=filename+L".tmp";
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD done=0;const bool ok=WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&done,nullptr) && done==data.size();CloseHandle(file);
    if(!ok){DeleteFileW(temporary.c_str());return false;}
    if(MoveFileExW(temporary.c_str(),filename.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(temporary.c_str());return false;
}
inline bool(*writer)(const std::wstring&,const std::vector<Entry>&)=&WriteFileEntries;
inline void CALLBACK Flush(PTP_CALLBACK_INSTANCE instance,void*,PTP_WORK){
    HMODULE held=nullptr;{std::lock_guard<std::mutex> lock(mutex);held=queuedModule;queuedModule=nullptr;}
    if(held)FreeLibraryWhenCallbackReturns(instance,held);
    try{for(;;){std::vector<Entry> copy;std::wstring file;std::uint64_t current;
        {std::lock_guard<std::mutex> lock(mutex);copy=entries;file=path;current=generation;}
        const bool ok=writer && writer(file,copy);
        {std::lock_guard<std::mutex> lock(mutex);writeFailed=!ok;if(ok){savedGeneration=current;nextRetry=0;}else nextRetry=GetTickCount64()+5000;
            if(!ok || current==generation){scheduled=false;return;}}
    }}catch(...){std::lock_guard<std::mutex> lock(mutex);writeFailed=true;nextRetry=GetTickCount64()+5000;scheduled=false;}
}
inline void QueueLocked(){
    if(!work || scheduled || stopping || generation==savedGeneration || GetTickCount64()<nextRetry)return;
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Flush),&module)){
        writeFailed=true;nextRetry=GetTickCount64()+5000;return;
    }
    if(module!=GetModuleHandleW(nullptr)){
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&Flush),&queuedModule)){
            writeFailed=true;nextRetry=GetTickCount64()+5000;return;}
    }
    scheduled=true;SubmitThreadpoolWork(work);
}
inline void Remember(const Key& key,unsigned mask=0){
    if(!enabled || !Valid(key))return;std::lock_guard<std::mutex> lock(mutex);
    for(auto& e:entries)if(Same(e.key,key)){if((e.mask|mask)!=e.mask){e.mask|=mask;++generation;}QueueLocked();return;}
    if(entries.size()>=maxEntries)entries.erase(entries.begin());entries.push_back({key,mask&255});++generation;QueueLocked();
}
inline bool Lookup(const Key& key,unsigned* mask=nullptr){
    if(!enabled || !Valid(key))return false;std::lock_guard<std::mutex> lock(mutex);
    for(const auto& e:entries)if(Same(e.key,key)){if(mask)*mask=e.mask;return true;}return false;
}
inline bool Init(const std::wstring& filename){
    if(work || enabled)return false;path=filename;entries.clear();generation=savedGeneration=0;nextRetry=0;scheduled=stopping=writeFailed=false;
    loadFailed=!ReadFileEntries(path,entries);if(loadFailed)entries.clear();
    work=CreateThreadpoolWork(&Flush,nullptr,nullptr);
    enabled=true;return work!=nullptr;
}
inline bool ProcessShuttingDown(){
    using Fn=BOOLEAN(WINAPI*)();const auto ntdll=GetModuleHandleW(L"ntdll.dll");
    const auto query=ntdll?reinterpret_cast<Fn>(GetProcAddress(ntdll,"RtlDllShutdownInProgress")):nullptr;
    return query && query()!=FALSE;
}
inline void Close(){
    // Process teardown has already stopped other threads. Never wait on their
    // callbacks or mutexes under the process-detach loader lock.
    if(ProcessShuttingDown()){enabled=false;return;}
    bool pending=false;
    {std::lock_guard<std::mutex> lock(mutex);stopping=true;enabled=false;pending=scheduled;}
    if(work){
        // A final library-reference release can invoke detach from callback
        // cleanup after our Flush body has finished. Never wait on that same
        // completed-body callback; CloseThreadpoolWork handles its tail safely.
        if(pending)WaitForThreadpoolWorkCallbacks(work,FALSE);
        CloseThreadpoolWork(work);work=nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex);entries.clear();scheduled=false;
}
}
