#pragma once
#include "sow_entities.hpp"
namespace sow::defender_repair {
struct ActorPair {const char* model;const char* ai;bool olog;};
inline constexpr ActorPair standardActors[]={
    {"Orc_Infantry","AI_Infantry",false},
    {"Orc_Infantry_Tough","AI_Infantry_Tough",false},
    {"Orc_Spearman","AI_Spearman",false},
    {"Orc_Spearman_Tough","AI_Spearman_Tough",false},
    {"Orc_Archer","AI_Archer",false},
    {"Orc_Archer_Tough","AI_Archer_Tough",false},
    {"Orc_Defender","AI_Defender",false},
    {"Orc_Defender_Tough","AI_Defender_Tough",false},
    {"Orc_Berserker","AI_Berserker",false},
    {"Orc_Berserker_Tough","AI_Berserker_Tough",false},
    {"OlogHai_Infantry","AI_Olog_Infantry",true},
};
inline constexpr const char* shippedOlogClasses[]={
    "CommanderOlog",
    "DLC_Architect",
    "DLC_Thrall",
    "G2010_Olog",
    "GP_Bruz",
    "GP_Ranger",
    "MachineDestroyerOlog",
    "OlogHai_Infantry",
    "OlogHai_Infantry_CelebrimborArmy",
    "OlogHai_Infantry_SauronArmy",
    "OlogHai_Prisoner",
    "Olog_Buff_DebugV2",
    "Shelob1_Olog",
    "ologHai_Infantry_DemoDragonbornOverlord",
};
inline const ActorPair* StandardClass(const Record& model){for(const auto& p:standardActors)if(model.Named(p.model))return &p;return nullptr;}
inline const ActorPair* StandardAI(const Record& tree){for(const auto& p:standardActors)if(tree.Named(p.ai))return &p;return nullptr;}
}
