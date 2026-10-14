#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <array> // std::array用に必要
#include <memory> // std::unique_ptr用に必要

#include "BlendMode/BlendMode.h"

// 前方宣言
class Camera;

// 3Dオブジェクト共通部
class Object3dCommon {
public:
  // 【Passkey Idiom】
  struct Token {
  private:
    friend class Object3dCommon;
    Token() {}
  };

private: // namespace省略のためのusing宣言
#pragma region using宣言

  // Microsoft::WRL::ComPtrをComPtrで省略
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma endregion

private: // メンバ変数
  static std::unique_ptr<Object3dCommon> instance_;

  // デフォルトカメラ
  Camera* defaultCamera_ = nullptr;

  // グラフィックスパイプラインステート (ブレンドモードごとに配列で保持)
  ComPtr<ID3D12PipelineState>
      psoArray_[BlendMode::BlendState::kCountOfBlendMode];

  // ワイヤーフレーム用のグラフィックスパイプラインステート
  ComPtr<ID3D12PipelineState>
      psoWireframeArray_[BlendMode::BlendState::kCountOfBlendMode];

  // 環境マップ
  uint32_t environmentMapSrvIndex_ = 0;

public: // シングルトンインスタンス取得
    static Object3dCommon* GetInstance();
    static void Destroy();

public: // メンバ関数
  // コンストラクタ(隠蔽)
  explicit Object3dCommon(Token);
  // 初期化
  void Initialize();

  // 終了
  void Finalize();

  // 共通描画設定(ブレンドモード, ワイヤーフレーム)
  void SetCommonDrawSettings(BlendMode::BlendState currentBlendMode, bool isWireframe = false);
  // getter

  // デフォルトカメラの取得
  Camera* GetDefaultCamera() const { return defaultCamera_; };

  // setter

  // デフォルトカメラの設定(最初に設定しておく用)
  void SetDefaultCamera(Camera *camera) { defaultCamera_ = camera; };

  // 環境マップの設定
  void SetEnvironmentMap(uint32_t srvIndex) { environmentMapSrvIndex_ = srvIndex; };

private: // コンストラクタ周り
    ~Object3dCommon() = default;
    Object3dCommon(const Object3dCommon&) = delete;
    const Object3dCommon& operator=(const Object3dCommon&) = delete;

    friend std::default_delete<Object3dCommon>;
};
