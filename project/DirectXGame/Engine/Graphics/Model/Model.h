#pragma once

#include "Types/ModelTypes.h"
#include "Types/ParticleTypes.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

// 3Dモデル
class Model {
private: // namespace省略のためのusing宣言
#pragma region using宣言

  // Microsoft::WRL::ComPtrをComPtrで省略
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

#pragma endregion

private: // メンバ変数
  // Objのファイルデータ
  ModelData modelData_{};

  // インデックスデータ用リソース
  ComPtr<ID3D12Resource> indexResource_ = nullptr;
  D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

  // バッファリソース
  ComPtr<ID3D12Resource> vertexResource_ = nullptr;
  // バッファリソース内の使い道を捕捉するバッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
  // バッファリソース内のデータを表すポインタ
  VertexData *vertexData_ = nullptr;

  // マテリアルデータ

  // バッファリソース
  ComPtr<ID3D12Resource> materialResource_ = nullptr;
  // バッファリソース内のデータを指すポインタ
  Material *materialData_ = nullptr;

  // Dissolveマスク用テクスチャパス
  std::string dissolveMaskFilePath_ = "masks/noise0.png";

public: // メンバ関数
  // 初期化(テクスチャロードでコマンドリスト積んでいるので注意)
  void Initialize(const std::string &directoryPath,
                  const std::string &filename);

  // リングプリミティブの生成
  void CreateRing(const std::string &textureFilePath, float innerRadius, float outerRadius, uint32_t division);
  void CreateRing(const std::string &textureFilePath, const RingSettings& settings);

  // シリンダープリミティブの生成
  void CreateCylinder(const std::string &textureFilePath, const CylinderSettings& settings);

  // 平面プリミティブの生成
  void CreatePlane(const std::string &textureFilePath, const PlaneSettings& settings);

  // 描画処理
  void Draw();

  // getter

  // RootNodeを取得するGetter
  const Node& GetRootNode() const { return modelData_.rootNode; }

  // 色の取得
  const Vector4 &GetColor() const { return materialData_->color; }

  // ライティングが有効かどうか
  const int32_t &IsEnableLighting() const {
    return materialData_->enableLighting;
  }

  // 環境マップ係数の取得
  const float &GetEnvironmentCoefficient() const {
    return materialData_->environmentCoefficient;
  }

  // Dissolveのゲッター
  int32_t GetDissolveEnable() const { return materialData_ ? materialData_->enableDissolve : 0; }
  float GetDissolveThreshold() const { return materialData_ ? materialData_->dissolveThreshold : 0.0f; }
  float GetDissolveEdgeRange() const { return materialData_ ? materialData_->dissolveEdgeRange : 0.0f; }
  Vector3 GetDissolveEdgeColor() const { return materialData_ ? materialData_->dissolveEdgeColor : Vector3{0.0f,0.0f,0.0f}; }
  const std::string& GetDissolveMaskTexturePath() const { return dissolveMaskFilePath_; }

  // バッファビューの取得
  const D3D12_VERTEX_BUFFER_VIEW &GetVertexBufferView() const { return vertexBufferView_; }
  const D3D12_INDEX_BUFFER_VIEW &GetIndexBufferView() const { return indexBufferView_; }
  uint32_t GetIndexCount() const { return static_cast<uint32_t>(modelData_.indices.size()); }
  const std::string& GetTextureFilePath() const { return modelData_.material.textureFilePath; }

#ifdef USE_IMGUI
  // 色の参照取得(ImGui用)
  Vector4 &GetColorDebug() { return materialData_->color; }
#endif // USE_IMGUI


  // setter

  // 色の設定
  void SetColor(const Vector4 &color) { materialData_->color = color; }

  // ライティングが有効かを設定
  void SetEnableLighting(const int32_t enableLighting) {
    materialData_->enableLighting = enableLighting;
  }

  // 環境マップ係数の設定
  void SetEnvironmentCoefficient(const float coefficient) {
    materialData_->environmentCoefficient = coefficient;
  }

  // Dissolveの設定
  void SetDissolveMaskTexture(const std::string &filePath);
  void SetDissolveParams(int32_t enable, float threshold, float edgeRange, const Vector3 &edgeColor);

  // テクスチャパスの設定
  void SetTextureFilePath(const std::string &filePath);

  // UVスケールの設定
  void SetUvScale(const Vector2 &uvScale);

private: // メンバ関数
  // .objファイルの読み取り
  void LoadModelFile(const std::string &directoryPath,
                   const std::string &fileName);

  // assimpのノードを変換
  Node ReadNode(aiNode* node);

  // インデックスバッファ作成用関数
  void CreateIndexResource();

  // 頂点データの作成
  void CreateVertexResource();

  // マテリアルバッファの作成
  void CreateMaterialResource();
};
