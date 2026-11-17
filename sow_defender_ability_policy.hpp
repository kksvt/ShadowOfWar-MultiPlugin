#pragma once
#include "sow_entities.hpp"

namespace sow::defender_ability_policy {
// Direct shipped preset grants. They explicitly carry Commander markers, but
// preserve the source exceptions rather than assuming every imported copy has
// already completed specialization initialization.
inline constexpr const char* reinforcementPresets[]={
    "DLC3_Architect","DLC3ToMain_Architect","DLC3ToMain_Slayer"
};
inline bool ReinforcementPreset(const Record& preset){
    for(const auto* name:reinforcementPresets)if(preset.Named(name))return true;
    return false;
}
inline bool InvalidReinforcements(const Record& trait,const char* specialization,
    const Record& preset,bool presetKnown){
    if(!trait.Named("Quirk_Summoner_Gang") || !specialization || !presetKnown)return false;
    return std::strcmp(specialization,"Marker_Specialization_Commander")!=0 && !ReinforcementPreset(preset);
}
inline bool NecromancyTrait(const Record& trait){
    return trait.Named("Raise_Dead") || trait.Named("Necromancer_SelfRes");
}
// The eligibility marker is deliberately distinct from actual ability traits.
// This does not inspect appearance: Gravewalker has DarkSkin, not Albino.
struct NecromancyState {
    bool traitsKnown=false,markersKnown=false;
    bool raiseDead=false,selfResurrection=false;
    bool necromancerMarker=false,curseMarker=false,curseWeapon=false;
};
enum class NecromancyReview {None,IncompleteContext,MissingCurseMarker,MissingCurseWeapon};
inline NecromancyReview ReviewNecromancy(const NecromancyState& state){
    if(!state.raiseDead && !state.selfResurrection)return NecromancyReview::None;
    if(!state.traitsKnown || !state.markersKnown)return NecromancyReview::IncompleteContext;
    if(!state.curseMarker)return NecromancyReview::MissingCurseMarker;
    if(!state.curseWeapon)return NecromancyReview::MissingCurseWeapon;
    return NecromancyReview::None;
}
inline const char* ReviewReason(NecromancyReview review){
    switch(review){
    case NecromancyReview::IncompleteContext:return "necromancy-context-incomplete";
    case NecromancyReview::MissingCurseMarker:return "necromancy-without-curse-marker; review-only";
    case NecromancyReview::MissingCurseWeapon:return "necromancy-without-curse-weapon-trait; review-only";
    default:return nullptr;
    }
}
// No automatic necromancy-cheat predicate is exposed: inventory/appearance
// provenance and special self-resurrection transitions are not established.
}
