#include "WorldState.hpp"
#include "../game/GameState.hpp"
#include "../game/StageFlow.hpp"
#include "../game/CameraEffects.hpp"
#include "../game/EnemyCallbacks.hpp"
#include "../game/HostilePool.hpp"
#include "../game/FriendlyPool.hpp"
#include "../game/LaserWorld.hpp"
#include "../game/ItemPresentation.hpp"
#include "../game/UfoEffects.hpp"
#include "../game/StageBackgroundEffect.hpp"
#include "../game/PresentationTracking.hpp"
#include <tuple>
namespace th12::multiplayer {
namespace {
auto stateReferences() {
  return std::tie(pilots, active, paused, finished, restartRequested,returnRequested,saveRequested,menuConfirm,
    menuSelection,menuAction,menuPreviousHeld,menuAge,menuHoldAge,storyPreviousHeld,currentSeat, tickNumber, wipeFrames, lifeFragments,
    bombFragments, teamRank, damageRemainders, bulletGrazeMasks, random, displayRandom,
    nextDetachedAnimation, bullets, itemManager, ufoManager, messageVM, spellState,
    spellAnimationKeys, messageAnimationKeys, messageAnimationOwners, messageEvents,
    unsupported, animationUnsupported, referenceFaults, runtimeFaults, globals, phasePlayerCounters,
    frame, difficulty, stageNumber, backgroundFrame, gameSpeed, backgroundVisible, stageState, score,
    eventBits, dialogue, spell, ended, oracleFixtureFlags, replayStageGlobals, nextEnemyId,temporaryEnemyOwner,
    nextProjectileId, nextItemId, lastSpawnedEnemyId, backgroundEnemyOrigin, fullGameFlow,
    cameraEffects, bossHud, bossMusic, musicSerial, hostilePool, scorePopups, itemPresentation,
    stageBackgroundEffect, presentation::epoch, presentation::ownerRevisions, sound_system::queue,
    bulletWorld.playerX, bulletWorld.playerY);
}
template<class... T> auto copyValues(const std::tuple<T&...>& values) { return std::tuple<T...>(values); }
}
struct WorldState::Data {
  decltype(copyValues(stateReferences())) values = copyValues(stateReferences());
  anm_logic::Scene scene = animationScene;
  laser::Manager lasers = laserManager;
  UfoEffects effects = ufoEffects;
  std::vector<Enemy> enemies;
  Data() { for (const auto& enemy : th12::enemies) enemies.push_back(*enemy); }
};
WorldState::WorldState() : data_(std::make_shared<Data>()) {}
void WorldState::restore() const {
  stateReferences() = data_->values;
  animationScene = data_->scene; laserManager = data_->lasers; ufoEffects = data_->effects;
  th12::enemies.clear();
  for (const auto& enemy : data_->enemies) th12::enemies.push_back(std::make_unique<Enemy>(enemy));
  refreshHostilePointers(); refreshFriendlyPointers();
}
}
