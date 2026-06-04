#pragma once

#include "esphome/components/daikin_rotex_can/types.h"
#include "esphome/core/log.h"
#include <functional>
#include <memory>
#include <map>
#include <cstdio>
#include <string>

namespace esphome {
namespace daikin_rotex_can {

class Utils {
    friend class LogFilterText;

    using TVoidFunc = std::function<void()>;
public:
    template<typename... Args>
    static std::string format(const std::string& format, Args... args) {
        // Fast path: format into a stack buffer. The vast majority of log
        // messages fit, so we avoid a heap allocation entirely. Only the rare
        // oversized message falls back to an exact-size heap buffer.
        char stack_buffer[256];
        const int needed = std::snprintf(stack_buffer, sizeof(stack_buffer), format.c_str(), args...);
        if (needed < 0) {
            return std::string();
        }
        if (static_cast<size_t>(needed) < sizeof(stack_buffer)) {
            return std::string(stack_buffer, static_cast<size_t>(needed));
        }
        std::string result(static_cast<size_t>(needed), '\0');
        std::snprintf(result.data(), static_cast<size_t>(needed) + 1, format.c_str(), args...);
        return result;
    }

    template<typename First, typename ... T>
    static bool is_in(First &&first, T && ... t) {
        return ((first == t) || ...);
    }

    static std::string to_hex(TMessage const& data);
    static bool find(std::string const& haystack, std::string const& needle);
    static std::vector<std::string> split(std::string const& str);
    static std::string to_hex(uint32_t value);
    static TMessage str_to_bytes(const std::string& str);
    static TMessage str_to_bytes_array8(const std::string& str);
    static std::map<uint16_t, std::string> str_to_map(const std::string& input);
    static uint16_t hex_to_uint16(const std::string& hexStr);
    static void setBytes(TMessage& data, int value, uint8_t offset, uint8_t len);

    // True only if the firmware is built with a log level that can actually emit
    // these messages (DEBUG or finer). When it isn't (e.g. logger.level: WARN in
    // production), the compiler folds this to `false` and dead-code-eliminates
    // the whole log() body below, so no formatting is done at all.
    static constexpr bool logging_enabled() {
        return ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG;
    }

    template<typename... Args>
    static void log(std::string const& tag, std::string const& str_format, Args... args) {
        if (!logging_enabled()) {
            return;
        }
        log_impl(tag, Utils::format(str_format, args...));
    }

    static bool equals(float a, float b, float epsilon);

private:
    static void log_impl(std::string const& tag, std::string const& formatted);

    static std::string g_log_filter;
};

inline bool Utils::equals(float a, float b, float epsilon) {
    return std::abs(a - b) <= epsilon;
}

}
}