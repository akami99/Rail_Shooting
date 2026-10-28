#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include <memory> // std::unique_ptr用に必要
#include <string>

// シーン管理
class SceneManager {
public:
    // 【Passkey Idiom】
    struct Token {
    private:
        friend class SceneManager;
        Token() {}
    };

    static SceneManager* GetInstance();

    // コンストラクタ(隠蔽)
    explicit SceneManager(Token);

    // 終了処理
    void Finalize();

    // 破棄用の関数
    static void Destroy();

    // 更新処理（シーンの切り替えチェックもここで行う）
    void Update();

    // オフスクリーン描画処理
    void DrawOffscreen();

    // ポストエフェクト影響なしの描画処理
    void DrawForeground();

    // 描画処理
    void Draw();

    // 文字列で次のシーンを予約する
    void ChangeScene(const std::string& sceneName);

    // ファクトリーをセットする（MyGame初期化時などに呼ぶ）
    void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory) {
        sceneFactory_ = std::move(sceneFactory);
    }

private:
    ~SceneManager() = default;
    static std::unique_ptr<SceneManager> instance_;
    friend std::default_delete<SceneManager>;

    // シーン工場
    std::unique_ptr<AbstractSceneFactory> sceneFactory_;

    // 現在のシーン
    std::unique_ptr<BaseScene> currentScene_ = nullptr;
    // 次のシーン（予約用）
    std::unique_ptr<BaseScene> nextScene_ = nullptr;
};