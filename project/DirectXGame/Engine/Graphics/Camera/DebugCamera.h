#pragma once
#include "Camera.h"
#include "Math/MathTypes.h"
#include "Math/Matrix/MatrixGenerators.h"

/// <summary>
/// デバッグカメラ
/// </summary>
class DebugCamera : public Camera {
private:
  // X,Y,Z周りのローカル回転角
  Vector3 rotation_ = {0, 0, 0};
  // ローカル座標
  Vector3 translation_ = {0, 0, -50};
  // 累積回転行列
  Matrix4x4 matRot_;
  // カメラがオービットモードかどうか
  bool isOrbitMode_ = false;
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
