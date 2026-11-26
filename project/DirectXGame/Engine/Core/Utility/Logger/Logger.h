#pragma once

#include <fstream>
#include <ostream>
#include <string>

// ログ出力クラス
class Logger {
public:
	static void InitializeFileLogging();

	static void Log(const std::string& message);

	static void Log(std::ostream& os, const std::string& message);

private:
	static void RotateLogFiles();

	// 内部管理用の静的メンバ変数
	static inline std::ofstream logStream_;
	static inline std::ofstream* fileStream_ = nullptr;

	// 設定定数
	static constexpr const char* kLogDir = "logs";
	static constexpr const char* kCurrentFile = "current.log";
	static constexpr int kMaxBackups = 5;
};
