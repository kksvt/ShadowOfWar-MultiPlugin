#pragma once
#include "sow_grunt_command_probe.hpp"
#include <algorithm>
#include "sow_command_gate.hpp"
#pragma comment(lib, "User32.lib")

namespace sow::grunt_command {
using BuffFn = grunt_command_probe::BuffFn;
using OutlineFn = void(__fastcall*)(Address, bool);
using UpdateFn = void(__fastcall*)(Address, Address, Address, Address);
using CleanupFn = void(__fastcall*)(Address);
using ItemFn = void(__fastcall*)(Address, Address);
using HasFn = bool(__fastcall*)(Address, Address);
using ModeFn = void(__fastcall*)(Address, Address, std::uint8_t, Address, Address);
using EntryFn = Address(*)(Address);
using ManagerFn = Address(*)();
using LookupFn = Address(*)(const char*);
using Vec3 = std::array<float, 3>;
struct CameraView { Vec3 position{}, forward{}; };
using CameraFn = bool(*)(Address, CameraView&);
using StateFn = Address(__fastcall*)(Address);
using MarkerCreateFn = Address(__fastcall*)(Address*);
using MarkerDeleteFn = void(__fastcall*)(Address*);
using MarkerSetFn = void(__fastcall*)(Address, Address);
using MarkerEnableFn = void(__fastcall*)(Address, bool);
inline Address LookupMarker() { return ring_might::Lookup(0x2701688,"Combat_Marked"); }
inline bool CameraForward(const std::array<float, 4>& quaternion, Vec3& forward) {
    float norm = 0;
    for (float value : quaternion) {
        if (!std::isfinite(value)) return false;
        norm += value * value;
    }
    if (!std::isfinite(norm) || norm < .25f || norm > 4.f) return false;
    const float scale = 1.f / std::sqrt(norm);
    const float x=quaternion[0]*scale, y=quaternion[1]*scale;
    const float z=quaternion[2]*scale, w=quaternion[3]*scale;
    // Mirrors native 3703E0: camera local +Z, quaternion XYZW.
    forward = {2.f*(x*z+y*w), 2.f*(y*z-x*w), 1.f-2.f*(x*x+y*y)};
    return true;
}
inline bool ReadCamera(Address player, CameraView& camera) {
    // Camera.DumpInfo -> 24604C -> 180458 uses this actor transform.
    const auto component = At<Address>(player, 0x2F90);
    const auto actor = At<Address>(component.value, 0xA8);
    const auto position = At<Vec3>(actor.value, 0xAC);
    const auto rotation = At<std::array<float, 4>>(actor.value, 0xB8);
    if (!component.readable || !actor.readable || !position.readable || !rotation.readable) return false;
    for (float value : position.value) if (!std::isfinite(value)) return false;
    camera.position = position.value;
    return CameraForward(rotation.value, camera.forward);
}
using ProjectFn = bool(*)(const Vec3&, Vec3&);
using BlockedFn = bool(__fastcall*)(Address, bool);
inline bool ProjectPoint(const Vec3& point, Vec3& screen) {
    // Same world-to-screen path used by native combat targeting at 24495E.
    const auto root = At<Address>(ring_might::Base(), 0x26FFA98).value;
    const auto view = At<Address>(root, 0x898).value;
    const auto id = At<unsigned>(view, 0x80);
    if (!id.readable || !id.value) return false;
    using Project = Address(__fastcall*)(Address, Vec3*, const Vec3*, bool*, bool*);
    bool onScreen = false, behind = true;
    reinterpret_cast<Project>(ring_might::Base() + 0x1A7238)(view, &screen, &point, &onScreen, &behind);
    return onScreen && !behind && std::isfinite(screen[0]) &&
        std::isfinite(screen[1]) && std::isfinite(screen[2]) && screen[2] > 0;
}
inline Address LookupItem(const char* name) { return ring_might::Lookup(0x2700530, name); }
struct NativeAPI {
    ItemFn add = nullptr, remove = nullptr; HasFn has = nullptr; ModeFn mode = nullptr;
    EntryFn entry = &grunt_command_probe::FindEntry;
    ManagerFn manager = &grunt_command_probe::Manager;
    LookupFn lookup = &LookupItem;
    ProjectFn project = &ProjectPoint;
    OutlineFn outline = nullptr;
    BlockedFn blocked = nullptr;
    CameraFn camera = &ReadCamera;
    StateFn state = nullptr;
    MarkerCreateFn markerCreate = nullptr;
    MarkerDeleteFn markerDelete = nullptr;
    MarkerSetFn markerType = nullptr, markerBind = nullptr;
    MarkerEnableFn markerEnable = nullptr;
    ManagerFn markerRecord = &LookupMarker;
};
inline NativeAPI native;
inline command_gate::Patch siegeGate;
inline thread_local Address checkingActor=0;
inline std::unique_ptr<PLH::x64Detour> buffHook, outlineHook, updateHook, cleanupHook;
inline std::uint64_t buffTrampoline = 0, outlineTrampoline = 0, updateTrampoline = 0;
inline std::uint64_t cleanupTrampoline = 0;
inline constexpr std::size_t maxOrders=256;
struct OrderSignal {
    std::atomic<Address> actor{0}, stats{0}, target{0};
    std::atomic<bool> destroyed{false};
    std::atomic<unsigned> damageReads{0}, progress{0};
};
inline std::array<OrderSignal,maxOrders> orderSignals{};
inline int commandKey = VK_F6, stealthKey = VK_LSHIFT;
inline int rallyKey=VK_F5;
inline bool rallyKeyDown=false;
using FollowFn=void(__fastcall*)(Address,Address,Address);
using ClearFollowFn=void(__fastcall*)(Address,bool);
inline Address LookupRallyMarker(){return ring_might::Lookup(0x2701688,"Followers");}
inline Address LookupRallyBand(){return ring_might::Lookup(0x2702420,"OrcBuddyFront");}
struct RallyAPI {
    FollowFn follow=nullptr;ClearFollowFn clear=nullptr;ManagerFn record=&LookupRallyBand;
    ManagerFn markerRecord=&LookupRallyMarker;
};
inline RallyAPI rallyNative;
struct RallyOrder {
    Address actor=0,stats=0,player=0,playerStats=0,component=0,band=0,record=0;
    ULONGLONG until=0,nextTrace=0;
    Address marker=0;
};
inline std::array<RallyOrder,maxOrders> rallyOrders{};
inline std::array<std::atomic<bool>,maxOrders> rallyDestroyed{};
inline std::array<std::atomic<Address>,maxOrders> rallyActors{};
inline void StopRally();
inline bool requireStealthKey = true, diagnostics = true, aiming = false;
inline bool consoleOutput = true;
inline bool crowdTrace=false;
inline bool traceNativeCommandEnds=false;
inline std::atomic<ULONGLONG> nextCrowdTrace{0};
inline std::atomic<bool> crowdCommandsActive{false};
inline std::atomic<unsigned long long> crowdBatch{0};
inline thread_local bool updating = false;
inline std::mutex stateMutex, logMutex, updateMutex;
inline std::atomic<unsigned> logCount{0};
struct Binding { Address actor = 0, stats = 0; };
struct Active {
    Binding unit; Address item = 0; bool ownsItem = false, issued = false;
    ULONGLONG readyUntil = 0, finishUntil = 0;
    Binding target; Address player = 0;
    int lastState = -1;
    ULONGLONG issuedAt = 0, nextTrace = 0;
    Address lastBoundTarget = ~Address{0}, lastCombatTarget = ~Address{0}, lastAttack = ~Address{0};
    int lastCommandFlag = -1, lastTargetCheck = -1;
    float lastTargetHealth = NAN;
    ULONGLONG lastBlockTrace = 0;
    const char* lastBlockReason = nullptr;
};
inline std::array<Binding, 256> candidates{};
inline std::array<Binding, 512> enemies{};
inline std::array<Binding, 16> outlined{};
inline std::array<Active,maxOrders> orders{};
inline bool(*stallObserver)(std::size_t,ULONGLONG,Address)=nullptr;
inline void(*stallTick)(ULONGLONG)=nullptr;
inline void(*orderAuditTick)(ULONGLONG)=nullptr;
inline void(*stallShutdown)()=nullptr;
struct Preview { Binding unit; bool owns = false, nativeWants = false; };
inline Preview preview;
inline std::atomic<Address> previewActor{0};
inline std::atomic<bool> previewDestroyed{false};
inline ULONGLONG nextAimUpdate = 0;
inline Address previewMarker=0, orderMarker=0;
inline bool Live(const Binding& b);
inline void Log(const char* event, Address actor=0, Address target=0, bool force=false);
inline void DestroyMarker(Address& holder) {
    const auto marker=holder; holder=0;
    if (marker && native.markerDelete) { auto temporary=marker; native.markerDelete(&temporary); }
}
inline void MakeMarker(Address& holder, const Binding& target, Address explicitType=0) {
    DestroyMarker(holder);
    if (!native.markerCreate || !native.markerDelete || !native.markerType ||
        !native.markerBind || !native.markerEnable || !Live(target)) return;
    const auto object=At<Address>(target.stats,0x48);
    const auto type=explicitType?explicitType:native.markerRecord();
    if (!object.readable || !object.value || !type) {
        Log("HUD marker unavailable: type or target object",0,target.actor); return;
    }
    native.markerCreate(&holder);
    if (!holder) return;
    native.markerType(holder,type); native.markerBind(holder,object.value); native.markerEnable(holder,true);
    Log(explicitType?"rally follower HUD marker bound":"native Combat_Marked HUD marker bound",0,target.actor);
}

inline void Log(const char* event, Address actor, Address target, bool force) {
    if (!force && (!diagnostics || logCount.fetch_add(1) >= 2048)) return;
    char line[1024]{};
    std::snprintf(line, sizeof(line), "[GruntCommand] %s actor=%p target=%p\n", event,
        reinterpret_cast<void*>(actor), reinterpret_cast<void*>(target));
    std::lock_guard<std::mutex> lock(logMutex);
    if (consoleOutput && GetConsoleWindow()) {
        std::fputs(line, stdout);
        std::fflush(stdout);
    }
    // File-path/open failure must not prevent output to the existing console.
    char path[MAX_PATH]{};
    const auto length = GetTempPathA(MAX_PATH, path);
    constexpr char name[] = "SoW-GruntCommand-active.log";
    if (!length || length + sizeof(name) > MAX_PATH) return;
    std::memcpy(path + length, name, sizeof(name));
    FILE* file = nullptr;
    if (fopen_s(&file, path, "a") == 0 && file) {
        std::fputs(line, file);
        std::fclose(file);
    }
}
inline Binding Bind(Address actor) { return {actor, At<Address>(actor, 0x26A8).value}; }
inline bool Live(const Binding& b) {
    return b.actor && b.stats && At<Address>(b.actor, 0x26A8).value == b.stats &&
        At<Address>(b.stats, 0x28).value == b.actor;
}
inline bool Alive(const Binding& b) {
    const auto hp = At<float>(b.stats, offset::stats::health);
    return Live(b) && hp.readable && std::isfinite(hp.value) && hp.value > 0;
}
inline const char* ExplainBlock(Address actor, Address context=0, bool omitSiege=false) {
    // Follow 1AA49D0(false) in native branch order. Names describe fields,
    // not unverified gameplay meanings. The v7 diagnostic can omit the scoped siege check.
    const auto stats=At<Address>(actor,0x26A8);
    if (!stats.readable) return "unreadable-actor";
    if (!stats.value) return "missing-stats";
    if (!context) context=At<Address>(ring_might::Base(),0x26FFAB0).value;
    const auto manager=At<Address>(context,0x778);
    const auto owner=At<Address>(manager.value,0x30);
    const auto gate=At<Address>(owner.value,0x1D8);
    if (!manager.readable || !owner.readable || !gate.readable) return "unreadable-global-gate";
    if (gate.value) {
        const auto flag=At<std::uint8_t>(gate.value,0x48);
        if (!flag.readable) return "unreadable-global-gate-flag";
        if (flag.value && !omitSiege) return "global-owner-1D8-flag48";
    }
    const auto relation=At<Address>(actor,0x2578), attached=At<Address>(actor,0x25A0);
    const auto allowed=At<std::uint8_t>(actor,0x2E7C);
    if (!relation.readable || !attached.readable || !allowed.readable) return "unreadable-actor-relation";
    if (relation.value==actor && attached.value && !allowed.value) return "self-relation-25A0-without-2E7C";
    const auto component=At<Address>(actor,0x2D08);
    const auto value=At<float>(component.value,0x18);
    if (!component.readable || !value.readable) return "unreadable-2D08-value18";
    if (value.value==0.f) return "zero-2D08-value18";
    const auto ai=At<Address>(actor,0x2CE8);
    const auto special=At<std::uint8_t>(stats.value,0x4A0);
    if (!ai.readable || !ai.value || !special.readable) return "unreadable-ai-or-special-flag";
    if (!special.value) {
        const auto a=At<std::uint8_t>(ai.value,0x940);
        const auto b=At<Address>(ai.value,0x968);
        if (!a.readable || !b.readable) return "unreadable-ai940-or968";
        if (a.value || b.value) return "ai940-or968-active";
    }
    if (!native.state) return "state-reader-unavailable";
    const auto state=native.state(actor);
    const auto stateFlag=At<std::uint8_t>(state,0x66);
    if (!stateFlag.readable) return "unreadable-state66";
    if (stateFlag.value) return "state-record-flag66";
    const auto pair=At<std::array<std::uint8_t,2>>(ai.value,0x910);
    const auto busy=At<Address>(ai.value,0x600);
    if (!pair.readable || !busy.readable) return "unreadable-ai910-or600";
    if (pair.value[0] && !pair.value[1]) return "ai910-active-without911";
    if (busy.value) return "ai600-nonnull";
    return "no-block-found-or-state-changed";
}
inline bool RallyType(const Binding& b) {
    if(grunt_aggression::OrcGrunt(b.stats))return true;
    const auto player=At<Address>(b.stats,0x40);
    const auto index=At<std::int32_t>(b.actor,0x2710);
    return Live(b) && player.readable && !player.value && index.readable && index.value==-1 &&
        ReadRecord(At<Address>(b.stats,0x60).value).Named("OlogHai_Infantry");
}
inline bool RallyFriendly(const Binding& b) {
    return Alive(b) && RallyType(b) && grunt_command_probe::PlayerAlly(b.actor);
}
inline bool Friendly(const Binding& b) {
    return Alive(b) && grunt_aggression::OrcGrunt(b.stats) &&
        grunt_command_probe::PlayerAlly(b.actor);
}
inline bool __fastcall ExemptSiegeGate(Address, Address actor) noexcept {
    if (!actor) return false;
    if (actor==checkingActor) return Friendly(Bind(actor));
    for (auto& signal:orderSignals) if (signal.actor.load()==actor) {
        if (signal.destroyed.load()) return false;
        return Friendly({actor,signal.stats.load()});
    }
    return false;
}
inline bool CommandBlocked(Address actor) {
    if (!native.blocked) return true;
    const auto previous=checkingActor;
    checkingActor=actor;
    const bool result=native.blocked(actor,false);
    checkingActor=previous;
    return result;
}
inline bool Enemy(const Binding& b) {
    const auto marker = At<Address>(b.stats, 0x40);
    // Read all allegiance flags: an unreadable actor is not treated as an enemy.
    const auto a = At<std::uint8_t>(b.actor, 0x2700);
    const auto c = At<std::uint8_t>(b.actor, 0x23B8);
    const auto d = At<std::uint8_t>(b.actor, 0x2C04);
    return Alive(b) && marker.readable && !marker.value && a.readable && c.readable &&
        d.readable && !a.value && !c.value && !d.value;
}
inline bool Highlighted(const Binding& b) {
    const auto flag = At<std::uint8_t>(b.actor, 0x23C8);
    return Enemy(b) && flag.readable && (flag.value & 1);
}
inline float DistanceSquared(Address a, Address b) {
    const auto p = At<std::array<float, 3>>(a, 0xAC), q = At<std::array<float, 3>>(b, 0xAC);
    if (!p.readable || !q.readable) return INFINITY;
    float result = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!std::isfinite(p.value[i]) || !std::isfinite(q.value[i])) return INFINITY;
        const float delta = p.value[i] - q.value[i]; result += delta * delta;
    }
    return result;
}
inline void Observe(const Binding& b) {
    // Initial faction buffs can precede health initialization. Validate health
    // at selection time instead of losing such newly spawned allies here.
    if (!Live(b) || !RallyType(b) ||
        !grunt_command_probe::PlayerAlly(b.actor)) return;
    std::lock_guard<std::mutex> lock(stateMutex);
    for (auto& entry : candidates) if (entry.actor == b.actor) { entry = b; return; }
    for (auto& entry : candidates) if (!Live(entry)) { entry = b; return; }
}
inline void ObserveEnemy(const Binding& b) {
    if (!Live(b)) return;
    const auto marker = At<Address>(b.stats, 0x40);
    if (!marker.readable || marker.value) return;
    // Cache identity before health/allegiance initialization; validate at selection.
    std::lock_guard<std::mutex> lock(stateMutex);
    for (auto& entry : enemies) if (entry.actor == b.actor) { entry = b; return; }
    for (auto& entry : enemies) if (!Live(entry)) { entry = b; return; }
}
inline void Mark(const Binding& b, bool enabled) {
    std::lock_guard<std::mutex> lock(stateMutex);
    for (auto& entry : outlined) if (entry.actor == b.actor) {
        entry = enabled ? b : Binding{}; return;
    }
    if (enabled) for (auto& entry : outlined) if (!Highlighted(entry)) { entry = b; return; }
}
inline Binding Target() {
    std::array<Binding, 16> snapshot;
    { std::lock_guard<std::mutex> lock(stateMutex); snapshot = outlined; }
    Binding chosen;
    for (const auto& b : snapshot) if (Highlighted(b)) {
        if (chosen.actor && chosen.actor != b.actor) return {}; // Ambiguous outline: do not guess.
        chosen = b;
    }
    return chosen;
}
inline float AimScore(const Vec3& screen) {
    if (!std::isfinite(screen[0]) || !std::isfinite(screen[1]) || !std::isfinite(screen[2]) ||
        screen[2] <= 0 || screen[0] < 0 || screen[0] > 1 || screen[1] < 0 || screen[1] > 1)
        return INFINITY;
    const float x = screen[0] - .5f, y = screen[1] - .5f;
    const float score = x*x + y*y;
    return score <= .12f*.12f ? score : INFINITY;
}
inline float CameraCosine(const CameraView& camera, const Vec3& point) {
    float length = 0, dot = 0;
    for (unsigned i=0; i<3; ++i) {
        if (!std::isfinite(point[i]) || !std::isfinite(camera.position[i]) ||
            !std::isfinite(camera.forward[i])) return -1.f;
        const float delta = point[i] - camera.position[i];
        length += delta*delta; dot += delta*camera.forward[i];
    }
    if (!std::isfinite(length) || !std::isfinite(dot) || length < 1.f) return -1.f;
    return (std::max)(-1.f, (std::min)(1.f, dot/std::sqrt(length)));
}
inline float BodyCosine(const CameraView& camera, const Vec3& origin) {
    auto torso=origin, head=origin;
    torso[1] += 100.f; head[1] += 150.f;
    return (std::max)(CameraCosine(camera,torso), CameraCosine(camera,head));
}
inline Binding AimTarget(Address player, bool report = false) {
    CameraView camera;
    const bool haveCamera = native.camera(player,camera);
    if (report && haveCamera) {
        char message[256]{};
        std::snprintf(message,sizeof(message),"camera pos=(%.1f,%.1f,%.1f) forward=(%.3f,%.3f,%.3f)",
            camera.position[0],camera.position[1],camera.position[2],
            camera.forward[0],camera.forward[1],camera.forward[2]);
        Log(message);
    }
    const auto nativeTarget = Target();
    const auto nativePoint = At<Vec3>(nativeTarget.actor,0xAC);
    const bool nativeInView = !haveCamera ||
        (nativePoint.readable && BodyCosine(camera,nativePoint.value) >= .5f);
    if (nativeTarget.actor && nativeInView && DistanceSquared(player,nativeTarget.actor) <= 4000.f*4000.f) {
        if (report) Log("aim scan method=native-outline", 0, nativeTarget.actor);
        return nativeTarget;
    }
    std::array<Binding, 512> snapshot;
    Binding previous;
    { std::lock_guard<std::mutex> lock(stateMutex); snapshot = enemies; previous = preview.unit; }
    Binding selected;
    float best = INFINITY, bestDistance = INFINITY;
    float closestCosine = -1.f, closestScreenScore = INFINITY;
    Vec3 closestScreen{};
    unsigned cached = 0, aliveEnemies = 0, inRange = 0, projected = 0, inCone = 0;
    for (const auto& b : snapshot) if (b.actor) ++cached;
    for (const auto& b : snapshot) if (Enemy(b)) {
        ++aliveEnemies;
        const float distance = DistanceSquared(player, b.actor);
        if (distance > 4000.f*4000.f) continue;
        ++inRange;
        auto position = At<Vec3>(b.actor, 0xAC);
        if (!position.readable) continue;
        float score = INFINITY;
        if (haveCamera) {
            const float cosine = BodyCosine(camera,position.value);
            closestCosine = (std::max)(closestCosine,cosine);
            // 14-degree acquisition cone; no viewport-id or normalized-screen assumption.
            if (cosine >= .9702957f) score = 1.f-cosine;
        } else {
            position.value[1] += 100.f;
            Vec3 screen{};
            if (!native.project(position.value,screen)) continue;
            ++projected;
            const float x=screen[0]-.5f, y=screen[1]-.5f, raw=x*x+y*y;
            if (raw < closestScreenScore) { closestScreenScore=raw; closestScreen=screen; }
            score = AimScore(screen);
        }
        if (std::isfinite(score)) ++inCone;
        if (score < best || (std::isfinite(score) && score == best && distance < bestDistance)) {
            selected = b; best = score; bestDistance = distance;
        }
    }
    if (!selected.actor && haveCamera && Enemy(previous) &&
        DistanceSquared(player,previous.actor) <= 4000.f*4000.f) {
        const auto point = At<Vec3>(previous.actor,0xAC);
        // Keep a displayed lock until 18 degrees to avoid losing it on minor camera motion.
        if (point.readable && BodyCosine(camera,point.value) >= .9510565f) selected=previous;
    }
    if (report) {
        char message[480]{};
        std::snprintf(message, sizeof(message), "aim scan method=%s cached=%u liveEnemies=%u inRange=%u nearAim=%u bestCameraAngle=%.2f projected=%u closestScreen=(%.3f,%.3f,%.1f)",
            haveCamera ? "camera-ray" : "screen-fallback",cached,aliveEnemies,inRange,inCone,
            std::acos(closestCosine)*57.2957795f,projected,closestScreen[0],closestScreen[1],closestScreen[2]);
        Log(message, 0, selected.actor);
    }
    return selected;
}
inline void ClearPreview() {
    DestroyMarker(previewMarker);
    Preview previous;
    { std::lock_guard<std::mutex> lock(stateMutex); previous = preview; preview = {}; }
    previewActor = 0;
    const bool destroyed = previewDestroyed.exchange(false);
    if (!destroyed && previous.owns && !previous.nativeWants && Live(previous.unit) && native.outline)
        native.outline(previous.unit.actor, false);
}
inline void SetPreview(const Binding& b) {
    Address current = 0;
    { std::lock_guard<std::mutex> lock(stateMutex); current = preview.unit.actor; }
    if (current != b.actor) {
        ClearPreview();
        if (!b.actor) return;
        const bool wasHighlighted = (At<std::uint8_t>(b.actor, 0x23C8).value & 1) != 0;
        { std::lock_guard<std::mutex> lock(stateMutex); preview = {b, !wasHighlighted, wasHighlighted}; }
        previewDestroyed = false; previewActor = b.actor;
        Log("aim selected enemy", 0, b.actor);
        MakeMarker(previewMarker,b);
    }
    if (b.actor && !(At<std::uint8_t>(b.actor, 0x23C8).value & 1) && native.outline) {
        { std::lock_guard<std::mutex> lock(stateMutex); preview.owns = true; }
        native.outline(b.actor, true);
    }
}
inline Binding PreviewTarget() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return previewDestroyed.load() ? Binding{} : preview.unit;
}
inline Binding Choose(Address player, Address target) {
    std::array<Binding, 256> snapshot;
    { std::lock_guard<std::mutex> lock(stateMutex); snapshot = candidates; }
    Binding chosen, blockedChoice;
    float best = INFINITY, bestBlocked=INFINITY;
    for (const auto& b : snapshot) if (Friendly(b) &&
        DistanceSquared(player, b.actor) <= 3000.f * 3000.f) {
        const auto entry = native.entry(b.actor);
        if (entry) {
            const auto setup = ReadRecord(At<Address>(entry, 8).value);
            if (!setup.Named("Bodyguard_DefaultOrc") || At<std::uint8_t>(entry, 0x80).value != 1)
                continue; // Preserve other buddy types and busy native summons.
        }
        const float distance = DistanceSquared(target, b.actor);
        if (distance > 4000.f*4000.f) continue;
        if (native.blocked && CommandBlocked(b.actor)) {
            if (distance<bestBlocked) { bestBlocked=distance; blockedChoice=b; }
        } else if (distance<best) { best=distance; chosen=b; }
    }
    return chosen.actor ? chosen : blockedChoice;
}
inline bool Foreground() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}
inline bool RisingEdge(bool down, bool& previous) {
    const bool edge = down && !previous; previous = down; return edge;
}
inline std::size_t OrderCount() {
    std::size_t count=0;for(const auto& order:orders)if(order.unit.actor)++count;return count;
}
inline void ReleaseUnit(std::size_t slot) {
    auto& active=orders[slot]; auto& signal=orderSignals[slot];
    const auto previous=active; active={};
    signal.actor=0;signal.target=0;signal.stats=0;
    const bool destroyed=signal.destroyed.exchange(false);
    if (!destroyed && previous.ownsItem && Live(previous.unit)) {
        const auto inventory=At<Address>(previous.unit.actor,0x2348).value;
        if(inventory && native.has(inventory,previous.item)) {
            native.remove(previous.unit.actor,previous.item);
            Log("removed temporary command item",previous.unit.actor);
        }
    }
}
inline void Release() {
    crowdCommandsActive=false;
    for(std::size_t i=0;i<orders.size();++i)ReleaseUnit(i);
    DestroyMarker(orderMarker);
}
inline bool CanDispatch(unsigned state, bool owns, bool hardBlocked, bool invalidPlayer) {
    return !invalidPlayer && (state == 1 || (state == 0 && owns && !hardBlocked));
}
inline void TraceCommand(Active& active, ULONGLONG now, Address entry, bool healthTrace) {
    if (!diagnostics || now < active.nextTrace) return;
    active.nextTrace = now + 100;
    const auto bound = At<Address>(active.unit.actor,0x2EB8);
    const auto flag = At<std::uint8_t>(active.unit.actor,0x2E90);
    const auto failed = At<std::uint8_t>(entry,0x38);
    const auto combat = At<Address>(At<Address>(active.unit.stats,0xA8).value,0x28);
    if (bound.value != active.lastBoundTarget || combat.value != active.lastCombatTarget ||
        int(flag.value) != active.lastCommandFlag || int(failed.value) != active.lastTargetCheck) {
        char message[512]{};
        std::snprintf(message,sizeof(message),
            "target trace elapsed=%llu ms commandFlag=%u boundStats=%p boundMatch=%d combatTarget=%p combatMatch=%d nativeTargetCheckFailed=%u reads=%d/%d/%d/%d",
            static_cast<unsigned long long>(now-active.issuedAt),unsigned(flag.value),
            reinterpret_cast<void*>(bound.value),bound.readable && bound.value==active.target.stats,
            reinterpret_cast<void*>(combat.value),combat.readable && combat.value==active.target.actor,
            unsigned(failed.value),bound.readable,combat.readable,flag.readable,failed.readable);
        Log(message,active.unit.actor,active.target.actor);
        active.lastBoundTarget=bound.value; active.lastCombatTarget=combat.value;
        active.lastCommandFlag=flag.value; active.lastTargetCheck=failed.value;
    }
    const auto attack = At<Address>(active.unit.stats,0xE0);
    if (attack.readable && attack.value != active.lastAttack) {
        const auto record=ReadRecord(attack.value);
        char message[320]{};
        std::snprintf(message,sizeof(message),"attack slot elapsed=%llu ms record=%s",
            static_cast<unsigned long long>(now-active.issuedAt),
            record.nameComplete ? record.name.data() : "<none/unavailable>");
        Log(message,active.unit.actor,active.target.actor); active.lastAttack=attack.value;
    }
    const auto hp=At<float>(active.target.stats,offset::stats::health);
    if (healthTrace && hp.readable && std::isfinite(hp.value) && hp.value!=active.lastTargetHealth) {
        char message[180]{};
        std::snprintf(message,sizeof(message),"ordered target health=%.3f elapsed=%llu ms (damage source not inferred)",
            hp.value,static_cast<unsigned long long>(now-active.issuedAt));
        Log(message,active.unit.actor,active.target.actor); active.lastTargetHealth=hp.value;
    }
}
inline void OnDamageCalculation(Address source, Address target, float damage, Address definition, char phase) {
    if (!source || !std::isfinite(damage) || damage<=0) return;
    for(auto& signal:orderSignals)if(signal.actor.load()==source && !signal.destroyed.load() && signal.target.load()==target)++signal.progress;
    if(!diagnostics)return;
    bool match=false;
    for(auto& signal:orderSignals) if(signal.actor.load()==source && !signal.destroyed.load() &&
        signal.target.load()==target && signal.damageReads.fetch_add(1)<8) {match=true;break;}
    if(!match)return;
    const auto record=ReadRecord(definition);
    char message[360]{};
    std::snprintf(message,sizeof(message),"ordered-pair damage calculation amount=%.3f phase=%d definition=%s",
        damage,int(phase),record.nameComplete ? record.name.data() : "<unavailable>");
    Log(message,source,target);
}
// Read-only observer of the already-hooked 3778D4 play update. Stage entries
// contain weak live stats pointers; never infer ownership from the circle target.
struct CrowdStages {
    std::array<Address,256> stats{};std::size_t count=0;
    unsigned stages=0;bool complete=false;
};
inline CrowdStages ReadCrowdStages(Address group) {
    CrowdStages result;const auto sentinel=group+8;
    auto node=At<Address>(group,0x10);
    for(unsigned n=0;node.readable && node.value && node.value!=sentinel && n<16;++n) {
        const auto count=At<unsigned>(node.value,0x10);
        if(!count.readable || !count.value || count.value>16)return result;
        ++result.stages;
        for(unsigned i=0;i<count.value;++i) {
            const auto stats=At<Address>(node.value,0x38+Address(i)*0x90);
            if(!stats.readable)return result;
            result.stats[result.count++]=stats.value;
        }
        node=At<Address>(node.value,8);
    }
    result.complete=node.readable && node.value==sentinel;
    return result;
}
inline void ObserveCommandPlay(Address group,Address circle,Address) {
    if(!crowdTrace || !crowdCommandsActive.load())return;
    const auto now=GetTickCount64();auto deadline=nextCrowdTrace.load();
    if(now<deadline)return;
    // Native 37D840/3778D4 identify circle+38 as target stats; +28 is its actor.
    const auto targetStats=At<Address>(circle,0x38);
    const auto targetActor=At<Address>(targetStats.value,0x28);
    if(!targetStats.readable || !targetActor.readable || !Live({targetActor.value,targetStats.value}))return;
    bool relevant=false;
    for(const auto& signal:orderSignals)
        if(signal.actor.load() && !signal.destroyed.load() && signal.target.load()==targetActor.value){relevant=true;break;}
    if(!relevant || !nextCrowdTrace.compare_exchange_strong(deadline,now+1000))return;
    const auto selected=ReadCrowdStages(group);
    const auto play=At<Address>(group,0x20);const auto phase=At<std::uint8_t>(group,0x28);
    const auto population=At<unsigned>(circle,0x1F4);
    const auto record=ReadRecord(play.value);
    unsigned issued=0,assigned=0,attacking=0,close=0,shown=0;
    std::array<std::array<char,512>,4> details{};
    for(const auto& signal:orderSignals) {
        const auto actor=signal.actor.load(),stats=signal.stats.load();
        if(!actor || signal.destroyed.load() || signal.target.load()!=targetActor.value || !Live({actor,stats}))continue;
        ++issued;bool slot=false;
        for(std::size_t i=0;i<selected.count;++i)if(selected.stats[i]==stats){slot=true;break;}
        if(slot)++assigned;
        const auto attack=At<Address>(stats,0xE0);
        if(attack.readable && attack.value)++attacking;
        const float distance=std::sqrt(DistanceSquared(actor,targetActor.value))*.01f;
        if(distance<=4.f)++close;
        if(shown<details.size()) {
            const auto name=ReadRecord(attack.value);
            std::snprintf(details[shown++].data(),details[0].size(),
                "crowd unit=%p assigned=%d distanceM=%.2f attack=%s attackRead=%d",
                reinterpret_cast<void*>(actor),slot,distance,
                name.nameComplete?name.name.data():"<none/unavailable>",attack.readable);
        }
    }
    char message[768]{};
    std::snprintf(message,sizeof(message),
        "crowd tick=%llu circle=%p group=%p play=%s phase=%u phaseRead=%d population=%u populationRead=%d stages=%u stageReadComplete=%d issued=%u assigned=%u attackRecords=%u within4m=%u shown=%u",
        static_cast<unsigned long long>(now),reinterpret_cast<void*>(circle),reinterpret_cast<void*>(group),
        record.nameComplete?record.name.data():"<none/unavailable>",unsigned(phase.value),phase.readable,
        population.value,population.readable,selected.stages,selected.complete,issued,assigned,attacking,close,shown);
    Log(message,0,targetActor.value,true);
    for(unsigned i=0;i<shown;++i)Log(details[i].data(),0,targetActor.value,true);
}
inline void AdvanceUnit(std::size_t slot, ULONGLONG now, Address player) {
    auto& active=orders[slot]; auto& signal=orderSignals[slot];
    if (!active.unit.actor) return;
    if (signal.destroyed.load()) { ReleaseUnit(slot); return; }
    if (active.player != player || !Friendly(active.unit) ||
        (!active.issued && !Enemy(active.target))) { ReleaseUnit(slot); return; }
    const auto entry = native.entry(active.unit.actor);
    const auto state = At<std::uint8_t>(entry, 0x80);
    if (active.issued) {
        if(entry && state.readable && state.value==2 && stallObserver && stallObserver(slot,now,player))return;
        TraceCommand(active,now,entry,slot==0);
        if (!entry || !state.readable || state.value != 2 || now >= active.finishUntil) {
            char message[160]{};
            std::snprintf(message, sizeof(message), "command ended state=%u valid=%d watchdog=%d elapsedMs=%llu",
                unsigned(state.value), state.readable, now >= active.finishUntil,
                static_cast<unsigned long long>(now-active.issuedAt));
            Log(message, active.unit.actor, active.target.actor,traceNativeCommandEnds); ReleaseUnit(slot);
        }
        return;
    }
    if (!entry) { if (now >= active.readyUntil) { Log("registration timed out"); ReleaseUnit(slot); } return; }
    const auto setup = ReadRecord(At<Address>(entry, 8).value);
    if (!setup.Named("Bodyguard_DefaultOrc") || !state.readable) {
        char message[320]{};
        std::snprintf(message, sizeof(message), "wrong/unreadable buddy setup=%s state=%u valid=%d",
            setup.nameComplete ? setup.name.data() : "<unavailable>", unsigned(state.value), state.readable);
        Log(message, active.unit.actor); ReleaseUnit(slot); return;
    }
    const bool hardBlocked = state.value == 0 &&
        (!native.blocked || CommandBlocked(active.unit.actor));
    const auto playerThreat = At<std::uint8_t>(player, 0x3914);
    // 186B308 identifies +3914 as HasThreat; B88DE0 exposes GetPlayerBeingTargeted.
    // Enemy awareness must not cancel this explicit separate-key command.
    const bool invalidPlayer = !ring_might::IsPlayer(At<Address>(player,0x26A8).value);
    if (active.lastState != state.value) {
        char message[256]{};
        std::snprintf(message, sizeof(message), "buddy setup=Bodyguard_DefaultOrc state=%u hardBlocked=%d playerHasThreat=%u threatRead=%d invalidPlayer=%d owns=%d",
            unsigned(state.value),hardBlocked,unsigned(playerThreat.value),playerThreat.readable,invalidPlayer,active.ownsItem);
        Log(message, active.unit.actor, active.target.actor); active.lastState = state.value;
    }
    if (diagnostics && hardBlocked &&
        (!active.lastBlockTrace || now-active.lastBlockTrace>=500)) {
        char message[320]{};
        const auto reason=ExplainBlock(active.unit.actor,0,siegeGate.installed);
        if (reason!=active.lastBlockReason) {
            std::snprintf(message,sizeof(message),"native blocker reason=%s",reason);
            Log(message,active.unit.actor,active.target.actor); active.lastBlockReason=reason;
        }
        active.lastBlockTrace=now;
    }
    if (!CanDispatch(state.value, active.ownsItem, hardBlocked, invalidPlayer)) {
        if (state.value == 0 && now < active.readyUntil) return;
        Log("buddy remained blocked or busy; cancelled", active.unit.actor); ReleaseUnit(slot); return;
    }
    // Bypass the player's summon action and player-awareness gate. The Orc's
    // native playbook still selects resistance, immunity, no-path, and timeout outcomes.
    const auto unit = active.unit.actor, target = active.target.actor;
    signal.target=active.target.actor; // Observe native work synchronous with mode entry too.
    native.mode(native.manager(), entry, 2, player, active.target.stats);
    active.issued = true; active.issuedAt=now; active.finishUntil = now + 15000;
    signal.damageReads=0; signal.target=active.target.actor;
    const auto current = native.entry(unit);
    Log(current && At<std::uint8_t>(current, 0x80).value == 2 ?
        "command issued; native mode=2" : "command mode unconfirmed", unit, target);
    TraceCommand(active,now,current,slot==0);
}
inline void Advance(ULONGLONG now,Address player) {
    if(stallTick)stallTick(now);
    if(orderAuditTick)orderAuditTick(now);
    const auto before=OrderCount();
    for(std::size_t i=0;i<orders.size();++i)AdvanceUnit(i,now,player);
    if(before && !OrderCount()) {crowdCommandsActive=false;DestroyMarker(orderMarker);Log("group command finished");}
}
struct Nearby {
    std::array<Binding,maxOrders> units{};std::size_t count=0;
};
inline Nearby CollectNearby(Address player,Address target) {
    std::array<Binding,256> snapshot;
    {std::lock_guard<std::mutex> lock(stateMutex);snapshot=candidates;}
    Nearby result;
    for(const auto& unit:snapshot) {
        if(!Friendly(unit) || DistanceSquared(player,unit.actor)>3000.f*3000.f ||
            DistanceSquared(target,unit.actor)>4000.f*4000.f)continue;
        const auto entry=native.entry(unit.actor);
        if(entry) {
            const auto setup=ReadRecord(At<Address>(entry,8).value);
            const auto state=At<std::uint8_t>(entry,0x80);
            if(!setup.Named("Bodyguard_DefaultOrc") || !state.readable || state.value!=1)continue;
        }
        bool duplicate=false;
        for(std::size_t i=0;i<result.count;++i)if(result.units[i].actor==unit.actor)duplicate=true;
        if(!duplicate && result.count<result.units.size())result.units[result.count++]=unit;
    }
    return result;
}
inline void RequestTarget(Address player, ULONGLONG now, const Binding& target) {
    if(OrderCount()) {Log("previous group command still active");return;}
    if(!Enemy(target)) {Log("no live aimed enemy");return;}
    StopRally();
    const auto nearby=CollectNearby(player,target.actor);
    if(!nearby.count) {Log("no available friendly Orc grunts nearby",0,target.actor);return;}
    ++crowdBatch;crowdCommandsActive=true;
    // Each add can relocate the native registry. Never retain registry pointers
    // across registration or mode transitions for different grunts.
    for(std::size_t i=0;i<nearby.count;++i) {
        const auto unit=nearby.units[i];
        if(!Enemy(target))break;
        if(!Friendly(unit))continue;
        const auto existing=native.entry(unit.actor);
        if(existing) {
            const auto setup=ReadRecord(At<Address>(existing,8).value);
            const auto state=At<std::uint8_t>(existing,0x80);
            if(!setup.Named("Bodyguard_DefaultOrc") || !state.readable || state.value!=1)continue;
        }
        auto& active=orders[i];auto& signal=orderSignals[i];
        active={unit,0,false,false,now+3000,0,target,player};
        signal.destroyed=false;signal.damageReads=0;signal.target=0;
        signal.stats=unit.stats;signal.actor=unit.actor;
        if(!existing) {
            const auto item=native.lookup("SetBuddy_Bodyguard_DefaultOrc");
            const auto inventory=At<Address>(unit.actor,0x2348).value;
            if(!item || !inventory || native.has(inventory,item)) {
                Log("command item unavailable or already pending; skipped",unit.actor,target.actor);
                ReleaseUnit(i);continue;
            }
            active.item=item;active.ownsItem=true;
            native.add(unit.actor,item);
            Log("temporary command item requested",unit.actor,target.actor);
        }
        AdvanceUnit(i,now,player);
    }
    std::size_t issued=0,pending=0;
    for(const auto& order:orders)if(order.unit.actor){if(order.issued)++issued;else ++pending;}
    char message[200]{};
    std::snprintf(message,sizeof(message),"group command nearby=%zu issued=%zu pending=%zu playerRadius=30m targetRadius=40m",nearby.count,issued,pending);
    Log(message,0,target.actor);
    crowdCommandsActive=OrderCount()!=0;nextCrowdTrace=0;
    if(OrderCount()) {
        if(previewMarker && PreviewTarget().actor==target.actor) {
            DestroyMarker(orderMarker);orderMarker=previewMarker;previewMarker=0;
            Log("HUD marker retained for group command",0,target.actor);
        } else MakeMarker(orderMarker,target);
    }
}
inline bool EmptyFollowQueue(Address component) {
    const auto begin=At<Address>(component,0x10),end=At<Address>(component,0x18);
    return begin.readable && end.readable && begin.value==end.value;
}
inline bool OwnRallyBand(const RallyOrder& order) {
    const auto component=At<Address>(order.actor,0x2D10);
    const auto current=At<Address>(component.value,8);
    const auto leader=At<Address>(order.band,0x28),type=At<Address>(order.band,0x70);
    return component.readable && component.value==order.component && current.readable &&
        current.value==order.band && leader.readable && leader.value==order.player &&
        type.readable && type.value==order.record;
}
inline void StopRallySlot(std::size_t slot) {
    auto order=rallyOrders[slot];rallyActors[slot]=0;rallyOrders[slot]={};
    DestroyMarker(order.marker);
    const bool destroyed=rallyDestroyed[slot].exchange(false);
    if(!order.actor || destroyed || !Alive({order.actor,order.stats}))return;
    // ClearFollow clears queued tasks too: never call it after a script has
    // replaced our band or queued another assignment.
    if(OwnRallyBand(order) && EmptyFollowQueue(order.component) && rallyNative.clear)
        rallyNative.clear(order.actor,true);
}
inline void StopRally() {
    for(std::size_t i=0;i<rallyOrders.size();++i)StopRallySlot(i);
}
inline void AdvanceRally(ULONGLONG now,Address player) {
    for(std::size_t i=0;i<rallyOrders.size();++i) {
        const auto order=rallyOrders[i];if(!order.actor)continue;
        if(rallyDestroyed[i].load() || !RallyFriendly({order.actor,order.stats}) ||
            player!=order.player || !Live({order.player,order.playerStats})) {StopRallySlot(i);continue;}
        if(!OwnRallyBand(order) || !EmptyFollowQueue(order.component)) {
            Log("rally yielded to changed native follow task",order.actor,player);
            StopRallySlot(i);continue;
        }
        const auto distance=DistanceSquared(player,order.actor);
        if(!std::isfinite(distance) || distance<=400.f*400.f || now>=order.until) {
            Log(distance<=400.f*400.f?"rally arrived within 4m":"rally timeout/invalid position",order.actor,player);
            StopRallySlot(i);continue;
        }
        if(diagnostics && now>=order.nextTrace) {
            char message[160]{};std::snprintf(message,sizeof(message),"rally distance=%.1fm nativeBand=matched",std::sqrt(distance)/100.f);
            Log(message,order.actor,player);rallyOrders[i].nextTrace=now+2000;
        }
    }
}
inline void RequestRally(Address player,ULONGLONG now) {
    const auto playerBinding=Bind(player);
    if(!Alive(playerBinding) || !ring_might::IsPlayer(playerBinding.stats) || !rallyNative.follow || !rallyNative.clear)return;
    const auto record=rallyNative.record();
    if(!record) {Log("rally unavailable: OrcBuddyFront record not loaded");return;}
    // Explicit F5 recalls our own attack orders; pre-existing native items are
    // preserved by Release. No player attack target is supplied to follow.
    Release();StopRally();aiming=false;ClearPreview();
    std::array<Binding,256> snapshot;
    {std::lock_guard<std::mutex> lock(stateMutex);snapshot=candidates;}
    std::size_t issued=0,alreadyNear=0,existingTask=0,refused=0;
    for(const auto& unit:snapshot) {
        if(!RallyFriendly(unit))continue;
        const auto distance=DistanceSquared(player,unit.actor);
        if(!std::isfinite(distance) || distance>3000.f*3000.f)continue;
        if(distance<=400.f*400.f) {++alreadyNear;continue;}
        const auto component=At<Address>(unit.actor,0x2D10);
        const auto owner=At<Address>(component.value,0),current=At<Address>(component.value,8);
        if(native.entry(unit.actor) || !component.readable || !component.value ||
            !owner.readable || owner.value!=unit.actor || !current.readable || current.value ||
            !EmptyFollowQueue(component.value)) {++existingTask;continue;}
        bool duplicate=false;
        for(const auto& order:rallyOrders)if(order.actor==unit.actor)duplicate=true;
        if(duplicate)continue;
        rallyNative.follow(unit.actor,player,record);
        const auto band=At<Address>(component.value,8);
        RallyOrder order{unit.actor,unit.stats,player,playerBinding.stats,component.value,band.value,record,now+20000,now+1000};
        if(!RallyFriendly(unit) || !band.readable || !band.value || !OwnRallyBand(order)) {++refused;Log("rally native follow unconfirmed",unit.actor,player);continue;}
        if(native.markerCreate && rallyNative.markerRecord) {
            const auto markerType=rallyNative.markerRecord();
            if(markerType)MakeMarker(order.marker,unit,markerType);
        }
        rallyDestroyed[issued]=false;rallyOrders[issued]=order;rallyActors[issued]=unit.actor;++issued;
        Log("rally native follow assigned",unit.actor,player);
    }
    char message[200]{};std::snprintf(message,sizeof(message),"rally group issued=%zu alreadyNear=%zu existingTask=%zu refused=%zu radius=30m arrival=4m timeout=20s",issued,alreadyNear,existingTask,refused);
    Log(message,0,player);
}
inline void Request(Address player, ULONGLONG now) { RequestTarget(player, now, Target()); }
inline void __fastcall BuffDetour(Address buff, Address stats, Address context) {
    PLH::FnCast(buffTrampoline, static_cast<BuffFn>(nullptr))(buff, stats, context);
    Observe(Bind(At<Address>(stats, 0x28).value));
    ObserveEnemy(Bind(At<Address>(stats, 0x28).value));
}
inline void __fastcall OutlineDetour(Address actor, bool enabled) {
    PLH::FnCast(outlineTrampoline, static_cast<OutlineFn>(nullptr))(actor, enabled);
    Mark(Bind(actor), enabled);
    { std::lock_guard<std::mutex> lock(stateMutex);
      if (preview.unit.actor == actor) preview.nativeWants = enabled; }
}
inline void ForgetActor(Address actor) {
    for(auto& signal:orderSignals)if(signal.actor.load()==actor)signal.destroyed=true;
    for(std::size_t i=0;i<rallyOrders.size();++i)if(rallyActors[i].load()==actor)rallyDestroyed[i]=true;
    if (previewActor.load() == actor) previewDestroyed = true;
    std::lock_guard<std::mutex> lock(stateMutex);
    for (auto& entry : candidates) if (entry.actor == actor) entry = {};
    for (auto& entry : outlined) if (entry.actor == actor) entry = {};
    for (auto& entry : enemies) if (entry.actor == actor) entry = {};
}
inline void __fastcall CleanupDetour(Address actor) {
    ForgetActor(actor);
    PLH::FnCast(cleanupTrampoline, static_cast<CleanupFn>(nullptr))(actor);
}
inline void(*onPlayerUpdate)()=nullptr; // Optional read-only diagnostic hotkey poll.
inline void __fastcall UpdateDetour(Address first, Address actorReference, Address manager, Address fourth) {
    PLH::FnCast(updateTrampoline, static_cast<UpdateFn>(nullptr))(first, actorReference, manager, fourth);
    if (updating) return;
    const auto player = Read<Address>(actorReference).value;
    const auto binding = Bind(player);
    if (!Live(binding) || !ring_might::IsPlayer(binding.stats)) return;
    std::unique_lock<std::mutex> lock(updateMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    updating = true;
    const auto now = GetTickCount64();
    if(onPlayerUpdate)onPlayerUpdate();
    Advance(now, player);AdvanceRally(now,player);
    const bool rallyDown=(GetAsyncKeyState(rallyKey)&0x8000)!=0;
    const bool rallyPressed=RisingEdge(rallyDown,rallyKeyDown);
    if(Foreground() && rallyPressed) {
        RequestRally(player,now);updating=false;return;
    }
    const bool down = (GetAsyncKeyState(commandKey) & 0x8000) != 0;
    const bool focused = Foreground();
    const bool stealth = !requireStealthKey || (GetAsyncKeyState(stealthKey) & 0x8000);
    if (!focused || !stealth) {
        aiming = false; ClearPreview();
    } else if (down) {
        const bool firstAim = !aiming;
        if (firstAim) { aiming = true; nextAimUpdate = 0; Log("aim mode; release command key to issue"); }
        if (now >= nextAimUpdate) { SetPreview(AimTarget(player, firstAim)); nextAimUpdate = now + 80; }
        else { const auto chosen = PreviewTarget(); SetPreview(Enemy(chosen) ? chosen : Binding{}); }
    } else if (aiming) {
        const auto target = PreviewTarget(); aiming = false;
        RequestTarget(player, now, target); ClearPreview();
    }
    updating = false;
}
inline bool Remove() {
    if(grunt_aggression::playObserver==&ObserveCommandPlay)grunt_aggression::playObserver=nullptr;
    // Clear while the outline trampoline is still allocated. Caller quiesces threads.
    if(stallShutdown)stallShutdown();
    ClearPreview(); aiming = false; Release(); StopRally();
    bool ok = true;
    for (auto* hook : {&updateHook, &outlineHook, &buffHook, &cleanupHook}) {
        if (*hook && !(*hook)->unHook()) { ok = false; continue; }
        hook->reset();
    }
    if (!siegeGate.Remove()) { ok=false; Log("siege call-site restore FAILED"); }
    if (ok) {
        native.outline = nullptr;
        std::lock_guard<std::mutex> lock(stateMutex); candidates = {}; outlined = {}; enemies = {};
    }
    return ok;
}
inline bool Install(int key = VK_F6, bool log = true, bool requireHeldStealthKey = true,
    int stealth = VK_LSHIFT, bool printToConsole = true, int rally = VK_F5) {
    if (buffHook || outlineHook || updateHook || cleanupHook || siegeGate.memory || grunt_command_probe::buffHook || key < 1 || key > 255 ||
        stealth < 1 || stealth > 255 || key == stealth || rally<1 || rally>255 || rally==key || rally==stealth) return false;
    commandKey = key; diagnostics = log; consoleOutput = printToConsole;
    requireStealthKey = requireHeldStealthKey;
    stealthKey = stealth; aiming = false; logCount = 0;rallyKey=rally;rallyKeyDown=false;
    const auto base = ring_might::Base();
    native = {reinterpret_cast<ItemFn>(base + 0x1E6B88),
        reinterpret_cast<ItemFn>(base + 0x4F12B4),
        reinterpret_cast<HasFn>(base + 0x24DF7C),
        reinterpret_cast<ModeFn>(base + 0x1AA4B90)};
    native.blocked = reinterpret_cast<BlockedFn>(base + 0x1AA49D0);
    native.state = reinterpret_cast<StateFn>(base + 0x2FD93C);
    rallyNative={reinterpret_cast<FollowFn>(base+0x1A8DD04),reinterpret_cast<ClearFollowFn>(base+0x1A8B6CC)};
    native.markerCreate = reinterpret_cast<MarkerCreateFn>(base + 0x60CE2C);
    native.markerDelete = reinterpret_cast<MarkerDeleteFn>(base + 0x4EE4C8);
    native.markerType = reinterpret_cast<MarkerSetFn>(base + 0x3576A4);
    native.markerBind = reinterpret_cast<MarkerSetFn>(base + 0x357C2C);
    native.markerEnable = reinterpret_cast<MarkerEnableFn>(base + 0x2F47B0);
    if (!siegeGate.Install(base+0x1AA4A02,base+0x440F68,&ExemptSiegeGate)) {
        Log(siegeGate.error); Log("siege exemption install FAILED; command not installed"); return false;
    }
    buffHook = std::make_unique<PLH::x64Detour>(base + 0x452ED8, reinterpret_cast<Address>(&BuffDetour), &buffTrampoline);
    outlineHook = std::make_unique<PLH::x64Detour>(base + 0x76D804, reinterpret_cast<Address>(&OutlineDetour), &outlineTrampoline);
    updateHook = std::make_unique<PLH::x64Detour>(base + 0x427A8C, reinterpret_cast<Address>(&UpdateDetour), &updateTrampoline);
    cleanupHook = std::make_unique<PLH::x64Detour>(base + 0x360988, reinterpret_cast<Address>(&CleanupDetour), &cleanupTrampoline);
    if (!cleanupHook->hook()) {
        cleanupHook.reset(); buffHook.reset(); outlineHook.reset(); updateHook.reset();
        Remove(); Log("cleanup hook FAILED"); return false;
    }
    if (!buffHook->hook()) { buffHook.reset(); outlineHook.reset(); updateHook.reset(); Remove(); Log("buff hook FAILED"); return false; }
    if (!outlineHook->hook()) { outlineHook.reset(); updateHook.reset(); Remove(); Log("outline hook FAILED"); return false; }
    native.outline = PLH::FnCast(outlineTrampoline, static_cast<OutlineFn>(nullptr));
    if (!updateHook->hook()) { updateHook.reset(); Remove(); Log("update hook FAILED"); return false; }
    nextCrowdTrace=0;
    if(crowdTrace && grunt_aggression::updateHook) {
        grunt_aggression::playObserver=&ObserveCommandPlay;
        Log("crowd trace enabled; read-only play snapshots at most once per second",0,0,true);
    } else if(crowdTrace) Log("crowd trace unavailable: aggression update hook is inactive",0,0,true);
    Log("v10.2 installed; immediate all-grunt attack dispatch restored; Olog rally and follower markers");
    return true;
}
} // namespace sow::grunt_command
