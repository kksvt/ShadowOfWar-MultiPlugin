#pragma once
#include "sow_grunt_command.hpp"

// Read-only evidence. No native calls, hooks, cached pointers or game writes.
namespace sow::command_perception_audit {
inline constexpr unsigned slotCount=256;
inline constexpr Address rowStride=0x4808,pairStride=0x48;
struct Capacity {
    unsigned occupied=0;bool readable=false;Address matrix=0;
};
inline Capacity ReadCapacity(Address flags,Address matrixPointer) {
    Capacity out;
    const auto used=Read<std::array<std::uint8_t,slotCount>>(flags);
    const auto matrix=Read<Address>(matrixPointer);
    if(used.readable){out.readable=true;for(auto value:used.value)if(value)++out.occupied;}
    if(matrix.readable)out.matrix=matrix.value;
    return out;
}
struct Clock {double seconds=0;bool readable=false;};
inline Clock ReadClock(Address managerPointer) {
    const auto manager=Read<Address>(managerPointer);
    if(!manager.readable || !manager.value)return {};
    const auto world=At<Address>(manager.value,8);
    if(!world.readable || !world.value)return {};
    const auto value=At<double>(world.value,0x20);
    return value.readable && std::isfinite(value.value)?Clock{value.value,true}:Clock{};
}
enum class Status {IndexUnreadable,NoSlot,NoMatrix,RowUnreadable,RowMismatch,PairUnreadable,PairMissing,PairMismatch,Changed,Ready};
inline const char* Name(Status status) {
    switch(status){
    case Status::IndexUnreadable:return "index-unreadable";
    case Status::NoSlot:return "no-slot";
    case Status::NoMatrix:return "no-matrix";
    case Status::RowUnreadable:return "row-unreadable";
    case Status::RowMismatch:return "row-mismatch";
    case Status::PairUnreadable:return "pair-unreadable";
    case Status::PairMissing:return "pair-missing";
    case Status::PairMismatch:return "pair-mismatch";
    case Status::Changed:return "identity-changed";
    case Status::Ready:return "ready";
    }
    return "unknown";
}
struct Evidence {
    Status status=Status::IndexUnreadable;
    std::uint32_t sourceSlot=~0u,targetSlot=~0u;
    unsigned state=0,type=0,evidence43=0,awareness44=0,refresh46=0;
    double stamp=0,age=0;float duration=0;int pastExpiry=-1;
};
template<class T> inline T Copy(const std::array<std::uint8_t,0x48>& bytes,std::size_t offset) {
    T value{};std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;
}
inline Evidence ReadEvidence(Address source,Address target,Address matrix,Clock clock) {
    Evidence out;
    if(!source || !target || source==target)return out;
    const auto sourceSlot=At<std::uint32_t>(source,0x2BF8),targetSlot=At<std::uint32_t>(target,0x2BF8);
    if(!sourceSlot.readable || !targetSlot.readable)return out;
    out.sourceSlot=sourceSlot.value;out.targetSlot=targetSlot.value;
    if(out.sourceSlot>=slotCount || out.targetSlot>=slotCount){out.status=Status::NoSlot;return out;}
    if(!matrix){out.status=Status::NoMatrix;return out;}
    const auto row=matrix+rowStride*out.sourceSlot;
    const auto owner=Read<Address>(row);
    if(!owner.readable){out.status=Status::RowUnreadable;return out;}
    if(owner.value!=source){out.status=Status::RowMismatch;return out;}
    const auto pair=row+8+pairStride*out.targetSlot;
    const auto bytes=Read<std::array<std::uint8_t,0x48>>(pair);
    if(!bytes.readable){out.status=Status::PairUnreadable;return out;}
    const auto pairTarget=Copy<Address>(bytes.value,0);
    if(!pairTarget){out.status=Status::PairMissing;return out;}
    if(pairTarget!=target){out.status=Status::PairMismatch;return out;}
    // Reject slot/identity reuse during a snapshot; other fields are best-effort,
    // not an atomic view of the engine's perception update.
    const auto sourceAgain=At<std::uint32_t>(source,0x2BF8),targetAgain=At<std::uint32_t>(target,0x2BF8);
    const auto ownerAgain=Read<Address>(row),pairAgain=Read<Address>(pair);
    if(!sourceAgain.readable || !targetAgain.readable || !ownerAgain.readable || !pairAgain.readable ||
       sourceAgain.value!=out.sourceSlot || targetAgain.value!=out.targetSlot || ownerAgain.value!=source || pairAgain.value!=target){
        out.status=Status::Changed;return out;
    }
    out.status=Status::Ready;
    out.state=bytes.value[0x40];out.type=bytes.value[0x41];
    out.evidence43=bytes.value[0x43];out.awareness44=bytes.value[0x44];out.refresh46=bytes.value[0x46];
    out.stamp=Copy<double>(bytes.value,0x20);out.duration=Copy<float>(bytes.value,0x34);
    if(clock.readable && std::isfinite(clock.seconds) && std::isfinite(out.stamp) && std::isfinite(out.duration) && out.duration>=0){
        out.age=clock.seconds-out.stamp;
        if(clock.seconds>=out.stamp)out.pastExpiry=clock.seconds>out.stamp+out.duration?1:0;
    }
    return out;
}
} // namespace sow::command_perception_audit
