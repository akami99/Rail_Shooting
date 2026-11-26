#define USE_MATH_DEFINES // 数学定義を使用する
#include <cmath>
#include <numbers>

#include "../Camera/DebugCamera.h"
#include "../../Core/Utility/Math/Functions/MathUtils.h"
#include "ImGuiManager.h"
#include "../Input/Input.h"

using namespace MathUtils;
using namespace MathGenerators;

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

  isOrbitMode_ = false;                      // オービットモードは無効
  cameraEulerRotation_ = {0.0f, 0.0f, 0.0f}; // カメラのオイラー角初期化
  target_ = {0.0f, 0.0f, 0.0f};              // 初期ターゲットは原点
  orbitDistance_ = 50.0f;                    // オービット距離の初期値
}

void DebugCamera::Update() {
#ifdef USE_IMGUI
  // デバッグ用ImGuiウィンドウ

  // 初期表示位置を設定(画面左上)
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);

  ImGui::Begin("Debug Camera Control");
  ImGui::Text("R: Reset Camera");
  ImGui::Text("Mode Change: TAB");
  // TABキー入力でモードを切り替える
  if (Input::GetInstance()->IsKeyTriggered(DIK_TAB)) {
    isOrbitMode_ = !isOrbitMode_;
    // モード切り替え時にカメラの状態をリセットまたは調整することもできるが、
    // まずはシンプルな切り替えから
    if (isOrbitMode_) {
      // 一人称視点 -> ピボット回転 に切り替える際
      // 現在のカメラのワールド行列から逆算して、target_とorbitDistance_を設定する
      // matRot_ は現在のカメラの回転を表している
      // translation_ は現在のカメラの絶対位置

      // カメラのZ軸方向（前方）ベクトルを取得（matRot_のZ軸成分）
      // HLSLのmul(vector,
      // matrix)は行ベクトルなので、行列の行が軸ベクトルに相当する
      // matRot_の3行目（インデックス2）がカメラのZ軸（前方）を向くベクトルとなる
      Vector3 cameraForward = {matRot_.m[2][0], matRot_.m[2][1],
                               matRot_.m[2][2]};
      cameraForward = Normalize(cameraForward); // 正規化

      // 現在のtranslation_（カメラ位置）からorbitDistance_分だけカメラ前方と逆方向に進んだ点がターゲット
      // もしlocalOffsetが{0,0,-orbitDistance_}なら、target_はtranslation_から前方へorbitDistance_離れた点
      target_ = translation_ + cameraForward * orbitDistance_;

      // cameraEulerRotation_ は現在のrotation_から初期化
      cameraEulerRotation_ =
          rotation_; // 現在の視点回転をオービットの初期回転に
    } else {
      // ピボット回転 -> 一人称視点 に切り替える際
      // rotation_（一人称視点のオイラー角）を、現在のオービットカメラのオイラー角に合わせる
      rotation_ = cameraEulerRotation_;
    }
  }

  // キーボード入力によるカメラの回転
  // 各モードで共通の回転差分計算
  const float rotateSpeed = 0.02f; // 回転速度 (ラジアン)

  // --- 各モードのロジック ---
  if (isOrbitMode_) {
    // --- ピボット回転 (オービットカメラ) ロジック ---

    // ピボット回転モード用の回転差分計算
    Vector3 deltaRotationEuler = {0.0f, 0.0f, 0.0f};
    if (Input::GetInstance()->IsKeyDown(DIK_W) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.x +=
          rotateSpeed; // ピボット回転：Wで上方向に移動 (ピッチ角度増加)
    } else if (Input::GetInstance()->IsKeyDown(DIK_S) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.x -=
          rotateSpeed; // ピボット回転：Sで下方向に移動 (ピッチ角度減少)
    }
    if (Input::GetInstance()->IsKeyDown(DIK_A) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.y +=
          rotateSpeed; // ピボット回転：Aで左方向に移動 (ヨー角度増加)
    } else if (Input::GetInstance()->IsKeyDown(DIK_D) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.y -=
          rotateSpeed; // ピボット回転：Dで右方向に移動 (ヨー角度減少)
    }

    // ここにピボット回転の計算を記述します。
    // まずは回転角度を累積
    cameraEulerRotation_ += deltaRotationEuler;

    // ピッチの制限
    float PI = std::numbers::pi_v<float>;
    cameraEulerRotation_.x = std::fmax(
        -PI / 2.0f + 0.01f,
        std::fmin(PI / 2.0f - 0.01f,
                  cameraEulerRotation_
                      .x)); // 微小なオフセットで完全に垂直になるのを防ぐ

    // オービット位置を決定するための回転行列を構築
    // この行列は、カメラがターゲットの周りのどの位置にいるかを定義します。
    Matrix4x4 orbitRotationMatrix = MakeRotateYMatrix(cameraEulerRotation_.y) *
                                    MakeRotateXMatrix(cameraEulerRotation_.x);

    // カメラのワールド座標を計算
    // カメラが通常Z軸負の方向を見ていると仮定し、その方向にorbitDistance_だけ離れた位置をオフセットとする
    // （ターゲットから見てカメラが「背後」に位置する）
    Vector3 localOffset = {0.0f, 0.0f, -orbitDistance_};

    // オフセットをオービット回転行列で変換し、ターゲット位置に加算してカメラのワールド位置を決定
    translation_ = TransformVector(localOffset, orbitRotationMatrix);
    translation_ += target_; // target_ + translation_

    // カメラの「視点」をターゲットに向けるための回転行列 (matRot_) を構築する
    // これにより、カメラは常にターゲットを注視しする
    Vector3 cameraZAxis = Normalize(
        target_ - translation_); // カメラのZ軸（前方）はターゲットの方向を向く

    // ワールドのY軸を仮のUpベクトルとする（クロス積の計算のため）
    Vector3 worldYAxis = {0.0f, 1.0f, 0.0f};

    // cameraZAxis (カメラ前方) が tempUp (ワールドY軸) とほぼ平行になる場合、
    // クロス積がゼロベクトルに近づき、カメラのX軸(右)が不安定になるため、
    // tempUp をワールドのZ軸に一時的に切り替える
    // ドット積の絶対値が0.999fより大きい場合、ほぼ平行とみなす
    if (std::abs(Dot(cameraZAxis, worldYAxis)) > 0.999f) {
      worldYAxis = {0.0f, 0.0f, 1.0f}; // ワールドのZ軸を代わりに使用
    } // 本格的に安定させるにはクォータニオンを使用する

    // カメラのX軸（右）を計算
    // （worldYAxis と cameraZAxis
    // が平行に近い場合はゼロベクトルになる可能性があるので注意が必要だが、
    //   オービットカメラのピッチ制限があれば通常は問題ないことが多い）
    Vector3 cameraXAxis = Normalize(Cross(worldYAxis, cameraZAxis));

    // カメラのY軸（上）を計算 (正確なUpベクトルはX軸とZ軸の外積で計算する)
    Vector3 cameraYAxis = Cross(cameraZAxis, cameraXAxis);

    // matRot_ を新しい視点方向の行列として直接構築する
    // HLSLのmul(vector, matrix)は行ベクトルを扱い、C++の行列が row-major
    // であれば、各行が基底ベクトルになる
    matRot_.m[0][0] = cameraXAxis.x;
    matRot_.m[0][1] = cameraXAxis.y;
    matRot_.m[0][2] = cameraXAxis.z;
    matRot_.m[0][3] = 0.0f;
    matRot_.m[1][0] = cameraYAxis.x;
    matRot_.m[1][1] = cameraYAxis.y;
    matRot_.m[1][2] = cameraYAxis.z;
    matRot_.m[1][3] = 0.0f;
    matRot_.m[2][0] = cameraZAxis.x;
    matRot_.m[2][1] = cameraZAxis.y;
    matRot_.m[2][2] = cameraZAxis.z;
    matRot_.m[2][3] = 0.0f;
    matRot_.m[3][0] = 0.0f;
    matRot_.m[3][1] = 0.0f;
    matRot_.m[3][2] = 0.0f;
    matRot_.m[3][3] = 1.0f;

    // rotation_ は表示用なので、cameraEulerRotation_ をそのまま使う
    rotation_ = cameraEulerRotation_;

    // オービットカメラの移動量（ターゲット位置の移動）
    const float moveSpeed = 0.5f; // オービットモードでのターゲット移動速度
    Vector3 localMove = {0.0f, 0.0f, 0.0f}; // ローカル移動ベクトルを初期化

    // キーボード入力に応じてローカル移動ベクトルを設定
    if (Input::GetInstance()->IsKeyDown(DIK_W) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.z += moveSpeed; // 前方へ移動 (カメラのローカルZ軸方向)
    } else if (Input::GetInstance()->IsKeyDown(DIK_S) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.z -= moveSpeed; // 後方へ移動 (カメラのローカルZ軸方向)
    }
    if (Input::GetInstance()->IsKeyDown(DIK_A) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.x -= moveSpeed; // 左へ移動 (カメラのローカルX軸方向)
    } else if (Input::GetInstance()->IsKeyDown(DIK_D) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.x += moveSpeed; // 右へ移動 (カメラのローカルX軸方向)
    }
    if (Input::GetInstance()->IsKeyDown(DIK_Q)) {
      localMove.y += moveSpeed; // 上へ移動 (カメラのローカルY軸方向)
    } else if (Input::GetInstance()->IsKeyDown(DIK_E)) {
      localMove.y -= moveSpeed; // 下へ移動 (カメラのローカルY軸方向)
    }

    // ローカル移動ベクトルをカメラの向きに合わせてワールド座標系に変換
    // matRot_
    // は既にカメラの「視点」の向き（ターゲットを注視するための行列）を表している
    Vector3 worldMove = TransformVector(localMove, matRot_);

    // ワールド座標系での移動量をターゲット位置に加算
    target_ += worldMove; // ターゲット位置を更新(Add(target_, worldMove))

    Vector3 rotatedOffset = TransformVector(localOffset, matRot_);
    translation_ = target_ + rotatedOffset;

    ImGui::Text("Mode: Orbit Camera");
    ImGui::Text("Target: (%.2f, %.2f, %.2f)", target_.x, target_.y, target_.z);
    ImGui::Text("Orbit Distance: %.2f", orbitDistance_);

    // 操作説明
    ImGui::Text("W/S + LSHIFT: Pivot Up/Down");
    ImGui::Text("A/D + LSHIFT: Pivot Left/Right");
    ImGui::Text("W/S: Move Target Forward/Backward");
    ImGui::Text("A/D: Move Target Left/Right");
    ImGui::Text("Q/E: Move Target Up/Down");

  } else {
    // --- 一人称視点 (First-Person Camera) ロジック ---

    // 一人称視点モード用の回転差分計算
    Vector3 deltaRotationEuler = {0.0f, 0.0f, 0.0f};
    if (Input::GetInstance()->IsKeyDown(DIK_W) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.x -=
          rotateSpeed; // 一人称視点：Wで下を向く (ピッチ角度減少)
    } else if (Input::GetInstance()->IsKeyDown(DIK_S) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.x +=
          rotateSpeed; // 一人称視点：Sで上を向く (ピッチ角度増加)
    }
    if (Input::GetInstance()->IsKeyDown(DIK_A) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.y -=
          rotateSpeed; // 一人称視点：Aで右を向く (ヨー角度減少)
    } else if (Input::GetInstance()->IsKeyDown(DIK_D) && Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      deltaRotationEuler.y +=
          rotateSpeed; // 一人称視点：Dで左を向く (ヨー角度増加)
    }

    // rotation_（表示用）を更新 (一人称視点モードのオイラー角を更新)
    rotation_ += deltaRotationEuler;

    // 回転角度の制限(一人称視点モードのピッチ制限)
    float PI = std::numbers::pi_v<float>;
    rotation_.x = std::fmax(-PI / 2.0f, std::fmin(PI / 2.0f, rotation_.x));

    // 累積の回転行列を合成(一人称視点モードの回転)
    matRot_ = MakeRotateYMatrix(deltaRotationEuler.y) * matRot_;
    matRot_ = matRot_ * MakeRotateXMatrix(deltaRotationEuler.x);

    // キーボード入力によるカメラの移動
    const float moveSpeed = 0.2f; // 移動速度
    Vector3 localMove = {0.0f, 0.0f, 0.0f};

    // 前後移動
    if (Input::GetInstance()->IsKeyDown(DIK_W) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.z += moveSpeed; // 前方へ移動
    } else if (Input::GetInstance()->IsKeyDown(DIK_S) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.z -= moveSpeed; // 後方へ移動
    }

    // 左右移動
    if (Input::GetInstance()->IsKeyDown(DIK_A) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.x -= moveSpeed; // 左へ移動
    } else if (Input::GetInstance()->IsKeyDown(DIK_D) && !Input::GetInstance()->IsKeyDown(DIK_LSHIFT)) {
      localMove.x += moveSpeed; // 右へ移動
    }

    // 上下移動
    if (Input::GetInstance()->IsKeyDown(DIK_Q)) {
      localMove.y += moveSpeed; // 上へ移動
    } else if (Input::GetInstance()->IsKeyDown(DIK_E)) {
      localMove.y -= moveSpeed; // 下へ移動
    }

    // ローカル移動ベクトルをワールド座標系に変換
    Vector3 worldMove = TransformVector(localMove, matRot_);

    // ワールド座標系での移動量を現在位置に加算
    translation_ += worldMove;

    ImGui::Text("Mode: First-Person Camera");
    // 操作説明
    ImGui::Text("W/S + LSHIFT: Pitch Up/Down");
    ImGui::Text("A/D + LSHIFT: Yaw Left/Right");
    ImGui::Text("W/S: Move Forward/Backward");
    ImGui::Text("A/D: Move Left/Right");
    ImGui::Text("Q/E: Move Up/Down\n\n\n\n");
  }

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