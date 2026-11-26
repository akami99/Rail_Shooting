#include "Sprite.h"
#include "Base/DX12Context.h"
#include "Math/Functions/MathUtils.h"
#include "Math/Matrix/MatrixGenerators.h"
#include "SpriteCommon.h"
#include "Texture/TextureManager.h"
#include "Base/SrvManager.h"

#include "Base/Win32Window.h"
#include <cassert>

using namespace Microsoft::WRL;
using namespace MathUtils;
using namespace MathGenerators;

// 初期化
void Sprite::Initialize(const std::string &filePath) {
  // メンバ変数にセット

  // 頂点バッファとインデックスバッファの作成
  CreateBufferResource();

  // マテリアルバッファの作成
  CreateMaterialResource();

  // 変換行列バッファの作成
  CreateTransformationMatrixResource();

  // インデックスを代入
  textureFilePath_ = filePath;

  // テクスチャサイズをイメージに合わせる
  AdjustTextureSize();
}

// 更新処理
void Sprite::Update() {
  if (!isDirty_) {
    return;
  }

  // アンカーポイント
  float left = 0.0f - anchorPoint_.x;
  float right = 1.0f - anchorPoint_.x;
  float top = 0.0f - anchorPoint_.y;
  float bottom = 1.0f - anchorPoint_.y;

  // 左右反転
  if (isFlipX_) {
    left = -left;
    right = -right;
  }
  // 上下反転
  if (isFlipY_) {
    top = -top;
    bottom = -bottom;
  }

  // テクスチャ範囲指定
  float texLeft = 0.0f;
  float texRight = 1.0f;
  float texTop = 0.0f;
  float texBottom = 1.0f;

  if (!useCustomSrv_) {
      const DirectX::TexMetadata& metadata =
          TextureManager::GetInstance()->GetMetaData(textureFilePath_);
      texLeft = textureLeftTop_.x / metadata.width;
      texRight = (textureLeftTop_.x + textureSize_.x) / metadata.width;
      texTop = textureLeftTop_.y / metadata.height;
      texBottom = (textureLeftTop_.y + textureSize_.y) / metadata.height;
  }
  else {
      // カスタムSRVの場合はとりあえず0-1で
      texLeft = 0.0f;
      texRight = 1.0f;
      texTop = 0.0f;
      texBottom = 1.0f;
  }

#pragma region 頂点データとインデックスデータの設定
  // 頂点データとインデックスデータの設定

  // 頂点リソースにデータを書き込む

  // 頂点0: 左下 (0,0)
  vertexData_[0].position = {left, bottom, 0.0f, 1.0f};
  vertexData_[0].texcoord = {texLeft, texBottom};
  vertexData_[0].normal = {0.0f, 0.0f, -1.0f};

  // 頂点1: 左上 (640,0)
  vertexData_[1].position = {left, top, 0.0f, 1.0f};
  vertexData_[1].texcoord = {texLeft, texTop};
  vertexData_[1].normal = {0.0f, 0.0f, -1.0f};

  // 頂点2: 右下 (0,360)
  vertexData_[2].position = {right, bottom, 0.0f, 1.0f};
  vertexData_[2].texcoord = {texRight, texBottom};
  vertexData_[2].normal = {0.0f, 0.0f, -1.0f};

  // 頂点3: 右上 (640,360)
  vertexData_[3].position = {right, top, 0.0f, 1.0f};
  vertexData_[3].texcoord = {texRight, texTop};
  vertexData_[3].normal = {0.0f, 0.0f, -1.0f};

  // インデックスリソースにデータを書き込む
  // 三角形1
  indexData_[0] = 0; // 頂点0
  indexData_[1] = 1; // 頂点1
  indexData_[2] = 2; // 頂点2
  // 三角形2
  indexData_[3] = 1; // 頂点1
  indexData_[4] = 3; // 頂点3
  indexData_[5] = 2; // 頂点2

#pragma endregion ここまで

#pragma region 変換行列データの設定

  // Transform情報を作る
  Transform transform{
      {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  transform.scale = {scale_.x, scale_.y, 1.0f};
  transform.rotate = {0.0f, 0.0f, rotation_}; // 回転はZ軸回転のみ対応
  transform.translate = {translate_.x, translate_.y, 0.0f};

  Matrix4x4 uvTransformMatrix = MakeAffineMatrix(
      uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);
  materialData_->uvTransform = uvTransformMatrix;

  // Transform情報から変換行列を作る
  Matrix4x4 worldMatrix =
      MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
  // View行列は単位行列
  Matrix4x4 viewMatrix = MakeIdentity4x4();
  // ProjectionMatrixを作って平行投影行列を書き込む
  Matrix4x4 projectionMatrix =
      MakeOrthographicMatrix(0.0f, 0.0f, float(Win32Window::kClientWidth),
                             float(Win32Window::kClientHeight), 0.0f, 100.0f);
  Matrix4x4 wvpMatrix = worldMatrix * viewMatrix * projectionMatrix;
  transformationMatrixData_->WVP =
      wvpMatrix; // World-View-Projection行列をWVPメンバーに入れる
  transformationMatrixData_->World =
      worldMatrix; // 純粋なワールド行列をWorldメンバーに入れる

  isDirty_ = false;

#pragma endregion ここまで
}

// 描画処理
void Sprite::Draw() {
  // VertexBufferとIndexBufferの設定
    DX12Context::GetInstance()->GetCommandList()->IASetVertexBuffers(
      0, 1, &vertexBufferView_);
    DX12Context::GetInstance()->GetCommandList()->IASetIndexBuffer(
      &indexBufferView_);

  // マテリアルCBVの設定
    DX12Context::GetInstance()
      ->GetCommandList()
      ->SetGraphicsRootConstantBufferView(
          0, materialResource_->GetGPUVirtualAddress());
  // 変換行列CBVの設定
    DX12Context::GetInstance()
      ->GetCommandList()
      ->SetGraphicsRootConstantBufferView(
          1, transformationMatrixResource_->GetGPUVirtualAddress());

  // SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
  D3D12_GPU_DESCRIPTOR_HANDLE srvHandle;
  if (useCustomSrv_) {
      srvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
  }
  else {
      srvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_);
  }

    DX12Context::GetInstance()
      ->GetCommandList()
      ->SetGraphicsRootDescriptorTable(
          2, srvHandle);

  // Dissolveマスク用のSRVをバインド。3はrootParameter[3]である。
  DX12Context::GetInstance()
      ->GetCommandList()
      ->SetGraphicsRootDescriptorTable(
          3, TextureManager::GetInstance()->GetSrvHandleGPU(dissolveMaskFilePath_));

  // 描画コマンド
    DX12Context::GetInstance()->GetCommandList()->DrawIndexedInstanced(
      6, 1, 0, 0, 0);
}

void Sprite::SetTexture(const std::string &filePath) {
  useCustomSrv_ = false;
  textureFilePath_ = filePath;

  // テクスチャが変わったので、切り出しサイズを新しいテクスチャに合わせる
  AdjustTextureSize();
  isDirty_ = true;
}

void Sprite::SetTexture(uint32_t srvIndex) {
    useCustomSrv_ = true;
    srvIndex_ = srvIndex;
    isDirty_ = true;
}

// 頂点バッファとインデックスバッファの作成
void Sprite::CreateBufferResource() {

#pragma region リソースとバッファビューの作成
  // リソースとバッファビューの作成

  // VertexResourceを作る
  vertexResource_ = DX12Context::GetInstance()->CreateBufferResource(
      sizeof(VertexData) * 6);
  // IndexResourceを作る
  indexResource_ = DX12Context::GetInstance()->CreateBufferResource(
      sizeof(uint32_t) * 6);

  // VertexBufferViewを作成する
  // リソースの先頭アドレスから使う
  vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
  // 使用するリソースのサイズは頂点6つ分のサイズ
  vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
  // １頂点当たりのサイズ
  vertexBufferView_.StrideInBytes = sizeof(VertexData);

  // IndexBufferViewを作成する
  // リソースの先頭アドレスから使う
  indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
  // 使用するリソースのサイズはインデックス6つ分のサイズ
  indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
  // インデックスはuint32_tとする
  indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

#pragma endregion ここまで

#pragma region VertexDataの設定
  // VertexDataの設定

  // VertexResourceにデータを書き込むためのアドレスを取得してVertexDataに割り当てる
  vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData_));

#pragma endregion ここまで

#pragma region IndexDataの設定
  // IndexDataの設定

  // IndexResourceにデータを書き込むためのアドレスを取得してIndexDataに割り当てる
  indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&indexData_));

#pragma endregion ここまで
}

// マテリアルバッファの作成
void Sprite::CreateMaterialResource() {

#pragma region マテリアルリソースの作成
  // マテリアルリソースの作成

  // マテリアルリソースを作る
  materialResource_ =
      DX12Context::GetInstance()->CreateBufferResource(sizeof(Material));

#pragma endregion ここまで

#pragma region MaterialDataの設定
  // MaterialDataの設定

  // MaterialResourceにデータを書き込むためのアドレスを取得してMaterialDataに割り当てる
  materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));
  // マテリアルデータの初期値を書き込む
  materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
  // ライティングを無効に設定
  materialData_->enableLighting = false;
  // UV変換行列を単位行列に設定
  materialData_->uvTransform = MakeIdentity4x4();

  // Dissolveパラメータの初期化
  materialData_->enableDissolve = 0;
  materialData_->dissolveThreshold = 0.0f;
  materialData_->dissolveEdgeRange = 0.02f;
  materialData_->dissolveEdgeColor = Vector3(1.0f, 0.4f, 0.3f);

#pragma endregion ここまで
}

// 変換行列バッファの作成
void Sprite::CreateTransformationMatrixResource() {
#pragma region 変換行列リソースの作成
  // 変換行列リソースの作成

  // 変換行列リソースを作る
  transformationMatrixResource_ =
      DX12Context::GetInstance()->CreateBufferResource(
          sizeof(TransformationMatrix));

#pragma endregion ここまで

#pragma region TransformationMatrixDataの設定
  // TransformationMatrixDataの設定

  // TransformationMatrixResourceにデータを書き込むためのアドレスを取得してTransformationMatrixDataに割り当てる
  // 書き込むためのアドレスを取得
  transformationMatrixResource_->Map(
      0, nullptr, reinterpret_cast<void **>(&transformationMatrixData_));
  // 単位行列を書き込んでおく
  transformationMatrixData_->WVP = MakeIdentity4x4();
  transformationMatrixData_->World = MakeIdentity4x4();

#pragma endregion ここまで
}

void Sprite::AdjustTextureSize() {
  // テクスチャメタデータを取得
  const DirectX::TexMetadata &metadata =
      TextureManager::GetInstance()->GetMetaData(textureFilePath_);

  textureSize_.x = static_cast<float>(metadata.width);
  textureSize_.y = static_cast<float>(metadata.height);
  // 画像をテクスチャサイズに合わせる
  scale_ = textureSize_;
}

void Sprite::SetDissolveMaskTexture(const std::string &filePath) {
  dissolveMaskFilePath_ = filePath;
  TextureManager::GetInstance()->LoadTexture(dissolveMaskFilePath_);
}

void Sprite::SetDissolveParams(int32_t enable, float threshold, float edgeRange, const Vector3 &edgeColor) {
  if (materialData_) {
    materialData_->enableDissolve = enable;
    materialData_->dissolveThreshold = threshold;
    materialData_->dissolveEdgeRange = edgeRange;
    materialData_->dissolveEdgeColor = edgeColor;
  }
}
