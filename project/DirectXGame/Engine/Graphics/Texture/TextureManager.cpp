#include "TextureManager.h"
#include "../../Core/Utility/Logger/Logger.h"
#include "../../Core/Utility/String/StringUtility.h"
#include "Base/DX12Context.h"
#include "Base/SrvManager.h"

using namespace StringUtility;
using namespace Microsoft::WRL;

std::unique_ptr<TextureManager> TextureManager::instance_ = nullptr;

// 初期化
void TextureManager::Initialize() {
  // SRVの数と同数
  textureDatas_.reserve(SrvManager::kMaxSRVCount);
}

// シングルトンインスタンスの取得
TextureManager *TextureManager::GetInstance() {
  if (instance_ == nullptr) {
    instance_ = std::make_unique<TextureManager>(Token{});
  }
  return instance_.get();
}

void TextureManager::Destroy() {
    instance_.reset();
}

TextureManager::TextureManager(Token) {
    // コンストラクタ
}

// 終了
void TextureManager::Finalize() {
    textureDatas_.clear();
    instance_.reset();
}

// SRVインデックスの開始番号
uint32_t TextureManager::GetSrvIndex(const std::string &filePath) {
    const TextureData* data = FindTextureData(filePath);

    if (data) {
        return data->srvIndex;
    }

    // ロードされていなかったらエラー
    Logger::Log("ERROR: Texture not loaded for path: " + filePath);
    assert(false && "Texture not loaded!");
    return 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE
TextureManager::GetSrvHandleGPU(const std::string &filePath) {
  // 配列の境界チェック（arrayIndexが配列のサイズ未満であることを確認）
  assert(SrvManager::GetInstance()->AllocatableTexture() &&
         "Error: texture array index out of bounds!");

  const TextureData* data = FindTextureData(filePath);

  if (data) {
      return data->srvHandleGPU;
  }

  Logger::Log("ERROR: Requested texture not found in map: " + filePath);
  assert(false && "Texture data not found!");
  return D3D12_GPU_DESCRIPTOR_HANDLE{};
}

const DirectX::TexMetadata &
TextureManager::GetMetaData(const std::string &filePath) {
  // 配列の境界チェック（arrayIndexが配列のサイズ未満であることを確認）
  assert(SrvManager::GetInstance()->AllocatableTexture() &&
         "Error: texture array index out of bounds!");

  const TextureData* data = FindTextureData(filePath);

  if (data) {
      return data->metadata;
  }

  Logger::Log("ERROR: Requested texture not found in map: " + filePath);
  assert(false && "Texture data not found!");
  static DirectX::TexMetadata errorMetadata{};
  return errorMetadata;
}

// テクスチャロード
void TextureManager::LoadTexture(const std::string &filePath) {

    std::string finalPath;

    // A. まず、渡されたパスそのまま（モデルフォルダ付きパスなど）で存在するか確認
    if (std::filesystem::exists(filePath)) {
        finalPath = filePath;
    } else {
        // B. 見つからない場合、汎用テクスチャフォルダ内を探す
        finalPath = "Resources/Textures/" + filePath;

        if (!std::filesystem::exists(finalPath)) {
            Logger::Log("ERROR: Texture not found. Tried:\n 1. " + filePath + "\n 2. " + finalPath);
            assert(false);
            return;
        }
    }

  // 読み込み済みテクスチャを検索
  auto it = textureDatas_.find(finalPath);

  if (it != textureDatas_.end()) {
    return;
  }

  // コマンドリストが開いていない場合は、一時的にコマンドリストを開く
  bool needSync = !DX12Context::GetInstance()->IsCommandListOpen();
  if (needSync) {
    DX12Context::GetInstance()->ResetCommandList();
  }

  // テクスチャ枚数上限チェック
  assert(SrvManager::GetInstance()->AllocatableTexture());

  HRESULT hr;

  // テクスチャを読み込んでプログラムで扱えるようにする
  DirectX::ScratchImage image{};
  std::wstring filePathW = ConvertString(finalPath);
  if (filePathW.ends_with(L".dds")) { // DDSファイルはWICで読み込めないため、DirectXTexのDDSローダーを使用
      hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
  }
  else {
      hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
  }
  assert(SUCCEEDED(hr));

  // ミップマップの作成
  DirectX::ScratchImage mipImages{};
  if (DirectX::IsCompressed(image.GetMetadata().format)) {
	  mipImages = std::move(image); // 圧縮テクスチャはミップマップを生成せず、そのまま使用
  }
  else {
      hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(),
          image.GetMetadata(), DirectX::TEX_FILTER_SRGB,
          4, mipImages);
  }
  assert(SUCCEEDED(hr));

  Logger::Log("INFO: Loaded texture at path: " + finalPath + "\n");

  // テクスチャデータを追加
  TextureData &textureData = textureDatas_[finalPath];

  textureData.metadata = mipImages.GetMetadata();
  textureData.resource = DX12Context::GetInstance()->CreateTextureResource(textureData.metadata);

  // SRV確保
  textureData.srvIndex = SrvManager::GetInstance()->Allocate();
  textureData.srvHandleCPU =
      SrvManager::GetInstance()->GetCPUDescriptorHandle(textureData.srvIndex);
  textureData.srvHandleGPU =
      SrvManager::GetInstance()->GetGPUDescriptorHandle(textureData.srvIndex);

  SrvManager::GetInstance()->CreateSRVForTexture(
      textureData.srvIndex, textureData.resource, textureData.metadata.format,
	  UINT(textureData.metadata.mipLevels), textureData.metadata.IsCubemap()); // CubeMapかどうかもSRV作成時に渡す

  // テクスチャリソースをアップロードし、コマンドリストに積む
  textureData.intermediateResource =
      DX12Context::GetInstance()->UploadTextureData(textureData.resource, mipImages);

  // 一時的にコマンドリストを開いた場合は、即座に実行・同期して閉じる
  if (needSync) {
    DX12Context::GetInstance()->ExecuteInitialCommandAndSync();
  }
}

// 中間リソースをまとめて解放する
void TextureManager::ReleaseIntermediateResources() {
  for (auto &pair : textureDatas_) {
    // intermediateResourceを解放
    pair.second.intermediateResource.Reset();
  }

  Logger::Log("INFO: All intermediate texture resources have been released.\n");
}

const TextureManager::TextureData* TextureManager::FindTextureData(const std::string& filePath) {
    // 1. そのままのパスで検索（モデル用など）
    auto it = textureDatas_.find(filePath);
    if (it != textureDatas_.end()) {
        return &it->second;
    }

    // 2. デフォルトディレクトリを付与して検索（スプライト用など）
    std::string fullPath = filePath;
    if (filePath.find("Resources/Textures/") == std::string::npos &&
        filePath.find("resources/textures/") == std::string::npos) {
        fullPath = "Resources/Textures/" + filePath;
    }

    it = textureDatas_.find(fullPath);
    if (it != textureDatas_.end()) {
        return &it->second;
    }

    return nullptr; // 見つからない
}
