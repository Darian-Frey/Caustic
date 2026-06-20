#pragma once

// Shareable preset URLs — encode the current preset to a compact, copy-
// pasteable string and decode it back. The string is one line, looks like
//
//     caustic:p1:eyJ2ZXJzaW9uIjoyLCJuYW1lIjoidGVzdCJ9
//
// (a `caustic:p1:` magic prefix followed by base64url'd compact JSON), so it
// survives chat platforms, email, and forum messages without escaping.
// Whitespace at either end is tolerated to make copy-paste robust.
//
// v1 has no compression — typical presets encode to 1–7 KB which works
// everywhere worth caring about. If a hand-authored CustomChord layer ever
// proves too large for sharing, miniz compression is a clean follow-up
// (bump the prefix to `caustic:p2:` and keep the v1 decoder for backwards
// compatibility).

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include <caustic/base64.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>

namespace caustic {

inline constexpr std::string_view kPresetUrlPrefixV1 = "caustic:p1:";

namespace detail {

// Trim ASCII whitespace from both ends — newlines and spaces creep in via
// copy-paste from chat / email clients.
inline std::string_view trim_ws(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace detail

// True iff the input (after trimming whitespace) starts with one of the
// recognised preset-URL prefixes. Cheap sniff test the UI can use to decide
// whether a clipboard string is worth trying to decode.
inline bool is_preset_url(std::string_view s) {
    const auto trimmed = detail::trim_ws(s);
    return trimmed.size() >= kPresetUrlPrefixV1.size() &&
           trimmed.substr(0, kPresetUrlPrefixV1.size()) == kPresetUrlPrefixV1;
}

// Serialise a preset to JSON, compact-encode, base64url-wrap, and prepend
// the magic prefix. Output is deterministic per input — same preset → same
// URL byte-for-byte.
inline std::string encode_preset_url(const Preset& p) {
    nlohmann::json j;
    to_json(j, p);
    const std::string body = j.dump();  // compact (no indent)
    std::string url;
    url.reserve(kPresetUrlPrefixV1.size() + body.size() * 4 / 3 + 4);
    url.append(kPresetUrlPrefixV1);
    url.append(base64url_encode(body));
    return url;
}

// Decode a preset URL. Throws std::runtime_error with a useful message on:
//   * missing or wrong prefix
//   * malformed base64url
//   * malformed JSON
//   * preset schema violations (re-thrown from preset_io's from_json)
// Accepts whitespace at either end so copy-paste from chat works.
inline Preset decode_preset_url(std::string_view s) {
    auto trimmed = detail::trim_ws(s);
    if (trimmed.size() < kPresetUrlPrefixV1.size() ||
        trimmed.substr(0, kPresetUrlPrefixV1.size()) != kPresetUrlPrefixV1) {
        throw std::runtime_error("preset URL: missing 'caustic:p1:' prefix");
    }
    trimmed.remove_prefix(kPresetUrlPrefixV1.size());
    const std::string body = base64url_decode(trimmed);
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(body);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string("preset URL: JSON parse failed — ") + e.what());
    }
    Preset p;
    from_json(j, p);
    return p;
}

}  // namespace caustic
