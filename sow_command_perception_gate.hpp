#pragma once
#include "sow_grunt_command.hpp"

namespace sow::command_perception_gate {
struct Target {
    Address primary=0,secondary=0,effective=0;int kind=-1;bool readable=false,primaryReadable=false,secondaryReadable=false;
};
inline Target ReadTarget(Address stimulus) {
    Target out;const auto kind=At<std::uint8_t>(stimulus,0x88);
    const auto primary=At<Address>(stimulus,0x48),secondary=At<Address>(stimulus,0x70);
    out.primaryReadable=primary.readable;out.secondaryReadable=secondary.readable;
    if(primary.readable)out.primary=primary.value;if(secondary.readable)out.secondary=secondary.value;
    if(kind.readable){out.kind=kind.value;out.readable=kind.value==1?primary.readable:kind.value==2?secondary.readable:false;}
    if(out.readable)out.effective=out.kind==1?out.primary:out.secondary;
    return out;
}
enum class Reason {Unresolved,Unreadable,Priority,NoStimulus,ActorMode,TargetMode,NoCallbacks,
    RelationOverride,InvalidSlot,RelationValue,RelationMask,Height,Pending,Interval,PassSnapshot};
inline const char* Name(Reason reason) {
    switch(reason){
    case Reason::Unreadable:return "unreadable";case Reason::Priority:return "priority";
    case Reason::NoStimulus:return "no-stimulus";case Reason::ActorMode:return "actor-mode";
    case Reason::TargetMode:return "target-mode";case Reason::NoCallbacks:return "no-callbacks";
    case Reason::RelationOverride:return "relation-override-unmodeled";case Reason::InvalidSlot:return "invalid-slot";
    case Reason::RelationValue:return "relation-value";case Reason::RelationMask:return "relation-mask";
    case Reason::Height:return "height";case Reason::Pending:return "pending-bit";
    case Reason::Interval:return "interval";case Reason::PassSnapshot:return "snapshot-passes";
    default:return "unresolved";
    }
}
struct Snapshot {
    Reason reason=Reason::Unresolved;Target target{};Address stimulus=0;
    unsigned listenerPriority=0,stimulusPriority=0,actorMode=0,targetMode=0,mask=0;
    unsigned sourceSlot=~0u,targetSlot=~0u,listenerVersion=0,cachedVersion=0,pending=0;
    int relation=-1;double listenerTime=0,stamp=0;float interval=0;
};
// Snapshot the native gate's simple branches without calling getters or changing
// its result. This is a post-call explanation, not a branch trace. Dynamic
// relationship overrides and oversized callback spans remain unresolved.
inline Snapshot Read(Address metadata,Address envelope,Address relationMatrix) {
    Snapshot out;
    const auto priority=At<unsigned>(metadata,0x30),required=At<unsigned>(envelope,0x10);
    if(!priority.readable || !required.readable){out.reason=Reason::Unreadable;return out;}
    out.listenerPriority=priority.value;out.stimulusPriority=required.value;
    if(required.value>priority.value){out.reason=Reason::Priority;return out;}
    const auto stimulus=At<Address>(envelope,0x90);
    if(!stimulus.readable){out.reason=Reason::Unreadable;return out;}
    if(!stimulus.value){out.reason=Reason::NoStimulus;return out;}
    out.stimulus=stimulus.value;out.target=ReadTarget(stimulus.value);
    const auto source=At<Address>(metadata,0);
    const auto actorMode=At<unsigned>(envelope,0x14),targetMode=At<unsigned>(envelope,0x18);
    if(!source.readable || !actorMode.readable || !targetMode.readable || !out.target.readable || !out.target.primaryReadable || !out.target.secondaryReadable){out.reason=Reason::Unreadable;return out;}
    out.actorMode=actorMode.value;out.targetMode=targetMode.value;
    if((actorMode.value==0 && out.target.primary==source.value) || (actorMode.value==2 && out.target.primary!=source.value)){out.reason=Reason::ActorMode;return out;}
    if((targetMode.value==0 && out.target.secondary==source.value) || (targetMode.value==2 && out.target.secondary!=source.value)){out.reason=Reason::TargetMode;return out;}
    const auto listener=At<Address>(metadata,8),range=At<Address>(envelope,8);
    const auto begin=At<unsigned>(range.value,0x18),end=At<unsigned>(range.value,0x1C);
    const auto callbacks=At<Address>(listener.value,0x70);
    if(!listener.readable || !range.readable || !begin.readable || !end.readable || !callbacks.readable){out.reason=Reason::Unreadable;return out;}
    if(end.value<begin.value || end.value-begin.value>64)return out;
    bool any=false;
    for(unsigned i=begin.value;i<end.value;++i){const auto entry=At<Address>(callbacks.value,8ull*i);if(!entry.readable){out.reason=Reason::Unreadable;return out;}if(entry.value){any=true;break;}}
    if(!any){out.reason=Reason::NoCallbacks;return out;}
    const auto overrideRelation=At<std::uint8_t>(envelope,0x7C);
    const auto sourceSlot=At<unsigned>(metadata,0x28),targetSlot=At<unsigned>(envelope,0x1C),mask=At<unsigned>(envelope,0x78);
    if(!overrideRelation.readable || !sourceSlot.readable || !targetSlot.readable || !mask.readable){out.reason=Reason::Unreadable;return out;}
    out.sourceSlot=sourceSlot.value;out.targetSlot=targetSlot.value;out.mask=mask.value;
    if(overrideRelation.value){out.reason=Reason::RelationOverride;return out;}
    if(out.sourceSlot>=256 || out.targetSlot>=256){out.reason=Reason::InvalidSlot;return out;}
    const auto relation=At<std::uint8_t>(relationMatrix,0x818+256ull*out.sourceSlot+out.targetSlot);
    if(!relation.readable){out.reason=Reason::Unreadable;return out;}
    out.relation=relation.value;
    if(relation.value>=4){out.reason=Reason::RelationValue;return out;}
    if(!(mask.value&(1u<<relation.value))){out.reason=Reason::RelationMask;return out;}
    const auto heightLimit=At<float>(envelope,0x8C),height=At<float>(metadata,0x2C),stimulusHeight=At<float>(stimulus.value,0x7C);
    if(!heightLimit.readable || !height.readable || !stimulusHeight.readable || !std::isfinite(heightLimit.value) || !std::isfinite(height.value) || !std::isfinite(stimulusHeight.value))return out;
    if(heightLimit.value>0 && std::fabs(height.value-stimulusHeight.value)>heightLimit.value){out.reason=Reason::Height;return out;}
    const auto cache=stimulus.value+0x90+16ull*out.sourceSlot;
    const auto version=At<unsigned>(metadata,0x18),cached=At<unsigned>(cache,8);
    const auto time=At<double>(metadata,0x20),stamp=At<double>(cache,0);
    const auto interval=At<float>(envelope,0x88);const auto pending=At<std::uint8_t>(cache,0xC);
    if(!version.readable || !cached.readable || !time.readable || !stamp.readable || !interval.readable || !pending.readable){out.reason=Reason::Unreadable;return out;}
    out.listenerVersion=version.value;out.cachedVersion=cached.value;out.listenerTime=time.value;out.stamp=stamp.value;out.interval=interval.value;out.pending=pending.value;
    if(version.value!=cached.value){out.reason=Reason::PassSnapshot;return out;}
    if(pending.value){out.reason=Reason::Pending;return out;}
    if(!std::isfinite(time.value) || !std::isfinite(stamp.value) || !std::isfinite(interval.value))return out;
    out.reason=stamp.value+interval.value>time.value?Reason::Interval:Reason::PassSnapshot;return out;
}
} // namespace sow::command_perception_gate
