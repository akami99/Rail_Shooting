#pragma once
#include <Windows.h>
#include <memory> // std::unique_ptr用に必要

#include "ImGuiManager.h"

// 前方宣言
class Win32Window;

// ゲーム全体を管理するフレームワーククラス
class RAFramework {
public:
    virtual ~RAFramework() = default;

    // 実行関数
    void Run();

    // 継承先で実装する関数
    virtual void Initialize();
    virtual void Finalize();
    virtual void Update() = 0;
    virtual void Draw() = 0;
    // 終了フラグ
    virtual bool IsEndRequest() {
        return endRequest_;
    }

protected: // 継承先から操作するためのメンバ関数
    void RequestEnd() { endRequest_ = true; }
    ImGuiManager* GetImGuiManager() const { return imGuiManager_.get(); }

private:
    bool endRequest_ = false;

    // --- 基盤システム (これらはFrameworkが持つ) ---
    // ImGuiマネージャ
    std::unique_ptr<ImGuiManager> imGuiManager_;
};
