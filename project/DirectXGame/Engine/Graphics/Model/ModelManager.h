#pragma once

#include <map>
#include <string>
#include <memory>

// 前方宣言
class Model;

// モデルマネージャー
class ModelManager {
public:
  // 【Passkey Idiom】
  struct Token {
  private:
    friend class ModelManager;
    Token() {}
  };

private: // namespace省略のためのusing宣言
  static std::unique_ptr<ModelManager> instance_;

  // モデルデータ
  std::map<std::string, std::unique_ptr<Model>> models_; // キーの順番を保つ

  public: // シングルトンインスタンス取得
    static ModelManager* GetInstance();
    static void Destroy();

public: // メンバ関数
  // コンストラクタ(隠蔽)
  explicit ModelManager(Token);
  // 初期化
  void Initialize();

  // 終了
  void Finalize();

  /// <summary>
  /// モデルの検索
  /// </summary>
  /// <param name="filePath">モデルファイルのパス</param>
  /// <returns>モデル</returns>
  Model *FindModel(const std::string &filePath);

  /// <summary>
  /// モデルファイルの読み込み()
  /// </summary>
  /// <param name="directoryPath">モデルファイルのディレクトリパス</param>
  /// <param name="filePath">モデルファイルのパス</param>
  void LoadModel(const std::string &directoryPath, const std::string &filePath);

  /// <summary>
  /// Ringプリミティブの生成
  /// </summary>
  /// <param name="name">リングの名前</param>
  /// <param name="textureFilePath">テクスチャファイルのパス</param>
  /// <param name="innerRadius">内半径</param>
  /// <param name="outerRadius">外半径</param>
  /// <param name="division">分割数</param>
  void CreateRing(const std::string& name, const std::string& textureFilePath, float innerRadius = 0.5f, float outerRadius = 1.0f, uint32_t division = 32);

private: // メンバ関数
  // デストラクタ(隠蔽)
  ~ModelManager() = default;
  // コピーコンストラクタの封印
  ModelManager(const ModelManager &) = delete;
  // コピー代入演算子の封印
  ModelManager &operator=(const ModelManager &) = delete;

  friend std::default_delete<ModelManager>;
};
