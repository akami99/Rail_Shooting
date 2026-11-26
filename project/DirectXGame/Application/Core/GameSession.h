#pragma once

/// <summary>
/// シーン間で共有・引き継ぎするゲームセッションデータを管理するクラス
/// </summary>
class GameSession {
public:
    static GameSession& GetInstance() {
        static GameSession instance;
        return instance;
    }

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    GameSession(GameSession&&) = delete;
    GameSession& operator=(GameSession&&) = delete;

    // クリア結果の設定・取得
    void SetClearResult(int score, float remainingTime) {
        clearScore_ = score;
        clearRemainingTime_ = remainingTime;
    }
    int GetClearScore() const { return clearScore_; }
    float GetClearRemainingTime() const { return clearRemainingTime_; }

    // デモモードの設定・取得
    void SetDemoMode(bool isDemo) { isDemoMode_ = isDemo; }
    bool IsDemoMode() const { return isDemoMode_; }

private:
    GameSession() = default;
    ~GameSession() = default;

    int clearScore_ = 0;
    float clearRemainingTime_ = 0.0f;
    bool isDemoMode_ = false;
};
