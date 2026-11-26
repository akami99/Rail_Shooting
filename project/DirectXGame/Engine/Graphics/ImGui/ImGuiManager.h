#pragma once

#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#endif

// ImGuiの管理
class ImGuiManager {
public:
  // 初期化
  void Initialize();
  // 終了
  void Finalize();

  // ImGuiの受付開始
  void Begin();
  // ImGuiの受付終了
  void End();
  // 描画
  void Draw();
};
