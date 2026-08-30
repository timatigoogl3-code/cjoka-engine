#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>

enum class LogLevel {
    Info,
    Warning,
    Error,
    Script
};

struct LogMessage {
    LogLevel level = LogLevel::Info;
    std::string message;
    std::string timestamp;
    std::string tag;
};

class DebugLog {
public:
    static DebugLog& Get() {
        static DebugLog instance;
        return instance;
    }

    static void SetEditorMode(bool inEditor) {
        Get().m_isEditorMode = inEditor;
    }

    static bool IsEditorMode() {
        return Get().m_isEditorMode;
    }

    static void Log(const std::string& msg, const std::string& tag = "Editor") {
        if (!Get().m_isEditorMode) return;
        Get().addMessage(LogLevel::Info, msg, tag);
    }

    static void LogWarning(const std::string& msg, const std::string& tag = "Editor") {
        if (!Get().m_isEditorMode) return;
        Get().addMessage(LogLevel::Warning, msg, tag);
    }

    static void LogError(const std::string& msg, const std::string& tag = "Editor") {
        if (!Get().m_isEditorMode) return;
        Get().addMessage(LogLevel::Error, msg, tag);
    }

    static void LogScript(const std::string& msg, const std::string& scriptName = "Script") {
        if (!Get().m_isEditorMode) return;
        Get().addMessage(LogLevel::Script, msg, scriptName);
    }

    std::vector<LogMessage> getMessages() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.clear();
    }

private:
    DebugLog() = default;

    void addMessage(LogLevel level, const std::string& msg, const std::string& tag) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");

        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push_back({level, msg, ss.str(), tag});
        if (m_messages.size() > 500) {
            m_messages.erase(m_messages.begin(), m_messages.begin() + 50);
        }

        if (level == LogLevel::Error) {
            std::cerr << "[" << ss.str() << "] [ERROR] [" << tag << "] " << msg << std::endl;
        } else if (level == LogLevel::Warning) {
            std::cout << "[" << ss.str() << "] [WARN] [" << tag << "] " << msg << std::endl;
        } else {
            std::cout << "[" << ss.str() << "] [LOG] [" << tag << "] " << msg << std::endl;
        }
    }

    bool m_isEditorMode = false;
    std::vector<LogMessage> m_messages;
    std::mutex m_mutex;
};

#define DEBUG_LOG(msg) DebugLog::Log(msg)
#define DEBUG_LOG_WARN(msg) DebugLog::LogWarning(msg)
#define DEBUG_LOG_ERROR(msg) DebugLog::LogError(msg)
