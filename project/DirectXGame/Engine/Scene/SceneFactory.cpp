#include "SceneFactory.h"
#include "Title/TitleScene.h"
#include "Demo/DemoScene.h"
#include "Game/ShootingScene.h"
#include "Clear/ClearScene.h"


std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
    // 文字列に応じて、適切なシーンを make_unique して返す
    if (sceneName == "TITLE") {
        return std::make_unique<TitleScene>();
    } else if (sceneName == "DEMO") {
        return std::make_unique<DemoScene>();
    } else if (sceneName == "SHOOTING") {
        return std::make_unique<ShootingScene>();
    } else if (sceneName == "CLEAR") {
        return std::make_unique<ClearScene>();
    }

    // 未知のシーン名なら nullptr を返す（あるいはアサートで止める）
    return nullptr;
}