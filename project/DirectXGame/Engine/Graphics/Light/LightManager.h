#pragma once
#include "LightTypes.h" // DirectionalLight構造体があるヘッダ
#include <d3d12.h>
#include <wrl.h>
#include <memory> // std::unique_ptr用に必要

class LightManager {
public:
    // 【Passkey Idiom】
    struct Token {
    private:
        friend class LightManager;
        Token() {}
    };

    static LightManager* GetInstance();
    static void Destroy();
public:
  // コンストラクタ(隠蔽)
  explicit LightManager(Token);
  void Initialize();
  void Finalize();
  // 更新の最後に呼ぶ
  void Update();
  void Draw();

  // それぞれのライトのGPUアドレスを取得
  D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightAddress() const {
    return directionalResource_->GetGPUVirtualAddress();
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetPointLightAddress() const {
    return pointResource_->GetGPUVirtualAddress();
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightAddress() const {
    return spotResource_->GetGPUVirtualAddress();
  }

  // データ設定用セッター
  void SetDirectionalLightColor(const Vector4 &color) {
    directionalData_->color = color;
  }
  void SetDirectionalLightDirection(const Vector3 &direction) {
    directionalData_->direction = direction;
  }
  void SetDirectionalLightIntensity(float intensity) {
    directionalData_->intensity = intensity;
  }

  void SetPointLightColor(const Vector4 &color) { pointData_->color = color; }
  void SetPointLightPosition(const Vector3 &position) {
    pointData_->position = position;
  }
  void SetPointLightIntensity(float intensity) {
    pointData_->intensity = intensity;
  }

  void SetSpotLightColor(const Vector4 &color) { spotData_->color = color; }
  void SetSpotLightPosition(const Vector3 &position) {
      spotData_->position = position;
  }
  void SetSpotLightIntensity(float intensity) {
      spotData_->intensity = intensity;
  }
  void SetSpotLightDirection(const Vector3 &direction) {
      spotData_->direction = direction;
  }

  // 構造体一括設定用セッター
  void SetDirectionalLightData(const DirectionalLight &data) {
    *directionalData_ = data;
  }
  void SetPointLightData(const PointLight &data) {
    *pointData_ = data;
  }
  void SetSpotLightData(const SpotLight &data) {
    *spotData_ = data;
  }

  // 読み取り専用ゲッター
  const DirectionalLight &GetDirectionalLightData() const { return *directionalData_; }
  const PointLight &GetPointLightData() const { return *pointData_; }
  const SpotLight &GetSpotLightData() const { return *spotData_; }

private:
    ~LightManager() = default;
    LightManager(const LightManager&) = delete;
    const LightManager& operator=(const LightManager&) = delete;

    friend std::default_delete<LightManager>;

private:
  // 平行光源バッファの作成
  void CreateDirectionalResource();

  // 点光源バッファの作成
  void CreatePointResource();

  // スポットライトバッファの作成
  void CreateSpotResource();

private:
  // シングルトンインスタンス
  static std::unique_ptr<LightManager> instance_;

  // 平行光源用
  Microsoft::WRL::ComPtr<ID3D12Resource> directionalResource_;
  DirectionalLight *directionalData_ = nullptr;

  // 点光源用
  Microsoft::WRL::ComPtr<ID3D12Resource> pointResource_;
  PointLight *pointData_ = nullptr;

  // スポットライト用
  Microsoft::WRL::ComPtr<ID3D12Resource> spotResource_;
  SpotLight *spotData_ = nullptr;
};