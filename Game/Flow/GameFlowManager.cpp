#include "GameFlowManager.h"

#include <SDL.h>

namespace Revora {

void GameFlowManager::Initialize(GameState initialState)
{
    currentState_ = initialState;

    // 初期状態の Enter コールバックを呼ぶ
    int idx = StateToIndex(currentState_);
    if (callbacks_[idx].onEnter) {
        callbacks_[idx].onEnter();
    }
}

void GameFlowManager::SetCallbacks(GameState state, FlowStateCallbacks callbacks)
{
    callbacks_[StateToIndex(state)] = std::move(callbacks);
}

void GameFlowManager::Update(float dt)
{
    int idx = StateToIndex(currentState_);
    if (callbacks_[idx].onUpdate) {
        callbacks_[idx].onUpdate(dt);
    }
}

void GameFlowManager::TransitionTo(GameState newState)
{
    if (newState == currentState_) {
        return;
    }

    // 現在の状態の Exit コールバック
    int oldIdx = StateToIndex(currentState_);
    if (callbacks_[oldIdx].onExit) {
        callbacks_[oldIdx].onExit();
    }

    GameState oldState = currentState_;
    currentState_ = newState;

    SDL_Log("GameFlow: %d -> %d",
            static_cast<int>(oldState),
            static_cast<int>(newState));

    // 新しい状態の Enter コールバック
    int newIdx = StateToIndex(newState);
    if (callbacks_[newIdx].onEnter) {
        callbacks_[newIdx].onEnter();
    }
}

bool GameFlowManager::ShouldUpdateVehicle() const
{
    return currentState_ == GameState::Racing;
}

bool GameFlowManager::ShouldAcceptInput() const
{
    return currentState_ == GameState::Racing;
}

bool GameFlowManager::ShouldUpdateRace() const
{
    return currentState_ == GameState::Countdown ||
           currentState_ == GameState::Racing;
}

int GameFlowManager::StateToIndex(GameState state)
{
    return static_cast<int>(state);
}

} // namespace Revora
