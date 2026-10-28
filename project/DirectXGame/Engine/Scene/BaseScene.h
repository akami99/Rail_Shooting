#pragma once
#include "IScene.h"

class SceneManager;

// BaseScene.h (共通機能。ISceneを継承)
class BaseScene : public IScene {
public:
    virtual ~BaseScene() = default;
    void DrawOffscreen() override {}
    virtual void DrawForeground() {}
};