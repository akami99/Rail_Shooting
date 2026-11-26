#pragma once // ヘッダーの多重インクルード防止

#include "Sound.h"      // Sound クラスの定義をインクルード
#include <atomic>       // スレッドセーフなフラグのために必要
#include <map>          // std::map のために必要
#include <mutex>        // スレッドセーフな操作のために必要
#include <string>       // std::string のために必要
#include <list>       // 再生中のボイスを管理するために使用(vectorの代わりに追加)
#include <wrl/client.h> // Microsoft::WRL::ComPtr のために必要
#include <memory>       // std::unique_ptr
#include <xaudio2.h>    // XAudio2 のインターフェースと型のために必要

// XAudio2 ライブラリをリンク
#pragma comment(lib, "xaudio2.lib")

// AudioManager クラス
class AudioManager {
public:
    // 【Passkey Idiom】
    struct Token {
    private:
        friend class AudioManager;
        Token() {}
    };
private:

  std::mutex playingVoicesMutex; // playingVoicesリストを保護するミューテックス
  Microsoft::WRL::ComPtr<IXAudio2> xAudio2; // XAudio2 エンジンオブジェクト
  IXAudio2MasteringVoice *masterVoice;      // マスターボイス
  std::map<std::string, Sound> sounds; // ロードされたサウンドを管理するマップ

  // 再生中のSourceVoiceとそのコールバックを管理する構造体
  struct PlayingVoice {
    IXAudio2SourceVoice *pVoice;
    IXAudio2VoiceCallback *pCallback; // コールバックオブジェクトも管理
    std::atomic<bool> isFinished;     // 再生終了フラグ

    // コンストラクタ
    PlayingVoice() : pVoice(nullptr), pCallback(nullptr), isFinished(false) {}

    // デストラクタでボイスとコールバックを解放
    ~PlayingVoice();

    // Move semantics for list
    PlayingVoice(PlayingVoice &&other) noexcept;
    PlayingVoice &operator=(PlayingVoice &&other) noexcept;

    // コピーコンストラクタとコピー代入演算子を禁止
    PlayingVoice(const PlayingVoice &) = delete;
    PlayingVoice &operator=(const PlayingVoice &) = delete;
  };
  std::list<PlayingVoice>
      playingVoices; // listに変更（これにより要素のアドレスが固定される）

  // コールバッククラスの定義（AudioManagerの内部クラスとして定義）
  class InternalVoiceCallback : public IXAudio2VoiceCallback {
  private:
    std::atomic<bool> *finishedFlag_; // 対応するPlayingVoiceの終了フラグ

  public:
    // コンストラクタ
    InternalVoiceCallback(std::atomic<bool> *flag) : finishedFlag_(flag) {}

    // 軽量なフラグ設定のみ
    void STDMETHODCALLTYPE OnBufferEnd(void *pBufferContext) override {
      if (finishedFlag_) {
        finishedFlag_->store(true);
      }
    }

    // 他のコールバックメソッドは空実装
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE
    OnVoiceProcessingPassStart(UINT32 BytesRequired) override {}
    void STDMETHODCALLTYPE OnBufferStart(void *pBufferContext) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void *pBufferContext) override {}
    void STDMETHODCALLTYPE OnVoiceError(void *pBufferContext,
                                        HRESULT Error) override {}
  };

public:
  // 追加: シングルトンインスタンス取得
    static AudioManager* GetInstance();

    // インスタンス破棄
    static void Destroy();

  // コンストラクタ(隠蔽)
  explicit AudioManager(Token);

  // XAudio2 エンジンを初期化する
  bool Initialize();

  void Finalize();

  // XAudio2 エンジンをシャットダウンする
  void Shutdown();

  // サウンドをロードし、名前を付けて管理する
  // @param name サウンドを識別するためのユニークな名前
  // @param filePath ロードするWAVファイルのパス
  // @return ロードが成功した場合は true、失敗した場合は false
  bool LoadSound(const std::string &name, const std::string &filePath);

  // ロードしたサウンドを名前で取得する
  // @param name 取得したいサウンドの名前
  // @return 見つかった場合は Sound
  // オブジェクトへのconstポインタ、見つからなかった場合は nullptr
  const Sound *GetSound(const std::string &name) const;

  // 名前を指定してサウンドを再生する
  // @param name 再生したいサウンドの名前
  void PlaySound(const std::string &name);

  // 再生終了したボイスを手動でクリーンアップ
  void CleanupFinishedVoices();

  private:
      // デストラクタ
      ~AudioManager();
      // コピー禁止
      AudioManager(const AudioManager&) = delete;
      const AudioManager& operator=(const AudioManager&) = delete;

      static std::unique_ptr<AudioManager> instance_;
      friend std::default_delete<AudioManager>;
};