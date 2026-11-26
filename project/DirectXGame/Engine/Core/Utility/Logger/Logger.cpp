#include "../Logger/Logger.h"

#include <chrono>
#include <dxgidebug.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

/// <summary>
/// 古いログファイルをリネームして整理する
/// </summary>
void Logger::RotateLogFiles() {
	// 古いバックアップから順に後ろへずらす
	// backup_1.log -> backup_2.log
	for (int i = kMaxBackups; i > 1; --i) {
		std::string oldPath = std::string(kLogDir) + "/backup_" + std::to_string(i - 1) + ".log";
		std::string newPath = std::string(kLogDir) + "/backup_" + std::to_string(i) + ".log";

		if (fs::exists(oldPath)) {
			// 既存の backup_2 があれば削除して置換
			if (fs::exists(newPath)) fs::remove(newPath);
			fs::rename(oldPath, newPath);
		}
	}

	// 現在のメインログをバックアップへ移動
	// current.log -> backup_1.log
	std::string currentPath = std::string(kLogDir) + "/" + kCurrentFile;
	std::string firstBackupPath = std::string(kLogDir) + "/backup_1.log";

	if (fs::exists(currentPath)) {
		if (fs::exists(firstBackupPath)) fs::remove(firstBackupPath);
		fs::rename(currentPath, firstBackupPath);
	}
}

void Logger::InitializeFileLogging() {
	// ディレクトリの作成
	if (!fs::exists(kLogDir)) {
		fs::create_directories(kLogDir);
	}

	// ファイルの世代交代を実行
	RotateLogFiles();

	// 新しいログファイルをオープン
	std::string logPath = std::string(kLogDir) + "/" + kCurrentFile;
	logStream_.open(logPath, std::ios::out);

	if (logStream_.is_open()) {
		fileStream_ = &logStream_;

		// ログのヘッダーに開始時刻を記録しておく
		auto now = std::chrono::system_clock::now();
		std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
			nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
		std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
		std::string dateStr = std::format("{:%Y-%m-%d %H:%M:%S}", localTime);
		Log("\n--- Logging Started: " + dateStr + " ---\n");
	}
}

void Logger::Log(const std::string& message) {
	// VS出力に出力
	OutputDebugStringA((message + "\n").c_str());

	// ファイルストリームが有効ならファイルにも書き込む
	if (fileStream_ && fileStream_->is_open()) {
		(*fileStream_) << message << std::endl;
		fileStream_->flush(); // 即時書き込みを保証
	}
}

void Logger::Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}