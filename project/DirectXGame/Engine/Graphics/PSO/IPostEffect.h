#pragma once

#include <d3d12.h>

/// @brief ポストエフェクトの基底インターフェース
class IPostEffect {
public:
    virtual ~IPostEffect() = default;

    /// <summary>
    /// PSO・定数バッファを初期化する
    /// </summary>
    virtual void Initialize() = 0;

    /// <summary>
    /// 毎フレームの状態更新（時間経過など）
    /// </summary>
    /// <param name="deltaTime">経過時間</param>
    virtual void Update(float deltaTime) { (void)deltaTime; }

    /// <summary>
    /// パイプラインステート・定数バッファ・固有リソースをコマンドリストへ設定する
    /// </summary>
    /// <param name="commandList">描画コマンドリスト</param>
    virtual void Apply(ID3D12GraphicsCommandList* commandList) = 0;

    /// <summary>
    /// 描画時に深度バッファの SRV 遷移が必要かどうか
    /// </summary>
    virtual bool NeedsDepthResource() const { return false; }
};
