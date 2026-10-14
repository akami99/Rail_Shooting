#include "RAFramework.h"
// 必要なエンジンのインクルード
#include "AudioManager.h"
#include "DX12Context.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "LightManager.h"
#include "Logger.h"
#include "ModelManager.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "PipelineManager.h"
#include "SceneManager.h"
#include "SpriteCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "Win32Window.h"

// dump用
#include <dbghelp.h>
#include <strsafe.h>

#pragma comment(lib, "Dbghelp.lib")

// ダンプ出力関数（static関数としてこのファイル内だけで使う）
static LONG WINAPI ExportDump(EXCEPTION_POINTERS *exception) {
  // ログと同じく、exeと同じ階層に「dumps」フォルダを作成する
  const wchar_t *dumpDir = L"dumps";

  CreateDirectoryW(dumpDir, nullptr);

  // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリいかに出力
  SYSTEMTIME time;
  GetLocalTime(&time);
  wchar_t filePath[MAX_PATH] = {0};

  StringCchPrintfW(filePath, MAX_PATH, L"%s/%04d-%02d%02d-%02d%02d%02d.dmp",
                   dumpDir, time.wYear, time.wMonth, time.wDay, time.wHour,
                   time.wMinute, time.wSecond);

  HANDLE dumpFileHandle =
      CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
                 FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

  // 4. ハンドルの有効性チェック
  if (dumpFileHandle == INVALID_HANDLE_VALUE) {
    // ファイル作成に失敗した場合（権限不足など）、ダンプ処理はスキップして終了
    // エラーコードをログに記録できれば理想だが、ここではシンプルな終了とする
    return EXCEPTION_EXECUTE_HANDLER;
  }

  // processId（このexeのID）とクラッシュ（例外）の発生したthreadIdを取得
  DWORD processId = GetCurrentProcessId();
  DWORD threadId = GetCurrentThreadId();
  // 設定情報を入力
  MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
  minidumpInformation.ThreadId = threadId;
  minidumpInformation.ExceptionPointers = exception;
  minidumpInformation.ClientPointers = TRUE;
  // Dumpを出力、MiniDumpNormalは最低限の情報を出力するフラグ
  MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle,
                    MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

  // ファイルハンドルを閉じる
  CloseHandle(dumpFileHandle);

  // 他に関連付けられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
  return EXCEPTION_EXECUTE_HANDLER;
}

void RAFramework::Run() {
  Initialize();

  // ウィンドウのxボタンが押されるまでループ
  while (true) {
    // メッセージ処理
    if (Win32Window::GetInstance()->ProcessMessage() || IsEndRequest()) {
      break;
    }

    // 更新
    Update();

    // 描画
    Draw();

    if (Input::GetInstance()->IsKeyTriggered(DIK_F4)) {
      break;
    }
  }

  Finalize();
}

void RAFramework::Initialize() {
  // ログ初期化
  Logger::InitializeFileLogging();

  /// システム基盤の初期化
  // Window
  Win32Window::GetInstance()->Initialize();
  // DirectXBase
  DX12Context::GetInstance()->Initialize();
  // SRVManager
  SrvManager::GetInstance()->Initialize();
  // DirectInput
  Input::GetInstance()->Initialize();
  // PipelineManager
  PipelineManager::GetInstance()->Initialize();
  // ImGuiManager
  imGuiManager_ = std::make_unique<ImGuiManager>();
  imGuiManager_->Initialize();

  // リソース系マネージャのためにコマンドリストを開く
  // (Texture, Model, ParticleなどはGPUにデータを送るためリストが必要)
  DX12Context::GetInstance()->GetCommandList()->Reset(
      DX12Context::GetInstance()->GetCommandAllocator(), nullptr);

  /// マネージャー類の初期化（シングルトン）
  LightManager::GetInstance()->Initialize();
  if (!AudioManager::GetInstance()->Initialize()) {
    // エラー処理 (ログ出力など)
    assert(false && "AudioManager Initialize Failed");
  }
  TextureManager::GetInstance()->Initialize();
  ModelManager::GetInstance()->Initialize();
  ParticleManager::GetInstance()->Initialize();

  /// 共通描画設定の生成
  // SpriteCommon
  SpriteCommon::GetInstance()->Initialize();
  // Object3dCommon
  Object3dCommon::GetInstance()->Initialize();

  // 初期化コマンドを一気に実行して完了を待つ
  // これにより、すべてのマネージャが準備完了状態でゲームが始まる
  DX12Context::GetInstance()->ExecuteInitialCommandAndSync();
}

void RAFramework::Finalize() {
  // シーンを消す前に、GPUが現在描画しているフレームの処理を完全に終わらせる
  DX12Context::GetInstance()->WaitForGpu();

  // シーンマネージャに現在のシーンを終了させる
  SceneManager::GetInstance()->Finalize();
  SceneManager::GetInstance()->Destroy();

  //// マネージャーの終了
  Object3dCommon::GetInstance()->Finalize();
  SpriteCommon::GetInstance()->Finalize();
  ParticleManager::GetInstance()->Finalize();
  ModelManager::GetInstance()->Finalize();
  TextureManager::GetInstance()->Finalize();
  AudioManager::GetInstance()->Shutdown();
  AudioManager::GetInstance()->Finalize();
  LightManager::GetInstance()->Finalize();

  imGuiManager_->Finalize();
  imGuiManager_.reset();

  PipelineManager::GetInstance()->Destroy();
  Input::GetInstance()->Destroy();
  SrvManager::GetInstance()->Finalize();
  DX12Context::GetInstance()->Finalize();
  DX12Context::GetInstance()->Destroy();

  Win32Window::GetInstance()->Destroy();
}
