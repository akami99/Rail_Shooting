#include "../Audio/AudioManager.h" // AudioManager.h をインクルード
#include <cassert>                 // アサートのために必要

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

std::unique_ptr<AudioManager> AudioManager::instance_ = nullptr;

// シングルトンインスタンスの取得実装
AudioManager* AudioManager::GetInstance() {
    if (instance_ == nullptr) {
        instance_ = std::make_unique<AudioManager>(Token{});
    }
    return instance_.get();
}

void AudioManager::Destroy() {
    instance_.reset();
}

// PlayingVoice 構造体のデストラクタの実装
AudioManager::PlayingVoice::~PlayingVoice() {
  if (pVoice) {
    pVoice->DestroyVoice(); // ボイスを破棄
    pVoice = nullptr;
  }
  if (pCallback) {
    delete pCallback; // コールバックオブジェクトを解放
    pCallback = nullptr;
  }
}

// PlayingVoice 構造体のムーブコンストラクタの実装
AudioManager::PlayingVoice::PlayingVoice(PlayingVoice &&other) noexcept
    : pVoice(other.pVoice), pCallback(other.pCallback),
      isFinished(other.isFinished.load()) {
  other.pVoice = nullptr;
  other.pCallback = nullptr;
  other.isFinished = false;
}

// PlayingVoice 構造体のムーブ代入演算子の実装
AudioManager::PlayingVoice &
AudioManager::PlayingVoice::operator=(PlayingVoice &&other) noexcept {
  if (this != &other) {
    // 既存のリソースを解放
    if (pVoice)
      pVoice->DestroyVoice();
    if (pCallback)
      delete pCallback;

    // other のリソースをこのオブジェクトに移動
    pVoice = other.pVoice;
    pCallback = other.pCallback;
    isFinished = other.isFinished.load();

    // other のリソースを無効化
    other.pVoice = nullptr;
    other.pCallback = nullptr;
    other.isFinished = false;
  }
  return *this;
}

// AudioManager のコンストラクタの実装
AudioManager::AudioManager(Token) : masterVoice(nullptr) {
  // ComPtr は自動的に nullptr で初期化される
}

// AudioManager のデストラクタの実装
AudioManager::~AudioManager() {
  Shutdown(); // シャットダウン処理を呼び出す
}

// XAudio2 エンジンを初期化する
bool AudioManager::Initialize() {
  HRESULT hr;

  // XAudio2 オブジェクトの生成
  hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
  if (FAILED(hr)) {
    return false;
  }

  // マスターボイスの生成
  hr = xAudio2->CreateMasteringVoice(&masterVoice);
  if (FAILED(hr)) {
    Shutdown(); // 失敗したらXAudio2も解放
    return false;
  }

  // Media Foundation の初期化
  hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
  if (FAILED(hr))
    return false;

  return true;
}

void AudioManager::Finalize() {
    // インスタンスの解放
    instance_.reset();
}

// XAudio2 エンジンをシャットダウンする
void AudioManager::Shutdown() {
  // ミューテックスでロック
  std::lock_guard<std::mutex> lock(playingVoicesMutex);

  // すべての再生中ボイスを停止・解放
  // playingVoices リストの各要素のデストラクタが pVoice と pCallback
  // を解放します。 そのため、ここではリストをクリアするだけで十分
  playingVoices.clear();

  // マスターボイスの解放
  if (masterVoice) {
    masterVoice->DestroyVoice();
    masterVoice = nullptr;
  }
  // IXAudio2 オブジェクトの解放 (ComPtr が自動で行う)
  xAudio2.Reset();

  // ロードされたすべてのサウンドデータを解放
  for (auto &pair : sounds) {
    pair.second.Unload();
  }
  sounds.clear();

  // Media Foundation の終了
  MFShutdown();
}

// サウンドをロードし、名前を付けて管理する
bool AudioManager::LoadSound(const std::string &name,
                             const std::string &filePath) {
  // 既に同じ名前のサウンドがロードされているかチェック
  if (sounds.count(name)) {
    return false;
  }

  Sound newSound;
  if (newSound.Load(filePath)) {
    // ムーブセマンティクスで追加
    sounds.emplace(name, std::move(newSound));
    return true;
  }
  return false;
}

// ロードしたサウンドを名前で取得する
const Sound *AudioManager::GetSound(const std::string &name) const {
  auto it = sounds.find(name);
  return (it != sounds.end()) ? &(it->second) : nullptr;
}

// 名前を指定してサウンドを再生する
void AudioManager::PlaySound(const std::string &name) {
  const Sound *sound = GetSound(name);
  if (!sound) {
    return;
  }

  // ミューテックスでロック
  std::lock_guard<std::mutex> lock(playingVoicesMutex);

  IXAudio2SourceVoice *pSourceVoice = nullptr;

  // 新しいPlayingVoiceを作成
  // 1. 先に要素を追加して、確定したアドレスを取得する
  playingVoices.emplace_back();
  PlayingVoice &newVoice = playingVoices.back();

  // 2. 確定した newVoice.isFinished のアドレスを渡す
  newVoice.pCallback = new InternalVoiceCallback(&newVoice.isFinished);

  // 3. ボイス作成 (キャストを使用)
  HRESULT hr = xAudio2->CreateSourceVoice(
      &newVoice.pVoice,
      reinterpret_cast<const WAVEFORMATEX *>(&sound->GetWaveFormatEx()), 0,
      XAUDIO2_DEFAULT_FREQ_RATIO, newVoice.pCallback);

  if (FAILED(hr)) {
    playingVoices.pop_back();
    return;
  }

  // バッファを送信
  XAUDIO2_BUFFER buf{};
  buf.pAudioData = sound->GetBuffer();
  buf.AudioBytes = sound->GetBufferSize();
  buf.Flags = XAUDIO2_END_OF_STREAM;

  hr = newVoice.pVoice->SubmitSourceBuffer(&buf);
  if (FAILED(hr)) {
    playingVoices.pop_back();
    return;
  }

  hr = newVoice.pVoice->Start(0);
  if (FAILED(hr)) {
    playingVoices.pop_back();
    return;
  }
}

// 終了したボイスのクリーンアップ
void AudioManager::CleanupFinishedVoices() {
  // ミューテックスでリスト操作を保護
  std::lock_guard<std::mutex> lock(playingVoicesMutex);

  // 終了フラグが立っているボイスを削除
  // std::list の remove_if は、条件に一致する要素を削除し、
  // 適切にデストラクタ（PlayingVoice::~PlayingVoice）を呼び出す。
  // これにより、XAudio2のボイス破棄とメモリ解放が安全に行われる。
  playingVoices.remove_if(
      [](const PlayingVoice &voice) { return voice.isFinished.load(); });
}