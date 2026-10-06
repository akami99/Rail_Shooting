#include "PipelineManager.h"

#include "Base/DX12Context.h"
#include "Logger/Logger.h"

#include <cassert>

using namespace Microsoft::WRL;
using namespace Logger;

// 静的メンバ変数の実体
std::unique_ptr<PipelineManager> PipelineManager::instance_ = nullptr;

PipelineManager* PipelineManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique<PipelineManager>(Token{});
	}
	return instance_.get();
}

void PipelineManager::Destroy() {
	instance_.reset();
}

PipelineManager::PipelineManager(Token) {
	// コンストラクタ
}

void PipelineManager::Initialize() {
	// シェーダーのロード
	LoadShader();

	// ルートシグネチャの生成
	CreateRootSignature();
}

// グラフィックスパイプラインの生成
ComPtr<ID3D12PipelineState>
PipelineManager::CreateSpritePSO(const D3D12_BLEND_DESC& blendDesc) {

#pragma region PSO共通設定
	// PSO共通設定

	// ---固定で良いのでこの段階で設定---
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

	// ラスタライザステート作成
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 三角形の中を塗りつぶす
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // カリングなし

	// Depth/Stencilステート
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = FALSE; // 2DなのでDepthテスト無効
	// ------------------------------

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignatureSprite_.Get(); // Sprite用Root Signature
	// Sprite用シェーダー
	psoDesc.VS = { vsBlobSprite_->GetBufferPointer(),
				  vsBlobSprite_->GetBufferSize() };
	psoDesc.PS = { psBlobSprite_->GetBufferPointer(),
				  psBlobSprite_->GetBufferSize() };
	// ステート
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	// その他の共通設定
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

#pragma endregion ここまで

#pragma region PSO生成

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(
		&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

#pragma endregion ここまで

	return pso;
}

ComPtr<ID3D12PipelineState>
PipelineManager::CreateParticlePSO(const D3D12_BLEND_DESC& blendDesc) {

	// ---------------------------------------------------------------------
	// I. サブステートの定義 (BlendState, RasterizerState, DepthStencilState,
	// InputLayout)
	// ---------------------------------------------------------------------

	// 1. InputLayout (頂点レイアウト)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].InputSlot = 0;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[0].InputSlotClass =
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElementDescs[0].InstanceDataStepRate = 0;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].InputSlot = 0;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].InputSlotClass =
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElementDescs[1].InstanceDataStepRate = 0;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].InputSlot = 0;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].InputSlotClass =
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	inputElementDescs[2].InstanceDataStepRate = 0;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// 2. RasterizerState (ラスタライザ設定)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // 裏面カリング無効
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 三角形の中を塗りつぶす

	// 3. DepthStencilState (深度/ステンシル設定)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true; // 深度テスト有効 (3D描画のため)
	depthStencilDesc.DepthWriteMask =
		D3D12_DEPTH_WRITE_MASK_ZERO; // 深度書き込み無効 (ブレンドのため)
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 比較関数

	// ---------------------------------------------------------------------
	// II. グラフィックスパイプラインステート (PSO) の構築
	// ---------------------------------------------------------------------

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

	// シェーダとシグネチャ (パイプラインの核となる部分)
	psoDesc.pRootSignature = rootSignatureParticle_.Get();
	psoDesc.VS = { vsBlobParticle_->GetBufferPointer(),
				  vsBlobParticle_->GetBufferSize() };
	psoDesc.PS = { psBlobParticle_->GetBufferPointer(),
				  psBlobParticle_->GetBufferSize() };

	// ステート設定 (I. で定義したサブステートを適用)
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;

	// レンダリング設定 (RT/DSVフォーマット、トポロジ)
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプリング設定 (固定値)
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// ---------------------------------------------------------------------
	// III. PSOの生成
	// ---------------------------------------------------------------------

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(
		&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

ComPtr<ID3D12PipelineState> PipelineManager::CreateSkyboxPSO() {
	// InputLayoutはPOSITIONのみ
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
		 D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};


	// ラスタライザステート
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 塗
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK; // 前面カリング(内側を描画)
	rasterizerDesc.DepthClipEnable = TRUE; // 深度クリッピング有効

	// デプスステンシルステート
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE; // 深度テスト有効
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 全ピクセルが同じ深度値になるので、深度書き込みは無効にする
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 比較関数はLESS_EQUALにする(同じ深度値のピクセルも描画するようにする)

	// ブレンドステートは通常の不透明描画と同じで良い
	D3D12_BLEND_DESC blendDesc{};
	// ブレンド無し
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO設定の雛形を作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	// ルートシグネチャを設定
	psoDesc.pRootSignature = rootSignatureSkybox_.Get();
	// シェーダーを設定
	psoDesc.VS = { vsBlobSkybox_->GetBufferPointer(), vsBlobSkybox_->GetBufferSize() };
	psoDesc.PS = { psBlobSkybox_->GetBufferPointer(), psBlobSkybox_->GetBufferSize() };
	// ステートを設定
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;

	// 描画ターゲットとトポロジの設定
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形リスト

	// サンプリング設定
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// PSO生成
	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(
		&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

ComPtr<ID3D12PipelineState> PipelineManager::CreatePostProcessPSO() {
	// 1. InputLayout (頂点レイアウト)
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	// 2. RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// 3. DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// 4. BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// 5. PSO構築
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobPostProcess_->GetBufferPointer(), vsBlobPostProcess_->GetBufferSize() };
	psoDesc.PS = { psBlobPostProcess_->GetBufferPointer(), psBlobPostProcess_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };

	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// グレースケール/セピア フィルター用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateColorFilterPSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobColorFilter_->GetBufferPointer(), psBlobColorFilter_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// ビネット用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateVignettePSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobVignette_->GetBufferPointer(), psBlobVignette_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// 平滑化用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateSmoothingPSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobSmoothing_->GetBufferPointer(), psBlobSmoothing_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// Gaussian Blur用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateGaussianBlurPSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobGaussianBlur_->GetBufferPointer(), psBlobGaussianBlur_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// アウトライン用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateOutlinePSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV, t1=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobOutline_->GetBufferPointer(), psBlobOutline_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// Radial Blur用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateRadialBlurPSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobRadialBlur_->GetBufferPointer(), psBlobRadialBlur_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// Dissolve用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateDissolvePSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV, t1=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobDissolve_->GetBufferPointer(), psBlobDissolve_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// Random用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateRandomPSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobRandom_->GetBufferPointer(), psBlobRandom_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}

// HSV用 PSO
ComPtr<ID3D12PipelineState> PipelineManager::CreateHSVPSO() {
	// RasterizerState (カリングなし)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

	// DepthStencilState (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	// BlendState (不透明)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;

	// PSO構築 (postProcess と同じ RootSignature を使用: b0=CBV, t0=SRV)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignaturePostProcess_.Get();
	psoDesc.VS = { vsBlobColorFilter_->GetBufferPointer(), vsBlobColorFilter_->GetBufferSize() };
	psoDesc.PS = { psBlobHSV_->GetBufferPointer(), psBlobHSV_->GetBufferSize() };
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));

	return pso;
}


ComPtr<ID3D12PipelineState> PipelineManager::CreateObject3dPSO(
	const D3D12_INPUT_ELEMENT_DESC* inputLayout, // Object3dCommonが決定
	uint32_t numElements,
	const D3D12_RASTERIZER_DESC& rasterizerDesc, // Object3dCommonが決定
	const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
	const D3D12_BLEND_DESC& blendDesc) {
	HRESULT hr;

	// PSO設定の雛形を作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	// ルートシグネチャを設定
	psoDesc.pRootSignature = rootSignature3D_.Get();
	// シェーダーを設定
	psoDesc.VS = { vsBlob3D_->GetBufferPointer(), vsBlob3D_->GetBufferSize() };
	psoDesc.PS = { psBlob3D_->GetBufferPointer(), psBlob3D_->GetBufferSize() };
	// 入力レイアウトを設定
	psoDesc.InputLayout = { inputLayout, numElements };
	// その他のステートを設定
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.BlendState = blendDesc;
	// 描画ターゲットとトポロジの設定
	psoDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 3dは三角形リスト
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] =
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // メインレンダーターゲット
	psoDesc.DSVFormat =
		DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度バッファのフォーマット
	// サンプリング設定
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// PSOの生成
	ComPtr<ID3D12PipelineState> pso;
	hr = DX12Context::GetInstance()->GetDevice()->CreateGraphicsPipelineState(&psoDesc,
		IID_PPV_ARGS(&pso));

	return pso;
}

// シェーダーの読み込み
void PipelineManager::LoadShader() {
	// VertexShaderの読み込み
	vsBlobSprite_ = DX12Context::GetInstance()->CompileShader(L"Sprite.VS.hlsl", L"vs_6_0");
	assert(vsBlobSprite_ != nullptr);
	// PixelShaderの読み込み
	psBlobSprite_ = DX12Context::GetInstance()->CompileShader(L"Sprite.PS.hlsl", L"ps_6_0");
	assert(psBlobSprite_ != nullptr);

	// オブジェクト用
	vsBlob3D_ = DX12Context::GetInstance()->CompileShader(L"Object3d.VS.hlsl", L"vs_6_0");
	assert(vsBlob3D_ != nullptr);
	psBlob3D_ = DX12Context::GetInstance()->CompileShader(L"Object3d.PS.hlsl", L"ps_6_0");
	assert(psBlob3D_ != nullptr);

	// パーティクル用
	vsBlobParticle_ = DX12Context::GetInstance()->CompileShader(L"Particle.VS.hlsl", L"vs_6_0");
	assert(vsBlobParticle_ != nullptr);
	psBlobParticle_ = DX12Context::GetInstance()->CompileShader(L"Particle.PS.hlsl", L"ps_6_0");
	assert(psBlobParticle_ != nullptr);

	// スカイボックス用
	vsBlobSkybox_ = DX12Context::GetInstance()->CompileShader(L"Skybox.VS.hlsl", L"vs_6_0");
	assert(vsBlobSkybox_ != nullptr);
	psBlobSkybox_ = DX12Context::GetInstance()->CompileShader(L"Skybox.PS.hlsl", L"ps_6_0");
	assert(psBlobSkybox_ != nullptr);

	// CopyImage (passthrough) 用
	vsBlobPostProcess_ = DX12Context::GetInstance()->CompileShader(L"Fullscreen.VS.hlsl", L"vs_6_0");
	assert(vsBlobPostProcess_ != nullptr);
	psBlobPostProcess_ = DX12Context::GetInstance()->CompileShader(L"CopyImage.PS.hlsl", L"ps_6_0");
	assert(psBlobPostProcess_ != nullptr);

	// グレースケール/セピア フィルター用
	vsBlobColorFilter_ = DX12Context::GetInstance()->CompileShader(L"Fullscreen.VS.hlsl", L"vs_6_0");
	assert(vsBlobColorFilter_ != nullptr);
	psBlobColorFilter_ = DX12Context::GetInstance()->CompileShader(L"Grayscale.PS.hlsl", L"ps_6_0");
	assert(psBlobColorFilter_ != nullptr);

	// ビネット用
	psBlobVignette_ = DX12Context::GetInstance()->CompileShader(L"Vignette.PS.hlsl", L"ps_6_0");
	assert(psBlobVignette_ != nullptr);

	// 平滑化用
	psBlobSmoothing_ = DX12Context::GetInstance()->CompileShader(L"Smoothing.PS.hlsl", L"ps_6_0");
	assert(psBlobSmoothing_ != nullptr);

	// Gaussian Blur用
	psBlobGaussianBlur_ = DX12Context::GetInstance()->CompileShader(L"GaussianBlur.PS.hlsl", L"ps_6_0");
	assert(psBlobGaussianBlur_ != nullptr);

	// Outline用
	psBlobOutline_ = DX12Context::GetInstance()->CompileShader(L"Outline.PS.hlsl", L"ps_6_0");
	assert(psBlobOutline_ != nullptr);

	// Radial Blur用
	psBlobRadialBlur_ = DX12Context::GetInstance()->CompileShader(L"RadialBlur.PS.hlsl", L"ps_6_0");
	assert(psBlobRadialBlur_ != nullptr);

	// Dissolve用
	psBlobDissolve_ = DX12Context::GetInstance()->CompileShader(L"Dissolve.PS.hlsl", L"ps_6_0");
	assert(psBlobDissolve_ != nullptr);

	// Random用
	psBlobRandom_ = DX12Context::GetInstance()->CompileShader(L"Random.PS.hlsl", L"ps_6_0");
	assert(psBlobRandom_ != nullptr);

	// HSV用
	psBlobHSV_ = DX12Context::GetInstance()->CompileShader(L"HSV.PS.hlsl", L"ps_6_0");
	assert(psBlobHSV_ != nullptr);
}

// ルートシグネチャの作成
void PipelineManager::CreateRootSignature() {
	HRESULT hr;

#pragma region 共通のSampler設定
	// 共通のSamplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[3] = {};
	
	// s0: WRAPサンプラー
	staticSamplers[0].Filter =
		D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;    // PixelShaderで使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う

	// s1: CLAMPサンプラー
	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // 0~1の範囲外をクランプ
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	staticSamplers[1].ShaderRegister = 1; // レジスタ番号1を使う

	// s2: POINT_CLAMPサンプラー
	staticSamplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // ポイントフィルタ
	staticSamplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[2].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[2].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	staticSamplers[2].ShaderRegister = 2; // レジスタ番号2を使う

#pragma endregion 共通のSampler設定ここまで

#pragma region Sprite用のRootSignature作成
	// Sprite用のRootSignature作成

	// Texture用 SRV (Pixel Shader用)
	D3D12_DESCRIPTOR_RANGE spriteTextureSrvRange[1] = {};
	spriteTextureSrvRange[0].BaseShaderRegister = 0; // t0
	spriteTextureSrvRange[0].NumDescriptors = 1;
	spriteTextureSrvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	spriteTextureSrvRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成
	D3D12_ROOT_PARAMETER spriteRootParameters[4] = {};


	// Root Parameter 0: Pixel Shader用 Material CBV (b0)
	spriteRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	spriteRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	spriteRootParameters[0].Descriptor.ShaderRegister = 0; // b0

	// Root Parameter 1: Vertex Shader用 Transform CBV (b0)
	spriteRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	spriteRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	spriteRootParameters[1].Descriptor.ShaderRegister = 1; // b1

	// Root Parameter 2: Pixel Shader用 Texture SRV (t0)
	spriteRootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	spriteRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	spriteRootParameters[2].DescriptorTable.pDescriptorRanges =
		spriteTextureSrvRange;
	spriteRootParameters[2].DescriptorTable.NumDescriptorRanges =
		_countof(spriteTextureSrvRange);

	// Root Parameter 3: Pixel Shader用 Dissolve MaskTexture SRV (t1)
	D3D12_DESCRIPTOR_RANGE spriteMaskTextureSrvRange[1] = {};
	spriteMaskTextureSrvRange[0].BaseShaderRegister = 1; // t1
	spriteMaskTextureSrvRange[0].NumDescriptors = 1;
	spriteMaskTextureSrvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	spriteMaskTextureSrvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	spriteRootParameters[3].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	spriteRootParameters[3].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;
	spriteRootParameters[3].DescriptorTable.pDescriptorRanges =
		spriteMaskTextureSrvRange;
	spriteRootParameters[3].DescriptorTable.NumDescriptorRanges =
		_countof(spriteMaskTextureSrvRange);

	// RootSignatureDesc
	D3D12_ROOT_SIGNATURE_DESC spriteRootSignatureDesc{};
	spriteRootSignatureDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	spriteRootSignatureDesc.pParameters = spriteRootParameters;
	spriteRootSignatureDesc.NumParameters = _countof(spriteRootParameters);
	spriteRootSignatureDesc.pStaticSamplers = staticSamplers; // 共通Samplerを使用
	spriteRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズと生成
	ComPtr<ID3DBlob> spriteSignatureBlob = nullptr;
	ComPtr<ID3DBlob> spriteErrorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&spriteRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&spriteSignatureBlob, &spriteErrorBlob);
	// エラーチェックと生成処理...
	if (FAILED(hr)) {
		Logger::Log("ERROR: " + std::string(reinterpret_cast<char*>(spriteErrorBlob->GetBufferPointer())));
		assert(false);
	}
	hr = DX12Context::GetInstance()->GetDevice()->CreateRootSignature(
		0, spriteSignatureBlob->GetBufferPointer(),
		spriteSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatureSprite_));
	assert(SUCCEEDED(hr));

#pragma endregion Sprite用のRootSignature作成ここまで

#pragma region Object3d用のRootSignature作成
	// Object3d用のRootSignature作成

	// Texture用 SRV (Pixel Shader用)
	D3D12_DESCRIPTOR_RANGE object3dTextureSrvRange[1] = {};
	object3dTextureSrvRange[0].BaseShaderRegister = 0; // t0
	object3dTextureSrvRange[0].NumDescriptors = 1;     // 数は1つ
	object3dTextureSrvRange[0].RangeType =
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	object3dTextureSrvRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// EnvironmentMap用 SRV (Pixel Shader用)
	D3D12_DESCRIPTOR_RANGE object3dEnvMapSrvRange[1] = {};
	object3dEnvMapSrvRange[0].BaseShaderRegister = 1; // t1
	object3dEnvMapSrvRange[0].NumDescriptors = 1;     // 数は1つ
	object3dEnvMapSrvRange[0].RangeType =
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	object3dEnvMapSrvRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成。PixelShaderのMaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER object3dRootParameters[9] = {};

	// Root Parameter 0: Pixel Shader用 Material CBV (b0)
	object3dRootParameters[0].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う(b0のbと一致する)
	object3dRootParameters[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	object3dRootParameters[0].Descriptor.ShaderRegister =
		0; // レジスタ番号0とバインド(b0の0と一致する。もしb11と紐づけたいなら11となる)

	// Root Parameter 1: Vertex Shader用 Transform CBV (b0)
	object3dRootParameters[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う(b0のbと一致する)
	object3dRootParameters[1].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_VERTEX;                      // VertexShaderで使う
	object3dRootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0

	// Root Parameter 2: Pixel Shader用 Texture SRV (t0)
	object3dRootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	object3dRootParameters[2].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	object3dRootParameters[2].DescriptorTable.pDescriptorRanges =
		object3dTextureSrvRange; // Tableの中身の配列を指定
	object3dRootParameters[2].DescriptorTable.NumDescriptorRanges =
		_countof(object3dTextureSrvRange); // Tableで利用する数

	// Root Parameter 3: Pixel Shader用 DirectionalLight CBV (b1)
	object3dRootParameters[3].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	object3dRootParameters[3].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	object3dRootParameters[3].Descriptor.ShaderRegister =
		1; // レジスタ番号1を使う

	// Root Parameter 4: Pixel Shader用 Camera CBV (b2)
	object3dRootParameters[4].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	object3dRootParameters[4].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	object3dRootParameters[4].Descriptor.ShaderRegister =
		2; // レジスタ番号2を使う

	// Root Parameter 5: Pixel Shader用 PointLight CBV (b3)
	object3dRootParameters[5].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	object3dRootParameters[5].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	object3dRootParameters[5].Descriptor.ShaderRegister =
		3; // レジスタ番号3を使う (b3)

	// Root Parameter 6: Pixel Shader用 SpotLight CBV (b4)
	object3dRootParameters[6].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	object3dRootParameters[6].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	object3dRootParameters[6].Descriptor.ShaderRegister =
		4; // レジスタ番号4を使う (b4)

	// Root Parameter 7: Pixel Shader用 EnvironmentMap SRV (t1)
	object3dRootParameters[7].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	object3dRootParameters[7].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;
	object3dRootParameters[7].DescriptorTable.pDescriptorRanges =
		object3dEnvMapSrvRange;
	object3dRootParameters[7].DescriptorTable.NumDescriptorRanges =
		_countof(object3dEnvMapSrvRange);

	// Root Parameter 8: Pixel Shader用 Dissolve MaskTexture SRV (t2)
	D3D12_DESCRIPTOR_RANGE object3dMaskTextureSrvRange[1] = {};
	object3dMaskTextureSrvRange[0].BaseShaderRegister = 2; // t2
	object3dMaskTextureSrvRange[0].NumDescriptors = 1;
	object3dMaskTextureSrvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	object3dMaskTextureSrvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	object3dRootParameters[8].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	object3dRootParameters[8].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;
	object3dRootParameters[8].DescriptorTable.pDescriptorRanges =
		object3dMaskTextureSrvRange;
	object3dRootParameters[8].DescriptorTable.NumDescriptorRanges =
		_countof(object3dMaskTextureSrvRange);

	// object用のRootSignatureDesc
	D3D12_ROOT_SIGNATURE_DESC object3dRootSignatureDesc{};
	object3dRootSignatureDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	object3dRootSignatureDesc.pParameters =
		object3dRootParameters; // ルートパラメータ配列へのポインタ
	object3dRootSignatureDesc.NumParameters =
		_countof(object3dRootParameters); // 配列の長さ
	object3dRootSignatureDesc.pStaticSamplers = staticSamplers;
	object3dRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	ComPtr<ID3DBlob> object3dSignatureBlob = nullptr;
	ComPtr<ID3DBlob> object3dErrorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&object3dRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&object3dSignatureBlob, &object3dErrorBlob);
	if (FAILED(hr)) {
		Logger::Log("ERROR: " + std::string(reinterpret_cast<char*>(object3dErrorBlob->GetBufferPointer())));
		assert(false);
	}
	// バイナリを元に生成
	hr = DX12Context::GetInstance()->GetDevice()->CreateRootSignature(
		0, object3dSignatureBlob->GetBufferPointer(),
		object3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature3D_));
	assert(SUCCEEDED(hr));

#pragma endregion Object3d用のRootSignature作成ここまで

#pragma region Particle用のRootSignature作成
	// Particle用のRootSignature作成

	// Particle用 SRV (Vertex Shader用)
	D3D12_DESCRIPTOR_RANGE particleInstancingSrvRange[1] = {};
	particleInstancingSrvRange[0].BaseShaderRegister = 0; // t0
	particleInstancingSrvRange[0].NumDescriptors = 1;     // 数は1つ
	particleInstancingSrvRange[0].RangeType =
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	particleInstancingSrvRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// テクスチャ用 SRV (Pixel Shader用)
	D3D12_DESCRIPTOR_RANGE particleTextureSrvRange[1] = {};
	particleTextureSrvRange[0].BaseShaderRegister = 0; // t0 (Texture)
	particleTextureSrvRange[0].NumDescriptors = 1;     // 数は1つ
	particleTextureSrvRange[0].RangeType =
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	particleTextureSrvRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成。PixelShaderのMaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER particleRootParameters[3] = {};

	// Root Parameter 0: Pixel Shader用 Material CBV (b0)
	particleRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	particleRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[0].Descriptor.ShaderRegister = 0; // b0

	// Root Parameter 1: Vertex Shader用 Instancing SRV (t0)
	particleRootParameters[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	particleRootParameters[1].DescriptorTable.pDescriptorRanges =
		particleInstancingSrvRange;
	particleRootParameters[1].DescriptorTable.NumDescriptorRanges =
		_countof(particleInstancingSrvRange);

	// Root Parameter 2: Pixel Shader用 Texture SRV (t0)
	particleRootParameters[2].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	particleRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	particleRootParameters[2].DescriptorTable.pDescriptorRanges =
		particleTextureSrvRange;
	particleRootParameters[2].DescriptorTable.NumDescriptorRanges =
		_countof(particleTextureSrvRange);

	// Particle用のRootSignatureDesc
	D3D12_ROOT_SIGNATURE_DESC particleRootSignatureDesc{};
	particleRootSignatureDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	particleRootSignatureDesc.pParameters =
		particleRootParameters; // ルートパラメータ配列へのポインタ
	particleRootSignatureDesc.NumParameters =
		_countof(particleRootParameters); // 配列の長さ (3)
	particleRootSignatureDesc.pStaticSamplers = staticSamplers;
	particleRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	ComPtr<ID3DBlob> particleSignatureBlob = nullptr;
	ComPtr<ID3DBlob> particleErrorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&particleRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&particleSignatureBlob, &particleErrorBlob);
	if (FAILED(hr)) {
		Logger::Log("ERROR: " + std::string(reinterpret_cast<char*>(particleErrorBlob->GetBufferPointer())));
		assert(false);
	}
	// バイナリを元に生成
	hr = DX12Context::GetInstance()->GetDevice()->CreateRootSignature(
		0, particleSignatureBlob->GetBufferPointer(),
		particleSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatureParticle_));
	assert(SUCCEEDED(hr));

#pragma endregion Particle用のRootSignature作成ここまで

#pragma region Skybox用のRootSignature作成
	// Skybox用のRootSignature作成

	// Texture用 SRV (Pixel Shader用)
	D3D12_DESCRIPTOR_RANGE skyboxTextureSrvRange[1] = {};
	skyboxTextureSrvRange[0].BaseShaderRegister = 0; // t0
	skyboxTextureSrvRange[0].NumDescriptors = 1;     // 数は1つ
	skyboxTextureSrvRange[0].RangeType =
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	skyboxTextureSrvRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成
	D3D12_ROOT_PARAMETER skyboxRootParameters[2] = {};

	// Root Parameter 0: Pixel Shader用 Material CBV (b0)
	skyboxRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	skyboxRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	skyboxRootParameters[0].Descriptor.ShaderRegister = 0; // b0

	// Root Parameter 1: Pixel Shader用 Texture SRV (t0)
	skyboxRootParameters[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	skyboxRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	skyboxRootParameters[1].DescriptorTable.pDescriptorRanges =
		skyboxTextureSrvRange;
	skyboxRootParameters[1].DescriptorTable.NumDescriptorRanges =
		_countof(skyboxTextureSrvRange);

	// Skybox用のRootSignatureDesc
	D3D12_ROOT_SIGNATURE_DESC skyboxRootSignatureDesc{};
	skyboxRootSignatureDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	skyboxRootSignatureDesc.pParameters =
		skyboxRootParameters; // ルートパラメータ配列へのポインタ
	skyboxRootSignatureDesc.NumParameters =
		_countof(skyboxRootParameters); // 配列の長さ
	skyboxRootSignatureDesc.pStaticSamplers = staticSamplers;
	skyboxRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	ComPtr<ID3DBlob> skyboxSignatureBlob = nullptr;
	ComPtr<ID3DBlob> skyboxErrorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&skyboxRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&skyboxSignatureBlob, &skyboxErrorBlob);
	if (FAILED(hr)) {
		Logger::Log("ERROR: " + std::string(reinterpret_cast<char*>(skyboxErrorBlob->GetBufferPointer())));
		assert(false);
	}

	// バイナリを元に生成
	hr = DX12Context::GetInstance()->GetDevice()->CreateRootSignature(
		0, skyboxSignatureBlob->GetBufferPointer(),
		skyboxSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignatureSkybox_));
	assert(SUCCEEDED(hr));

#pragma endregion Skybox用のRootSignature作成ここまで

#pragma region PostProcess用のRootSignature作成
	// DescriptorRange (t0)
	D3D12_DESCRIPTOR_RANGE postProcessTextureRange[1] = {};
	postProcessTextureRange[0].BaseShaderRegister = 0; // t0
	postProcessTextureRange[0].NumDescriptors = 1;
	postProcessTextureRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	postProcessTextureRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// DescriptorRange (t1: Depth)
	D3D12_DESCRIPTOR_RANGE postProcessDepthRange[1] = {};
	postProcessDepthRange[0].BaseShaderRegister = 1; // t1
	postProcessDepthRange[0].NumDescriptors = 1;
	postProcessDepthRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	postProcessDepthRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter
	D3D12_ROOT_PARAMETER postProcessRootParameters[3] = {};
	// b0: Parameter
	postProcessRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	postProcessRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	postProcessRootParameters[0].Descriptor.ShaderRegister = 0;
	// t0: Texture (Color)
	postProcessRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	postProcessRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	postProcessRootParameters[1].DescriptorTable.pDescriptorRanges = postProcessTextureRange;
	postProcessRootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(postProcessTextureRange);
	// t1: Texture (Depth)
	postProcessRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	postProcessRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	postProcessRootParameters[2].DescriptorTable.pDescriptorRanges = postProcessDepthRange;
	postProcessRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(postProcessDepthRange);

	// RootSignatureDesc
	D3D12_ROOT_SIGNATURE_DESC postProcessRootSignatureDesc{};
	postProcessRootSignatureDesc.pParameters = postProcessRootParameters;
	postProcessRootSignatureDesc.NumParameters = _countof(postProcessRootParameters);
	postProcessRootSignatureDesc.pStaticSamplers = staticSamplers;
	postProcessRootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);
	postProcessRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;


	ComPtr<ID3DBlob> postProcessSignatureBlob = nullptr;
	ComPtr<ID3DBlob> postProcessErrorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&postProcessRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &postProcessSignatureBlob, &postProcessErrorBlob);
	assert(SUCCEEDED(hr));
	hr = DX12Context::GetInstance()->GetDevice()->CreateRootSignature(0, postProcessSignatureBlob->GetBufferPointer(), postProcessSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignaturePostProcess_));
	assert(SUCCEEDED(hr));
#pragma endregion
}

// Compute PSOを生成して返す関数
ComPtr<ID3D12PipelineState> PipelineManager::CreateComputePSO(
	ID3D12RootSignature* rootSignature,
	IDxcBlob* computeShaderBlob) {
	assert(rootSignature != nullptr);
	assert(computeShaderBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};
	computePsoDesc.pRootSignature = rootSignature;
	computePsoDesc.CS = {
		computeShaderBlob->GetBufferPointer(),
		computeShaderBlob->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ComPtr<ID3D12PipelineState> pso = nullptr;
	HRESULT hr = DX12Context::GetInstance()->GetDevice()->CreateComputePipelineState(
		&computePsoDesc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));
	return pso;
}

// Compute用RootSignatureを生成して返す関数
ComPtr<ID3D12RootSignature> PipelineManager::CreateComputeRootSignature(
	const D3D12_ROOT_SIGNATURE_DESC& desc) {
	ComPtr<ID3DBlob> signatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		}
		assert(false && "Failed to serialize compute root signature");
	}

	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = DX12Context::GetInstance()->GetDevice()->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
	return rootSignature;
}