#define USE_MATH_DEFINES // 数学定義を使用する
#include <cmath>
#include <numbers>

#include "../Camera/DebugCamera.h"
#include "../../Core/Utility/Math/Functions/MathUtils.h"
#include "ImGuiManager.h"
#include "../Input/Input.h"

using namespace MathUtils;
using namespace MathGenerators;

// =========================================================================
// 操作モード State Pattern 実装
// =========================================================================
Vector3 DebugCamera::ComputeLocalMove(float moveSpeed) {
  Vector3 localMove = {0.0f, 0.0f, 0.0f};
  if (Input::GetInstance()->IsKeyDown(DIK_W) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    localMove.z += moveSpeed;
  } else if (Input::GetInstance()->IsKeyDown(DIK_S) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    localMove.z -= moveSpeed;
  }
  if (Input::GetInstance()->IsKeyDown(DIK_A) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    localMove.x -= moveSpeed;
  } else if (Input::GetInstance()->IsKeyDown(DIK_D) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    localMove.x += moveSpeed;
  }
  if (Input::GetInstance()->IsKeyDown(DIK_Q)) {
    localMove.y += moveSpeed;
  } else if (Input::GetInstance()->IsKeyDown(DIK_E)) {
    localMove.y -= moveSpeed;
  }
  return localMove;
}

class DebugCamera::FirstPersonModeState final : public DebugCamera::IDebugCameraModeState {
public:
  void OnEnter(DebugCamera& camera) override {
    // ピボット回転 -> 一人称視点 に切り替える際、現在のオービット角を引き継ぐ
    camera.rotation_ = camera.cameraEulerRotation_;
  }

  void Update(DebugCamera& camera) override;
};

class DebugCamera::OrbitModeState final : public DebugCamera::IDebugCameraModeState {
public:
  void OnEnter(DebugCamera& camera) override {
    // 一人称視点 -> ピボット回転 に切り替える際、現在の視点前方から target_ を逆算
    Vector3 cameraForward = {camera.matRot_.m[2][0], camera.matRot_.m[2][1], camera.matRot_.m[2][2]};
    cameraForward = Normalize(cameraForward);
    camera.target_ = camera.translation_ + cameraForward * camera.orbitDistance_;
    camera.cameraEulerRotation_ = camera.rotation_;
  }

  void Update(DebugCamera& camera) override {
#ifdef USE_IMGUI
    if (Input::GetInstance()->IsKeyTriggered(DIK_TAB)) {
      camera.ChangeModeState(std::make_unique<FirstPersonModeState>());
      return;
    }

    const float rotateSpeed = 0.02f;
    Vector3 deltaRotationEuler = {0.0f, 0.0f, 0.0f};
    if (Input::GetInstance()->IsKeyDown(DIK_W) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.x += rotateSpeed;
    } else if (Input::GetInstance()->IsKeyDown(DIK_S) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.x -= rotateSpeed;
    }
    if (Input::GetInstance()->IsKeyDown(DIK_A) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.y += rotateSpeed;
    } else if (Input::GetInstance()->IsKeyDown(DIK_D) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.y -= rotateSpeed;
    }

    camera.cameraEulerRotation_ += deltaRotationEuler;

    float PI = std::numbers::pi_v<float>;
    camera.cameraEulerRotation_.x = std::fmax(
        -PI / 2.0f + 0.01f,
        std::fmin(PI / 2.0f - 0.01f, camera.cameraEulerRotation_.x));

    Matrix4x4 orbitRotationMatrix = MakeRotateYMatrix(camera.cameraEulerRotation_.y) *
                                    MakeRotateXMatrix(camera.cameraEulerRotation_.x);
    Vector3 localOffset = {0.0f, 0.0f, -camera.orbitDistance_};
    camera.translation_ = TransformVector(localOffset, orbitRotationMatrix);
    camera.translation_ += camera.target_;

    Vector3 cameraZAxis = Normalize(camera.target_ - camera.translation_);
    Vector3 worldYAxis = {0.0f, 1.0f, 0.0f};
    if (std::abs(Dot(cameraZAxis, worldYAxis)) > 0.999f) {
      worldYAxis = {0.0f, 0.0f, 1.0f};
    }

    Vector3 cameraXAxis = Normalize(Cross(worldYAxis, cameraZAxis));
    Vector3 cameraYAxis = Cross(cameraZAxis, cameraXAxis);

    camera.matRot_.m[0][0] = cameraXAxis.x; camera.matRot_.m[0][1] = cameraXAxis.y; camera.matRot_.m[0][2] = cameraXAxis.z; camera.matRot_.m[0][3] = 0.0f;
    camera.matRot_.m[1][0] = cameraYAxis.x; camera.matRot_.m[1][1] = cameraYAxis.y; camera.matRot_.m[1][2] = cameraYAxis.z; camera.matRot_.m[1][3] = 0.0f;
    camera.matRot_.m[2][0] = cameraZAxis.x; camera.matRot_.m[2][1] = cameraZAxis.y; camera.matRot_.m[2][2] = cameraZAxis.z; camera.matRot_.m[2][3] = 0.0f;
    camera.matRot_.m[3][0] = 0.0f;          camera.matRot_.m[3][1] = 0.0f;          camera.matRot_.m[3][2] = 0.0f;          camera.matRot_.m[3][3] = 1.0f;

    camera.rotation_ = camera.cameraEulerRotation_;

    Vector3 localMove = ComputeLocalMove(0.5f);
    Vector3 worldMove = TransformVector(localMove, camera.matRot_);
    camera.target_ += worldMove;

    Vector3 rotatedOffset = TransformVector(localOffset, camera.matRot_);
    camera.translation_ = camera.target_ + rotatedOffset;

    ImGui::Text("Mode: Orbit Camera");
    ImGui::Text("Target: (%.2f, %.2f, %.2f)", camera.target_.x, camera.target_.y, camera.target_.z);
    ImGui::Text("Orbit Distance: %.2f", camera.orbitDistance_);
    ImGui::Text("W/S + LSHIFT: Pivot Up/Down");
    ImGui::Text("A/D + LSHIFT: Pivot Left/Right");
    ImGui::Text("W/S: Move Target Forward/Backward");
    ImGui::Text("A/D: Move Target Left/Right");
    ImGui::Text("Q/E: Move Target Up/Down");
#else
    (void)camera;
#endif
  }
};

void DebugCamera::FirstPersonModeState::Update(DebugCamera& camera) {
#ifdef USE_IMGUI
  if (Input::GetInstance()->IsKeyTriggered(DIK_TAB)) {
    camera.ChangeModeState(std::make_unique<OrbitModeState>());
    return;
  }

  const float rotateSpeed = 0.02f;
  Vector3 deltaRotationEuler = {0.0f, 0.0f, 0.0f};
  if (Input::GetInstance()->IsKeyDown(DIK_W) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    deltaRotationEuler.x -= rotateSpeed;
  } else if (Input::GetInstance()->IsKeyDown(DIK_S) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    deltaRotationEuler.x += rotateSpeed;
  }
  if (Input::GetInstance()->IsKeyDown(DIK_A) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    deltaRotationEuler.y -= rotateSpeed;
  } else if (Input::GetInstance()->IsKeyDown(DIK_D) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
    deltaRotationEuler.y += rotateSpeed;
  }

  camera.rotation_ += deltaRotationEuler;

  float PI = std::numbers::pi_v<float>;
  camera.rotation_.x = std::fmax(-PI / 2.0f, std::fmin(PI / 2.0f, camera.rotation_.x));

  camera.matRot_ = MakeRotateYMatrix(deltaRotationEuler.y) * camera.matRot_;
  camera.matRot_ = camera.matRot_ * MakeRotateXMatrix(deltaRotationEuler.x);

  Vector3 localMove = ComputeLocalMove(0.2f);
  Vector3 worldMove = TransformVector(localMove, camera.matRot_);
  camera.translation_ += worldMove;

  ImGui::Text("Mode: First-Person Camera");
  ImGui::Text("W/S + LSHIFT: Pitch Up/Down");
  ImGui::Text("A/D + LSHIFT: Yaw Left/Right");
  ImGui::Text("W/S: Move Forward/Backward");
  ImGui::Text("A/D: Move Left/Right");
  ImGui::Text("Q/E: Move Up/Down\n\n\n\n");
#else
  (void)camera;
#endif
}

void DebugCamera::ChangeModeState(std::unique_ptr<IDebugCameraModeState> nextState) {
  modeState_ = std::move(nextState);
  if (modeState_) {
    modeState_->OnEnter(*this);
  }
}

DebugCamera::DebugCamera() { Initialize(); }

void DebugCamera::Initialize() {
  Camera::Initialize();

  // 初期値の計算
  rotation_ = {0, 0, 0};
  translation_ = {0, 0, -20};
  matRot_ = MakeIdentity4x4(); // 初期回転行列は単位行列
  Matrix4x4 worldMatrix = MakeAffineMatrix({1.0f, 1.0f, 1.0f}, matRot_, translation_);
  Matrix4x4 viewMatrix = Inverse(worldMatrix);
  SetTranslate(translation_);
  SetRotate(rotation_);
  UpdateProjectionAndConstantBuffer(worldMatrix, viewMatrix);

  cameraEulerRotation_ = {0.0f, 0.0f, 0.0f}; // カメラのオイラー角初期化
  target_ = {0.0f, 0.0f, 0.0f};              // 初期ターゲットは原点
  orbitDistance_ = 50.0f;                    // オービット距離の初期値
  ChangeModeState(std::make_unique<FirstPersonModeState>());
}

void DebugCamera::Update() {
#ifdef USE_IMGUI
  // デバッグ用ImGuiウィンドウ

  // 初期表示位置を設定(画面左上)
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);

  ImGui::Begin("Debug Camera Control");
  ImGui::Text("R: Reset Camera");
  ImGui::Text("Mode Change: TAB");

  if (!modeState_) {
    ChangeModeState(std::make_unique<FirstPersonModeState>());
  }
  modeState_->Update(*this);

  // --- 共通の処理 ---
  
  // リセット
  if (Input::GetInstance()->IsKeyTriggered(DIK_R)) {
    Initialize(); // カメラを初期化
  }

  // カメラのワールド行列を再計算
  Matrix4x4 worldMatrix = MakeAffineMatrix({1.0f, 1.0f, 1.0f}, matRot_, translation_);

  // ビュー行列を更新
  Matrix4x4 viewMatrix = Inverse(worldMatrix);
  SetTranslate(translation_);
  SetRotate(rotation_);
  UpdateProjectionAndConstantBuffer(worldMatrix, viewMatrix);

  ImGui::Separator();
  ImGui::Text("Translation: (%.2f, %.2f, %.2f)", translation_.x, translation_.y,
              translation_.z);
  ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", rotation_.x, rotation_.y,
              rotation_.z);
  ImGui::Text("World Matrix: \n%.2f %.2f %.2f %.2f\n%.2f %.2f %.2f %.2f\n%.2f "
              "%.2f %.2f %.2f\n%.2f %.2f %.2f %.2f",
              worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2],
              worldMatrix.m[0][3], worldMatrix.m[1][0], worldMatrix.m[1][1],
              worldMatrix.m[1][2], worldMatrix.m[1][3], worldMatrix.m[2][0],
              worldMatrix.m[2][1], worldMatrix.m[2][2], worldMatrix.m[2][3],
              worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2],
              worldMatrix.m[3][3]);
  ImGui::Text("View Matrix: \n%.2f %.2f %.2f %.2f\n%.2f %.2f %.2f %.2f\n%.2f "
              "%.2f %.2f %.2f\n%.2f %.2f %.2f %.2f",
              viewMatrix.m[0][0], viewMatrix.m[0][1], viewMatrix.m[0][2],
              viewMatrix.m[0][3], viewMatrix.m[1][0], viewMatrix.m[1][1],
              viewMatrix.m[1][2], viewMatrix.m[1][3], viewMatrix.m[2][0],
              viewMatrix.m[2][1], viewMatrix.m[2][2], viewMatrix.m[2][3],
              viewMatrix.m[3][0], viewMatrix.m[3][1], viewMatrix.m[3][2],
              viewMatrix.m[3][3]);
  ImGui::Text(
      "matRot_:\n%.2f %.2f %.2f %.2f\n%.2f %.2f %.2f %.2f\n%.2f %.2f %.2f "
      "%.2f\n%.2f %.2f %.2f %.2f",
      matRot_.m[0][0], matRot_.m[0][1], matRot_.m[0][2], matRot_.m[0][3],
      matRot_.m[1][0], matRot_.m[1][1], matRot_.m[1][2], matRot_.m[1][3],
      matRot_.m[2][0], matRot_.m[2][1], matRot_.m[2][2], matRot_.m[2][3],
      matRot_.m[3][0], matRot_.m[3][1], matRot_.m[3][2], matRot_.m[3][3]);
  ImGui::End();

#endif
}