#define NOMINMAX

#include "Model.h"
#include "Base/DX12Context.h"
#include "Texture/TextureManager.h"

#include "Math/Functions/MathUtils.h"
#include "Math/Matrix/MatrixGenerators.h"
#include "Logger.h"

#include <assert.h>
#include <fstream>
#include <sstream>
#include <string>
#include <numbers>
#include <algorithm>

using namespace Microsoft::WRL;
using namespace MathUtils;
using namespace MathGenerators;

void Model::Initialize(const std::string &directoryPath,
                       const std::string &filename) {
  // モデル読み込み
  LoadModelFile(directoryPath, filename);

  // 頂点データを作成する
  CreateVertexResource();

  // インデックスバッファ作成
  CreateIndexResource();

  // マテリアルバッファの作成
  CreateMaterialResource();

  // .objの参照しているテクスチャファイル読み込み、インデックスを代入
  TextureManager::GetInstance()->LoadTexture(
      modelData_.material.textureFilePath);

  modelData_.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(
      modelData_.material.textureFilePath);
}

void Model::CreateRing(const std::string& textureFilePath, float innerRadius, float outerRadius, uint32_t division) {
    // 既存リソースがある場合はGPUの完了を待ち、アンマップして安全に破棄できるようにする
    if (vertexResource_ || indexResource_ || materialResource_) {
        DX12Context::GetInstance()->WaitForGpu();
        if (vertexResource_ && vertexData_) {
            vertexResource_->Unmap(0, nullptr);
            vertexData_ = nullptr;
        }
        if (materialResource_ && materialData_) {
            materialResource_->Unmap(0, nullptr);
            materialData_ = nullptr;
        }
    }

    // データをクリアしておく
    modelData_.vertices.clear();
    modelData_.indices.clear();

    // ============================================================
    // 【ガード処理】不正な値をシャットアウト
    // ============================================================
    uint32_t safeDivision = std::max(3u, division);
    if (safeDivision > 256) safeDivision = 256; // 念のため上限もガード

    float safeInnerRadius = std::max(0.0f, innerRadius);
    // ============================================================

    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(safeDivision);

    // 頂点データの生成
    for (uint32_t i = 0; i <= safeDivision; ++i) {
        float angle = i * radianPerDivide;
        float s = std::sin(angle);
        float c = std::cos(angle);
        float u = float(i) / float(safeDivision);

        // 外側の頂点
        VertexData outerVertex;
        outerVertex.position = { -s * outerRadius, c * outerRadius, 0.0f, 1.0f };
        outerVertex.normal = { 0.0f, 0.0f, -1.0f };
        outerVertex.texcoord = { u, 0.0f };
        modelData_.vertices.push_back(outerVertex);

        // 内側の頂点
        VertexData innerVertex;
        innerVertex.position = { -s * safeInnerRadius, c * safeInnerRadius, 0.0f, 1.0f };
        innerVertex.normal = { 0.0f, 0.0f, -1.0f };
        innerVertex.texcoord = { u, 1.0f };
        modelData_.vertices.push_back(innerVertex);
    }

    // インデックスデータの生成
    for (uint32_t i = 0; i < safeDivision; ++i) {
        uint32_t base = i * 2;
        // 頂点レイアウト: [Outer0, Inner0, Outer1, Inner1, ...]
        // 三角形1
        modelData_.indices.push_back(base);     // Outer i
        modelData_.indices.push_back(base + 2); // Outer i+1
        modelData_.indices.push_back(base + 1); // Inner i
        // 三角形2
        modelData_.indices.push_back(base + 1); // Inner i
        modelData_.indices.push_back(base + 2); // Outer i+1
        modelData_.indices.push_back(base + 3); // Inner i+1
    }

    // マテリアル設定
    modelData_.material.textureFilePath = textureFilePath;
    modelData_.rootNode.localMatrix = MakeIdentity4x4();
    modelData_.rootNode.name = "Ring";

    // リソースの作成
    CreateVertexResource();
    CreateIndexResource();
    CreateMaterialResource();

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData_.material.textureFilePath);
}

void Model::CreateRing(const std::string& textureFilePath, const RingSettings& settings) {
    // 既存リソースがある場合はGPUの完了を待ち、アンマップして安全に破棄できるようにする
    if (vertexResource_ || indexResource_ || materialResource_) {
        DX12Context::GetInstance()->WaitForGpu();
        if (vertexResource_ && vertexData_) {
            vertexResource_->Unmap(0, nullptr);
            vertexData_ = nullptr;
        }
        if (materialResource_ && materialData_) {
            materialResource_->Unmap(0, nullptr);
            materialData_ = nullptr;
        }
    }

    // データをクリアしておく
    modelData_.vertices.clear();
    modelData_.indices.clear();

    // ============================================================
    // 【ガード処理】不正な値をシャットアウト
    // ============================================================
    uint32_t safeDivision = std::max(3u, settings.division);
    if (safeDivision > 256) safeDivision = 256; // 念のため上限もガード

    float safeInnerRadius = std::max(0.0f, settings.innerRadius);
    // ============================================================

    float startRad = settings.startAngle * std::numbers::pi_v<float> / 180.0f;
    float endRad = settings.endAngle * std::numbers::pi_v<float> / 180.0f;
    float angleRange = endRad - startRad;

    float rStart = settings.startOuterRadius;
    float rMid = settings.midOuterRadius;
    float rEnd = settings.endOuterRadius;

    // 二次スプライン（放物線補間）係数の算出
    float aCoeff = 2.0f * rStart - 4.0f * rMid + 2.0f * rEnd;
    float bCoeff = -3.0f * rStart + 4.0f * rMid - rEnd;
    float cCoeff = rStart;

    // 頂点データの生成
    for (uint32_t i = 0; i <= safeDivision; ++i) {
        float t = float(i) / float(safeDivision);
        float angle = startRad + t * angleRange;
        float s = std::sin(angle);
        float c = std::cos(angle);

        // スプライン曲線による外径の計算
        float outerRadius = aCoeff * t * t + bCoeff * t + cCoeff;

        // 外側の頂点
        VertexData outerVertex;
        outerVertex.position = { -s * outerRadius, c * outerRadius, 0.0f, 1.0f };
        outerVertex.normal = { 0.0f, 0.0f, -1.0f };
        if (settings.isUvSwap) {
            outerVertex.texcoord = { 0.0f, t };
        } else {
            outerVertex.texcoord = { t, 0.0f };
        }
        modelData_.vertices.push_back(outerVertex);

        // 内側の頂点
        VertexData innerVertex;
        innerVertex.position = { -s * safeInnerRadius, c * safeInnerRadius, 0.0f, 1.0f };
        innerVertex.normal = { 0.0f, 0.0f, -1.0f };
        if (settings.isUvSwap) {
            innerVertex.texcoord = { 1.0f, t };
        } else {
            innerVertex.texcoord = { t, 1.0f };
        }
        modelData_.vertices.push_back(innerVertex);
    }

    // インデックスデータの生成
    for (uint32_t i = 0; i < safeDivision; ++i) {
        uint32_t base = i * 2;
        // 三角形1
        modelData_.indices.push_back(base);     // Outer i
        modelData_.indices.push_back(base + 2); // Outer i+1
        modelData_.indices.push_back(base + 1); // Inner i
        // 三角形2
        modelData_.indices.push_back(base + 1); // Inner i
        modelData_.indices.push_back(base + 2); // Outer i+1
        modelData_.indices.push_back(base + 3); // Inner i+1
    }

    // マテリアル設定
    modelData_.material.textureFilePath = textureFilePath;
    modelData_.rootNode.localMatrix = MakeIdentity4x4();
    modelData_.rootNode.name = "Ring";

    // リソースの作成
    CreateVertexResource();
    CreateIndexResource();
    CreateMaterialResource();

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData_.material.textureFilePath);
}

void Model::CreateCylinder(const std::string& textureFilePath, const CylinderSettings& settings) {
    // 既存リソースがある場合はGPUの完了を待ち、アンマップして安全に破棄できるようにする
    if (vertexResource_ || indexResource_ || materialResource_) {
        DX12Context::GetInstance()->WaitForGpu();
        if (vertexResource_ && vertexData_) {
            vertexResource_->Unmap(0, nullptr);
            vertexData_ = nullptr;
        }
        if (materialResource_ && materialData_) {
            materialResource_->Unmap(0, nullptr);
            materialData_ = nullptr;
        }
    }

    // データをクリアしておく
    modelData_.vertices.clear();
    modelData_.indices.clear();

    // ============================================================
    // 【安全ガード処理】関数の先頭で不正な値をシャットアウト
    // ============================================================
    uint32_t safeDivision = std::max(3u, settings.division);
    if (safeDivision > 256) safeDivision = 256;

    uint32_t safeVerticalDivision = std::max(1u, settings.verticalDivision);
    if (safeVerticalDivision > 256) safeVerticalDivision = 256;
	// ============================================================

    float startRad = settings.startAngle * std::numbers::pi_v<float> / 180.0f;
    float endRad = settings.endAngle * std::numbers::pi_v<float> / 180.0f;
    float angleRange = endRad - startRad;

    // 頂点の生成 (縦方向 yIndex: 0 から verticalDivision)
    for (uint32_t yIndex = 0; yIndex <= safeVerticalDivision; ++yIndex) {
        float vNorm = float(yIndex) / float(safeVerticalDivision);
        float h = (1.0f - vNorm) * settings.height; // yIndex=0 が上(高さ height), yIndex=verticalDivision が下(高さ 0)

        // 上底から下底にかけての半径を線形補間 (楕円対応)
        Vector2 radius = {
            settings.topRadius.x * (1.0f - vNorm) + settings.bottomRadius.x * vNorm,
            settings.topRadius.y * (1.0f - vNorm) + settings.bottomRadius.y * vNorm
        };

        // 円周方向の頂点生成 (xIndex: 0 から division)
        for (uint32_t xIndex = 0; xIndex <= safeDivision; ++xIndex) {
            float uNorm = float(xIndex) / float(safeDivision);
            float angle = startRad + uNorm * angleRange;
            float s = std::sin(angle);
            float c = std::cos(angle);

            // 頂点の位置 (X, Y, Z)
            VertexData vertex;
            vertex.position = { -s * radius.x, h, c * radius.y, 1.0f };

            // 法線：Y軸の高さは無視し、X, Z方向の外側を向く法線
            vertex.normal = { -s, 0.0f, c };
            float normalLen = std::sqrt(vertex.normal.x * vertex.normal.x + vertex.normal.z * vertex.normal.z);
            if (normalLen > 0.0f) {
                vertex.normal.x /= normalLen;
                vertex.normal.z /= normalLen;
            }

            // UV座標
            float u = uNorm;
            float v = vNorm;
            if (settings.flipV) {
                v = 1.0f - v;
            }
            if (settings.isUvSwap) {
                std::swap(u, v);
            }
            vertex.texcoord = { u, v };

            modelData_.vertices.push_back(vertex);
        }
    }

    // インデックスデータの生成
    for (uint32_t y = 0; y < safeVerticalDivision; ++y) {
        for (uint32_t x = 0; x < safeDivision; ++x) {
            uint32_t topLeft = y * (safeDivision + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * (safeDivision + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;

            // 三角形1
            modelData_.indices.push_back(topLeft);
            modelData_.indices.push_back(topRight);
            modelData_.indices.push_back(bottomLeft);

            // 三角形2
            modelData_.indices.push_back(bottomLeft);
            modelData_.indices.push_back(topRight);
            modelData_.indices.push_back(bottomRight);
        }
    }

    // マテリアル設定
    modelData_.material.textureFilePath = textureFilePath;
    modelData_.rootNode.localMatrix = MakeIdentity4x4();
    modelData_.rootNode.name = "Cylinder";

    // リソースの作成
    CreateVertexResource();
    CreateIndexResource();
    CreateMaterialResource();

    // テクスチャ読み込み
    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData_.material.textureFilePath);
}

void Model::CreatePlane(const std::string &textureFilePath, const PlaneSettings& settings) {
    modelData_.vertices.clear();
    modelData_.indices.clear();

    uint32_t divX = settings.divisionX > 0 ? settings.divisionX : 1;
    uint32_t divY = settings.divisionY > 0 ? settings.divisionY : 1;

    float stepX = settings.size.x / static_cast<float>(divX);
    float stepY = settings.size.y / static_cast<float>(divY);

    float startX = -settings.size.x * 0.5f;
    float startY = -settings.size.y * 0.5f;

    // 頂点生成
    for (uint32_t y = 0; y <= divY; ++y) {
        float posY = startY + static_cast<float>(y) * stepY;
        float v = static_cast<float>(y) / static_cast<float>(divY);
        if (settings.flipV) {
            v = 1.0f - v;
        }

        for (uint32_t x = 0; x <= divX; ++x) {
            float posX = startX + static_cast<float>(x) * stepX;
            float u = static_cast<float>(x) / static_cast<float>(divX);

            VertexData vertex;
            vertex.position = { posX, posY, 0.0f, 1.0f };
            vertex.normal = { 0.0f, 0.0f, -1.0f };
            
            float finalU = u;
            float finalV = v;
            if (settings.isUvSwap) {
                std::swap(finalU, finalV);
            }
            vertex.texcoord = { finalU, finalV };

            modelData_.vertices.push_back(vertex);
        }
    }

    // インデックス生成
    for (uint32_t y = 0; y < divY; ++y) {
        for (uint32_t x = 0; x < divX; ++x) {
            uint32_t topLeft = y * (divX + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * (divX + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;

            // 三角形1
            modelData_.indices.push_back(topLeft);
            modelData_.indices.push_back(topRight);
            modelData_.indices.push_back(bottomLeft);

            // 三角形2
            modelData_.indices.push_back(bottomLeft);
            modelData_.indices.push_back(topRight);
            modelData_.indices.push_back(bottomRight);
        }
    }

    modelData_.material.textureFilePath = textureFilePath;
    modelData_.rootNode.localMatrix = MakeIdentity4x4();
    modelData_.rootNode.name = "Plane";

    CreateVertexResource();
    CreateIndexResource();
    CreateMaterialResource();

    // マテリアルの初期色を設定
    if (materialData_) {
        materialData_->color = settings.color;
    }

    TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
    modelData_.material.textureIndex = TextureManager::GetInstance()->GetSrvIndex(modelData_.material.textureFilePath);
}

// 描画処理
void Model::Draw() {
  // VertexBufferの設定
  DX12Context::GetInstance()->GetCommandList()->IASetVertexBuffers(
      0, 1, &vertexBufferView_);
  // IBV(インデックスバッファビュー)の設定
  DX12Context::GetInstance()->GetCommandList()->IASetIndexBuffer(
      &indexBufferView_);
  // マテリアルCBVの設定
  DX12Context::GetInstance()
      ->GetCommandList()
      ->SetGraphicsRootConstantBufferView(
          0, materialResource_->GetGPUVirtualAddress());
  // SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
  DX12Context::GetInstance()->GetCommandList()->SetGraphicsRootDescriptorTable(
      2, TextureManager::GetInstance()->GetSrvHandleGPU(
             modelData_.material.textureFilePath));

  // Dissolve用のマスクテクスチャを設定。8はrootParameter[8]である。
  DX12Context::GetInstance()->GetCommandList()->SetGraphicsRootDescriptorTable(
      8, TextureManager::GetInstance()->GetSrvHandleGPU(
             dissolveMaskFilePath_));

  // 描画コマンド
  // 引数: (インデックス数, インスタンス数, インデックス開始位置, 頂点オフセット, インスタンスオフセット)
  DX12Context::GetInstance()->GetCommandList()->DrawIndexedInstanced(
      UINT(modelData_.indices.size()), 1, 0, 0, 0);
}

// .objファイルの読み取り
void Model::LoadModelFile(const std::string &directoryPath,
                          const std::string &fileName) {
    Assimp::Importer importer;
    std::string fullPath = directoryPath + "/" + fileName;

    // 現在成功している設定（左手系・時計回り・UV反転）を維持
    const aiScene* scene = importer.ReadFile(fullPath.c_str(),
        aiProcess_FlipWindingOrder | // 時計回りに変換
        aiProcess_FlipUVs |          // UV反転
        //aiProcess_MakeLeftHanded |   // 左手系に変換
        aiProcess_Triangulate        // 三角形化
    );

    // シーンの読み込みチェック
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::Log("ERROR: Assimp import failed: " + std::string(importer.GetErrorString()));
        assert(false && "Assimp import failed");
        return;
    }

    // データをクリアしておく
    modelData_.vertices.clear();
    modelData_.indices.clear();

    // --- メッシュの解析（複数メッシュ対応） ---
    for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        assert(mesh->HasNormals());
        assert(mesh->HasTextureCoords(0));
        assert(mesh->HasFaces());

        // 重要：現在の頂点数を保持しておく（これがインデックスのオフセットになる）
        // 1つ目のメッシュなら0、2つ目なら1つ目の頂点数がここに入る
        uint32_t indexOffset = static_cast<uint32_t>(modelData_.vertices.size());

        // --- 頂点データの追加 ---
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            aiVector3D& position = mesh->mVertices[i];
            aiVector3D& normal = mesh->mNormals[i];
            aiVector3D& texcoord = mesh->mTextureCoords[0][i];

            VertexData vertex;
            vertex.position = { position.x, position.y, position.z, 1.0f };
            vertex.normal = { normal.x, normal.y, normal.z };
            vertex.texcoord = { texcoord.x, texcoord.y };

            vertex.position.x *= -1.0f;
            vertex.normal.x *= -1.0f;

            modelData_.vertices.push_back(vertex);
        }

        // --- インデックスデータの追加 ---
        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            aiFace& face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3);

            // インデックスにオフセットを足して、全体の通し番号にする
            modelData_.indices.push_back(face.mIndices[0] + indexOffset);
            modelData_.indices.push_back(face.mIndices[1] + indexOffset);
            modelData_.indices.push_back(face.mIndices[2] + indexOffset);
        }
    }

    // --- マテリアルの解析 ---
    // ※現在の構造（Modelクラスにテクスチャが1つ）だと、複数のテクスチャを持つモデルは正しく表現できない
    // そのため、とりあえず「最初にテクスチャを持っているメッシュのマテリアル」を採用する
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* material = scene->mMaterials[i];
        if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
            aiString textureFilePath;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
            modelData_.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
            // ひとつ見つかったら終了（現在の仕様の限界）
            break;
        }
    }

    if (modelData_.material.textureFilePath.empty()) {
        modelData_.material.textureFilePath = TextureManager::GetInstance()->defaultTexturePath_;
    }

    // --- ノードの解析 ---
    modelData_.rootNode = ReadNode(scene->mRootNode);

    // データが空でないか最終チェック
    assert(!modelData_.vertices.empty() && "Vertex data is empty");
    assert(!modelData_.indices.empty() && "Index data is empty");
}

Node Model::ReadNode(aiNode* node) {
    Node result;
    aiMatrix4x4 aiLocalMatrix = node->mTransformation; // nodeのMatrixを取得
    aiLocalMatrix.Transpose(); // 列ベクトル形式を行ベクトル形式に転置（Assimpは列優先、DirectXは行優先のため）
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.localMatrix.m[i][j] = aiLocalMatrix[i][j];
        }
    }

    result.name = node->mName.C_Str(); // Node名を取得
    result.children.resize(node->mNumChildren); // 子供の数だけ確保
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        // 再帰的に読んで階層構造を作っていく
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }
    return result;
}

void Model::CreateIndexResource() {
  // リソースのサイズ（インデックス数 * uint32_tのサイズ）
  size_t sizeInBytes = sizeof(uint32_t) * modelData_.indices.size();

  // リソース作成
  indexResource_ =
      DX12Context::GetInstance()->CreateBufferResource(sizeInBytes);

  // インデックスバッファビューの作成
  indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
  indexBufferView_.SizeInBytes = UINT(sizeInBytes);
  indexBufferView_.Format = DXGI_FORMAT_R32_UINT; // uint32_tを使用

  // データの書き込み
  uint32_t *mappedIndex = nullptr;
  indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&mappedIndex));
  std::memcpy(mappedIndex, modelData_.indices.data(), sizeInBytes);
  indexResource_->Unmap(0, nullptr);
}

// 頂点バッファの作成
void Model::CreateVertexResource() {

#pragma region リソースとバッファビューの作成
  // リソースとバッファビューの作成

  // VertexResourceを作る
  vertexResource_ = DX12Context::GetInstance()->CreateBufferResource(
      sizeof(VertexData) * modelData_.vertices.size());

  // VertexBufferViewを作成する
  // リソースの先頭アドレスから使う
  vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
  // 使用するリソースのサイズは頂点6つ分のサイズ
  vertexBufferView_.SizeInBytes =
      UINT(sizeof(VertexData) * modelData_.vertices.size());
  // １頂点当たりのサイズ
  vertexBufferView_.StrideInBytes = sizeof(VertexData);

#pragma endregion ここまで

#pragma region VertexDataの設定
  // VertexDataの設定

  // 書き込むためのアドレスを取得
  vertexResource_->Map(
      0, nullptr,
      reinterpret_cast<void **>(&vertexData_)); // 書き込むためのアドレスを取得
  std::memcpy(vertexData_, modelData_.vertices.data(),
              sizeof(VertexData) *
                  modelData_.vertices.size()); // 頂点データをリソースにコピー

#pragma endregion ここまで
}

// マテリアルバッファの作成
void Model::CreateMaterialResource() {

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
  materialData_->enableLighting = true;
  // UV変換行列を単位行列に設定
  materialData_->uvTransform = MakeIdentity4x4();
  // 光沢度を設定
  materialData_->shininess = 10.0f;
  // 環境マップ係数を設定
  materialData_->environmentCoefficient = 0.0f;

  // Dissolveパラメータの初期化
  materialData_->enableDissolve = 0;
  materialData_->dissolveThreshold = 0.0f;
  materialData_->dissolveEdgeRange = 0.02f;
  materialData_->dissolveEdgeColor = Vector3(1.0f, 0.4f, 0.3f);

#pragma endregion ここまで
}

void Model::SetDissolveMaskTexture(const std::string &filePath) {
  dissolveMaskFilePath_ = filePath;
  TextureManager::GetInstance()->LoadTexture(dissolveMaskFilePath_);
}

void Model::SetDissolveParams(int32_t enable, float threshold, float edgeRange, const Vector3 &edgeColor) {
  if (materialData_) {
    materialData_->enableDissolve = enable;
    materialData_->dissolveThreshold = threshold;
    materialData_->dissolveEdgeRange = edgeRange;
    materialData_->dissolveEdgeColor = edgeColor;
  }
}
