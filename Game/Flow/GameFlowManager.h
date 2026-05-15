#pragma once

#include <functional>

namespace Revora {

/// ゲーム全体の状態遷移
/// Title → CourseSelect はフェーズ7 (UI) で実装するため、
/// 現段階では Countdown → Racing → Finished → Result を使用する
enum class GameState {
    Title,         // タイトル画面 (Phase 7)
    CourseSelect,  // コース選択 (Phase 7)
    Countdown,     // カウントダウン中
    Racing,        // レース中
    Finished,      // 全ラップ完了 (ゴースト保存等の処理)
    Result         // リザルト画面 (Phase 7)
};

/// 各状態で呼ばれるコールバック群
struct FlowStateCallbacks {
    std::function<void()>      onEnter;   // 状態に入った瞬間
    std::function<void(float)> onUpdate;  // 毎フレーム (dt を受け取る)
    std::function<void()>      onExit;    // 状態を出る瞬間
};

/// ゲーム状態遷移のステートマシン
/// Application がコールバックを登録し、状態遷移を GameFlowManager に委譲する
/// これにより Application の肥大化を防ぎ、状態遷移ロジックを一箇所に集約する
class GameFlowManager {
public:
    /// 初期状態を設定する
    void Initialize(GameState initialState);

    /// 特定の状態にコールバックを登録する
    void SetCallbacks(GameState state, FlowStateCallbacks callbacks);

    /// 毎フレーム呼ばれる
    void Update(float dt);

    /// 状態を遷移させる (Enter/Exit コールバックが呼ばれる)
    void TransitionTo(GameState newState);

    /// 現在の状態を取得する
    GameState GetCurrentState() const { return currentState_; }

    // --- 状態に基づくヘルパー (Application が if 文を並べずに済む) ---

    /// 車両物理を更新すべきか (Racing 中のみ)
    bool ShouldUpdateVehicle() const;

    /// プレイヤー入力を受け付けるか (Racing 中のみ)
    bool ShouldAcceptInput() const;

    /// RaceManager を更新すべきか (Countdown / Racing 中)
    bool ShouldUpdateRace() const;

private:
    static constexpr int kStateCount = 6;

    FlowStateCallbacks callbacks_[kStateCount] = {};
    GameState currentState_ = GameState::Countdown;

    /// GameState を配列インデックスに変換する
    static int StateToIndex(GameState state);
};

} // namespace Revora
