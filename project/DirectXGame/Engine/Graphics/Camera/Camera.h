#pragma once

#include "Types/GraphicsTypes.h"

#include <d3d12.h>       // ID3D12Resource のため
#include <wrl/client.h>   // Microsoft::WRL::ComPtr のため

// カメラ
class Camera {
private: // メンバ変数
  Transform transform_{};
  // ワールド行列
  Matrix4x4 worldMatrix_{};
  // ビュー行列
  Matrix4x4 viewMatrix_{};
  // 透視投影行列
  Matrix4x4 projectionMatrix_{};
  // 垂直方向視野角
  float fovY_{};
  // アスペクト比
  float aspectRatio_{};
  // ニアクリップ距離
  float nearClip_{};
  // ファークリップ距離
  float farClip_{};
  // ビュープロジェクション行列
  Matrix4x4 viewProjectionMatrix_{};
  // 定数バッファ
  Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
  // 定数バッファのデータポインタ
  CameraForGPU *constData_ = nullptr;

  bool isMapped_ = false;

public: // メンバ関数
  // コンストラクタ
  Camera();
  ~Camera();
  // コピー禁止にする（これを追加）
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  // 初期化関数
  void Initialize();

  // 更新
  void Update();

  // getter

  // GPU上のアドレスを取得するゲッター
  D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferGPUVirtualAddress() const {
      return constBuffer_->GetGPUVirtualAddress();
  }
  // ワールド行列の取得
  const Matrix4x4 &GetWorldMatrix() const { return worldMatrix_; }
  // ビュー行列の取得
  const Matrix4x4 &GetViewMatrix() const { return viewMatrix_; }
  // 透視投影行列の取得
  const Matrix4x4 &GetProjectionMatrix() const { return projectionMatrix_; }
  // ビュープロジェクション行列の取得
  const Matrix4x4 &GetViewProjectionMatrix() const {
    return viewProjectionMatrix_;
  }
  // 回転の取得
  const Vector3 &GetRotate() const { return transform_.rotate; }
  // 座標の取得
  const Vector3 &GetTranslate() const { return transform_.translate; }

  // setter

  // 回転の設定
  void SetRotate(const Vector3 &rotate) { transform_.rotate = rotate; }
  // 座標の設定
  void SetTranslate(const Vector3 &translate) {
    transform_.translate = translate;
  }
  // 垂直方向視野角の設定
  void SetFovY(const float &fovY) { fovY_ = fovY; }
  // アスペクト比の設定
  void SetAspectRatio(const float &aspectRatio) { aspectRatio_ = aspectRatio; }
  // ニアクリップ距離の設定
  void SetNearClip_(const float &nearClip) { nearClip_ = nearClip; }
  // ファークリップ距離の設定
  void SetFarClip(const float &farClip) { farClip_ = farClip; }
};
