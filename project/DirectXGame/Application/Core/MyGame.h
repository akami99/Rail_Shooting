#pragma once
#include "RAFramework.h"

#include "DebugCamera.h"
#include "MathTypes.h"
#include <vector>

#include "RenderTexture.h"
#include "PostProcessManager.h"
#include <memory>

class MyGame : public RAFramework {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    std::unique_ptr<RenderTexture> renderTexture_;
};
