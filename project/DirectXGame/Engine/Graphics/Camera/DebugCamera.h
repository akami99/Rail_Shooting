#pragma once
#include "Camera.h"
#include "Math/MathTypes.h"
#include "Math/Matrix/MatrixGenerators.h"
#include <memory>

/// <summary>
/// デバッグカメラ
/// </summary>
class DebugCamera : public Camera {
private:
  // 操作モードの State Pattern 基底インターフェース（内部実装詳細としてカプセル化）
  class IDebugCameraModeState {
  public:
    virtual ~IDebugCameraModeState() = default;
    virtual void OnEnter(DebugCamera& camera) = 0;
    virtual void Update(DebugCamera& camera) = 0;
  };
  class FirstPersonModeState;
  class OrbitModeState;

  void ChangeModeState(std::unique_ptr<IDebugCameraModeState> nextState);
  static Vector3 ComputeLocalMove(float moveSpeed);

  // X,Y,Z周りのローカル回転角
  Vector3 rotation_ = {0, 0, 0};
  // ローカル座標
  Vector3 translation_ = {0, 0, -50};
  // 累積回転行列
  Matrix4x4 matRot_;
  // 操作モードステート
  std::unique_ptr<IDebugCameraModeState> modeState_;
  // オービットカメラのオイラー角 (ピボット回転用)
  Vector3 cameraEulerRotation_;
  // オービットカメラが注視するターゲット座標
  Vector3 target_;
  // オービットカメラのターゲットからの距離
  float orbitDistance_;

public:
  /// <summary>
  /// コンストラクタ
  /// </summary>
  DebugCamera();
  ~DebugCamera() override = default;

  /// <summary>
  /// 初期化
  /// </summary>
  void Initialize() override;

  /// <summary>
  /// 更新
  /// </summary>
  void Update() override;

  // 回転の取得（既存呼び出し元との互換用エイリアス）
  const Vector3 &GetRotation() const { return GetRotate(); }
};
