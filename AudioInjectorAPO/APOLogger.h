#pragma once
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERRR,
    CRITICAL
};

struct LogEntry {
    std::string message;
    LogLevel level;
    std::chrono::system_clock::time_point timestamp;
};

class APOLogger {
private:
    static constexpr size_t BUFFER_SIZE = 8192; // Power of 2 is important

    // Ring buffer for lock-free logging from real-time threads
    std::vector<LogEntry> m_buffer;
    std::atomic<size_t> m_writeIndex{0};
    std::atomic<size_t> m_readIndex{0};

    // Worker thread for asynchronous processing
    std::thread m_workerThread;
    std::atomic<bool> m_running{false};

    // File output
    std::ofstream m_logFile;
    std::mutex m_fileMutex; // Only used by worker thread

    // Notification mechanism for low-frequency wakeups
    std::condition_variable m_cv;
    std::mutex m_cvMutex;

    // Singleton instance
    static APOLogger* s_instance;

    // Private constructor for singleton
    APOLogger() : m_buffer(BUFFER_SIZE) {}

    // Worker thread function - processes log entries from the ring buffer
    void ProcessLogs() {
        while (m_running) {
            bool processedEntries = false;

            // Process available log entries
            while (m_readIndex.load(std::memory_order_relaxed) !=
                   m_writeIndex.load(std::memory_order_acquire)) {

                size_t index = m_readIndex.load(std::memory_order_relaxed) & (BUFFER_SIZE - 1);
                LogEntry& entry = m_buffer[index];

                // Write to file (non-realtime thread can do file I/O)
                {
                    std::lock_guard<std::mutex> lock(m_fileMutex);
                    if (m_logFile.is_open()) {
                        auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            entry.timestamp.time_since_epoch()) % 1000;

                        std::tm timeInfo;
                        localtime_s(&timeInfo, &time);

                        m_logFile << "[" << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
                            << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
                            << LevelToString(entry.level) << ": "
                            << entry.message << std::endl;
                    }
                }

                // Increment read index
                m_readIndex.fetch_add(1, std::memory_order_release);
                processedEntries = true;
            }

            // If no entries were processed, wait for notification
            if (!processedEntries) {
                std::unique_lock<std::mutex> lock(m_cvMutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(100));
            }
        }
    }

    // Convert log level to string
    const char* LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERRR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

public:
    // Get singleton instance
    static APOLogger& GetInstance() {
        static APOLogger instance;
        return instance;
    }

    // Initialize the logger
    bool Initialize(const std::string& logPath) {
        if (m_running) {
            return true; // Already initialized
        }

        {
            std::lock_guard<std::mutex> lock(m_fileMutex);
            m_logFile.open(logPath, std::ios::out | std::ios::app);
            if (!m_logFile.is_open()) {
                return false;
            }
        }

        m_running = true;
        m_workerThread = std::thread(&APOLogger::ProcessLogs, this);

        // Log initialization event
        LogRealtime(LogLevel::INFO, "APO Logger initialized");
        return true;
    }

    // Shutdown the logger
    void Shutdown() {
        if (!m_running) {
            return; // Already shutdown
        }

        // Log shutdown event
        LogRealtime(LogLevel::INFO, "APO Logger shutting down");

        // Stop worker thread
        m_running = false;
        m_cv.notify_all();

        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        // Close log file
        std::lock_guard<std::mutex> lock(m_fileMutex);
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }

    // Safe for real-time threads - lock-free logging through ring buffer
    void LogRealtime(LogLevel level, const std::string& message) {
        // Get next write position (atomic)
        size_t writeIndex = m_writeIndex.fetch_add(1, std::memory_order_relaxed);
        size_t index = writeIndex & (BUFFER_SIZE - 1); // Wrap around using power-of-2 size

        // Create log entry
        LogEntry& entry = m_buffer[index];
        entry.message = message;
        entry.level = level;
        entry.timestamp = std::chrono::system_clock::now();

        // Signal that data is ready
        m_writeIndex.store(writeIndex + 1, std::memory_order_release);

        // Notify worker thread (only occasionally to avoid excessive wakeups)
        // This is a heuristic - wake up after several entries or when the buffer is filling
        if ((writeIndex % 32) == 0 ||
            ((writeIndex - m_readIndex.load(std::memory_order_relaxed)) > (BUFFER_SIZE / 2))) {
            std::lock_guard<std::mutex> lock(m_cvMutex);
            m_cv.notify_one();
        }
    }

    // Template version that supports formatting like printf but type-safe
    template<typename... Args>
    void LogRealtimeFormat(LogLevel level, const char* format, Args... args) {
        // Format string
        char buffer[1024]{ 0 };
        snprintf(buffer, sizeof(buffer), format, args...);
        LogRealtime(level, buffer);
    }

    // Non-realtime logging (for initialization, shutdown, etc.)
    void Log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(m_fileMutex);
        if (m_logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::tm timeInfo;
            localtime_s(&timeInfo, &time);

            m_logFile << "[" << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S")
                << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
                << LevelToString(level) << ": "
                << message << std::endl;
        }
    }
};

// Convenience macros for APO real-time logging
#define APO_LOG_TRACE(msg) APOLogger::GetInstance().LogRealtime(LogLevel::TRACE, msg)
#define APO_LOG_DEBUG(msg) APOLogger::GetInstance().LogRealtime(LogLevel::DEBUG, msg)
#define APO_LOG_INFO(msg) APOLogger::GetInstance().LogRealtime(LogLevel::INFO, msg)
#define APO_LOG_WARN(msg) APOLogger::GetInstance().LogRealtime(LogLevel::WARN, msg)
#define APO_LOG_ERROR(msg) APOLogger::GetInstance().LogRealtime(LogLevel::ERRR, msg)
#define APO_LOG_CRITICAL(msg) APOLogger::GetInstance().LogRealtime(LogLevel::CRITICAL, msg)

// Format versions
#define APO_LOG_TRACE_F(fmt, ...) APOLogger::GetInstance().LogRealtimeFormat(LogLevel::TRACE, fmt, __VA_ARGS__)
#define APO_LOG_DEBUG_F(fmt, ...) APOLogger::GetInstance().LogRealtimeFormat(LogLevel::DEBUG, fmt, __VA_ARGS__)
#define APO_LOG_INFO_F(fmt, ...) APOLogger::GetInstance().LogRealtimeFormat(LogLevel::INFO, fmt, __VA_ARGS__)
#define APO_LOG_WARN_F(fmt, ...) APOLogger::GetInstance().LogRealtimeFormat(LogLevel::WARN, fmt, __VA_ARGS__)
#define APO_LOG_ERROR_F(fmt, ...) APOLogger::GetInstance().LogRealtimeFormat(LogLevel::ERRR, fmt, __VA_ARGS__)
#define APO_LOG_CRITICAL_F(fmt, ...) APOLogger::GetInstance().LogRealtimeFormat(LogLevel::CRITICAL, fmt, __VA_ARGS__)
