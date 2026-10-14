#include "Object3dCommon.h"

#include <cassert>

#include "Base/DX12Context.h"
#include "Camera/Camera.h"
#include "Light/LightManager.h"
#include "PSO/PipelineManager.h"
#include "Base/SrvManager.h"

#include "externals/DirectXTex/d3dx12.h"

using namespace Microsoft::WRL;
using namespace BlendMode;

std::unique_ptr<Object3dCommon> Object3dCommon::instance_ = nullptr;

// シングルトンインスタンスの実装
Object3dCommon *Object3dCommon::GetInstance() {
    if (instance_ == nullptr) {
        instance_ = std::make_unique<Object3dCommon>(Token{});
    }
    return instance_.get();
}

void Object3dCommon::Destroy() {
    instance_.reset();
}

Object3dCommon::Object3dCommon(Token) {
    // コンストラクタ
}

void Object3dCommon::Initialize() {
  // 頂点レイアウト
  D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
       D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
       0},
  };
  // const UINT kNumElements = _countof(inputLayout);

  // ラスタライザステート
  D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
#ifdef _DEBUG
  rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 背面カリング <==== ※PSOをそれぞれのカリングに対応させるまでは無しにしておく(ライティングで裏部分は変になるので注意！)
#else
  rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; // 背面カリング <==== ※本来はこっち
#endif // _DEBUG

  // ワイヤーフレーム用のラスタライザステート
  D3D12_RASTERIZER_DESC wireframeRasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  wireframeRasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME; // ワイヤーフレーム描画
  wireframeRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 裏面の線も表示する


  // デプスステンシルステート
  D3D12_DEPTH_STENCIL_DESC depthStencilDesc =
      CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

  // BlendModeごとの設定は汎用関数から取得
  std::array<D3D12_BLEND_DESC, kCountOfBlendMode> blendDescs =
      CreateBlendStateDescs();

  // パイプラインマネージャを使って、ブレンドモードごとのPSOを生成
  for (size_t i = 0; i < kCountOfBlendMode; ++i) {
      // 半透明描画の場合は深度書き込みを禁止する
      if (i == kBlendModeNone || i == kBlendModeNormal) {
          depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 書き込み有効
      } else {
          depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 書き込み無効
      }

      // PSOを生成して配列に格納
      psoArray_[i] = PipelineManager::GetInstance()->CreateObject3dPSO(
          inputLayout, _countof(inputLayout), rasterizerDesc, depthStencilDesc,
          blendDescs[i]);

      // ワイヤーフレーム用
      psoWireframeArray_[i] = PipelineManager::GetInstance()->CreateObject3dPSO(
          inputLayout, _countof(inputLayout), wireframeRasterizerDesc, depthStencilDesc,
          blendDescs[i]);
  }
}

void Object3dCommon::Finalize() {
    // デフォルトカメラの参照をクリア
    defaultCamera_ = nullptr;

    instance_.reset();
}

void Object3dCommon::SetCommonDrawSettings(BlendState currentBlendMode, bool isWireframe) {
  // ルートシグネチャ、PSO、トポロジの設定
  DX12Context::GetInstance()->GetCommandList()->SetGraphicsRootSignature(
      PipelineManager::GetInstance()->Get3DRootSignature());
  if (currentBlendMode >= 0 && currentBlendMode < kCountOfBlendMode) {
    // PSOを設定
      if (isWireframe) {
          // ワイヤーフレーム用のPSOを設定
          DX12Context::GetInstance()->GetCommandList()->SetPipelineState(
              psoWireframeArray_[currentBlendMode].Get());
      } else {
          // 通常のPSOを設定
          DX12Context::GetInstance()->GetCommandList()->SetPipelineState(
              psoArray_[currentBlendMode].Get());
      }
  } else {
    // 不正な値の場合
    DX12Context::GetInstance()->GetCommandList()->SetPipelineState(
        psoArray_[kBlendModeNormal].Get());
  }
  DX12Context::GetInstance()->GetCommandList()->IASetPrimitiveTopology(
      D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // 2. ライトの一括設定
  LightManager::GetInstance()->Draw();

  // 3. カメラの一括設定 (RootParameter Index 4)
  if (defaultCamera_) {
    DX12Context::GetInstance()
        ->GetCommandList()
        ->SetGraphicsRootConstantBufferView(
            4, defaultCamera_->GetConstantBufferGPUVirtualAddress());
  }

  // 4. 環境マップの設定 (RootParameter Index 7)
  SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(7, environmentMapSrvIndex_);
}
