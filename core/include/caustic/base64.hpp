#pragma once

// base64url codec — URL-safe alphabet (`-` `_` instead of `+` `/`), no
// padding. Used by preset_url.hpp to wrap a JSON payload into a shareable
// caustic:p1: string. Header-only, no external deps, throws std::runtime_error
// on malformed input.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace caustic {

inline std::string base64url_encode(std::string_view bytes) {
    constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);

    auto byte = [&](std::size_t i) {
        return static_cast<std::uint8_t>(bytes[i]);
    };

    std::size_t i = 0;
    for (; i + 3 <= bytes.size(); i += 3) {
        const std::uint32_t v =
            (static_cast<std::uint32_t>(byte(i))     << 16) |
            (static_cast<std::uint32_t>(byte(i + 1)) <<  8) |
             static_cast<std::uint32_t>(byte(i + 2));
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        out.push_back(kAlphabet[(v >>  6) & 0x3F]);
        out.push_back(kAlphabet[ v        & 0x3F]);
    }
    // Tail of 1 or 2 leftover bytes — emit 2 or 3 chars, no padding.
    if (i < bytes.size()) {
        std::uint32_t v = static_cast<std::uint32_t>(byte(i)) << 16;
        if (i + 1 < bytes.size()) {
            v |= static_cast<std::uint32_t>(byte(i + 1)) << 8;
        }
        out.push_back(kAlphabet[(v >> 18) & 0x3F]);
        out.push_back(kAlphabet[(v >> 12) & 0x3F]);
        if (i + 1 < bytes.size()) {
            out.push_back(kAlphabet[(v >> 6) & 0x3F]);
        }
    }
    return out;
}

inline std::string base64url_decode(std::string_view s) {
    auto value_of = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-') return 62;
        if (c == '_') return 63;
        return -1;
    };

    // Tolerate optional `=` padding for forward compat with plain base64
    // inputs — strip trailing `=`s before length checks.
    while (!s.empty() && s.back() == '=') s.remove_suffix(1);

    if (s.size() % 4 == 1) {
        throw std::runtime_error("base64url: orphan character at end");
    }

    std::string out;
    out.reserve(s.size() * 3 / 4);

    std::size_t i = 0;
    for (; i + 4 <= s.size(); i += 4) {
        const int a = value_of(s[i]);
        const int b = value_of(s[i + 1]);
        const int c = value_of(s[i + 2]);
        const int d = value_of(s[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) {
            throw std::runtime_error("base64url: invalid character");
        }
        const std::uint32_t v =
            (static_cast<std::uint32_t>(a) << 18) |
            (static_cast<std::uint32_t>(b) << 12) |
            (static_cast<std::uint32_t>(c) <<  6) |
             static_cast<std::uint32_t>(d);
        out.push_back(static_cast<char>((v >> 16) & 0xFF));
        out.push_back(static_cast<char>((v >>  8) & 0xFF));
        out.push_back(static_cast<char>( v        & 0xFF));
    }
    const std::size_t tail = s.size() - i;
    if (tail >= 2) {
        const int a = value_of(s[i]);
        const int b = value_of(s[i + 1]);
        if (a < 0 || b < 0) {
            throw std::runtime_error("base64url: invalid character in tail");
        }
        std::uint32_t v = (static_cast<std::uint32_t>(a) << 18) |
                          (static_cast<std::uint32_t>(b) << 12);
        if (tail == 3) {
            const int c = value_of(s[i + 2]);
            if (c < 0) {
                throw std::runtime_error("base64url: invalid character in tail");
            }
            v |= static_cast<std::uint32_t>(c) << 6;
            out.push_back(static_cast<char>((v >> 16) & 0xFF));
            out.push_back(static_cast<char>((v >>  8) & 0xFF));
        } else {
            out.push_back(static_cast<char>((v >> 16) & 0xFF));
        }
    }
    return out;
}

}  // namespace caustic
