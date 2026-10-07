#include "Object3d.h"
#include "Base/DX12Context.h"
#include "Camera/Camera.h"
#include "Light/LightManager.h"
#include "Model/Model.h"
#include "Model/ModelManager.h"
#include "Object3dCommon.h"
#include "Texture/TextureManager.h"

#include "Math/Functions/MathUtils.h"
#include "Math/Matrix/MatrixGenerators.h"

#include <assert.h>

using namespace Microsoft::WRL;
using namespace MathUtils;
using namespace MathGenerators;

Object3d::~Object3d() {
    // Map済みリソースをUnmap
    for (uint32_t i = 0; i < kMaxViews; ++i) {
        if (transformationMatrixResources_[i] && transformationMatrixData_[i]) {
            transformationMatrixResources_[i]->Unmap(0, nullptr);
            transformationMatrixData_[i] = nullptr;
        }
    }
}

void Object3d::Initialize() {
  camera_ = Object3dCommon::GetInstance()->GetDefaultCamera();

  // 変換行列バッファの作成
  CreateTransformationMatrixResource();
}

// 更新処理 (全ビュー更新)
void Object3d::Update() {
    for (uint32_t i = 0; i < kMaxViews; ++i) {
        Update(i, camera_);
    }
}

// 指定したビュー用の更新
void Object3d::Update(uint32_t viewIndex, Camera* camera) {
    assert(viewIndex < kMaxViews);

    // Transform情報からワールド行列を作る (全ビュー共通)
    Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate,
                                             transform_.translate);

    // Model側のRootNodeのTransformを反映する
    if (model_) {
        Matrix4x4 rootMatrix = model_->GetRootNode().localMatrix;
        worldMatrix = rootMatrix * worldMatrix;
    }

    // ワールドビュー射影行列の計算
    Matrix4x4 wvpMatrix;
    if (camera) {
        const Matrix4x4 &viewProjectionMatrix = camera->GetViewProjectionMatrix();
        wvpMatrix = worldMatrix * viewProjectionMatrix;
    } else {
        wvpMatrix = worldMatrix;
    }

    // 非均一スケール対応：逆転置行列の計算
    Matrix4x4 worldInverseTranspose = Transpose(Inverse(worldMatrix));

    // 指定されたビューのデータを更新
    transformationMatrixData_[viewIndex]->WVP = wvpMatrix;
    transformationMatrixData_[viewIndex]->World = worldMatrix;
    transformationMatrixData_[viewIndex]->WorldInverseTranspose = worldInverseTranspose;
}

// 描画処理
void Object3d::Draw(uint32_t viewIndex) {
    assert(viewIndex < kMaxViews);

    // 変換行列CBVの設定
    DX12Context::GetInstance()
        ->GetCommandList()
        ->SetGraphicsRootConstantBufferView(
            1, transformationMatrixResources_[viewIndex]->GetGPUVirtualAddress());

    // 描画コマンド
    if (model_) {
        if (!overrideTexturePath_.empty()) {
            model_->SetTextureFilePath(overrideTexturePath_);
        }
        model_->SetUvScale(uvScale_);
        model_->Draw();
    }
}

void Object3d::SetModel(const std::string &filepath) {
  // モデルを検索してセットする
  model_ = ModelManager::GetInstance()->FindModel(filepath);
}

// 変換行列バッファの作成
void Object3d::CreateTransformationMatrixResource() {
    for (uint32_t i = 0; i < kMaxViews; ++i) {
        // 変換行列リソースの作成
        transformationMatrixResources_[i] =
            DX12Context::GetInstance()->CreateBufferResource(
                sizeof(TransformationMatrix));

        // TransformationMatrixDataの設定
        transformationMatrixResources_[i]->Map(
            0, nullptr, reinterpret_cast<void **>(&transformationMatrixData_[i]));
        
        // 単位行列で初期化
        transformationMatrixData_[i]->WVP = MakeIdentity4x4();
        transformationMatrixData_[i]->World = MakeIdentity4x4();
        transformationMatrixData_[i]->WorldInverseTranspose = MakeIdentity4x4();
    }
}
