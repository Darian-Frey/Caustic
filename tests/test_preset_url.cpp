#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <caustic/base64.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/preset_url.hpp>

namespace fs = std::filesystem;
using namespace caustic;

// ---------------------------------------------------------------------------
// base64url codec

TEST_CASE("base64url round-trips canonical byte sequences") {
    // RFC-style test vectors mapped to the URL alphabet (no padding, -_ instead of +/).
    CHECK(base64url_encode("")       == "");
    CHECK(base64url_encode("f")      == "Zg");
    CHECK(base64url_encode("fo")     == "Zm8");
    CHECK(base64url_encode("foo")    == "Zm9v");
    CHECK(base64url_encode("foob")   == "Zm9vYg");
    CHECK(base64url_encode("fooba")  == "Zm9vYmE");
    CHECK(base64url_encode("foobar") == "Zm9vYmFy");

    CHECK(base64url_decode("Zg")       == "f");
    CHECK(base64url_decode("Zm8")      == "fo");
    CHECK(base64url_decode("Zm9v")     == "foo");
    CHECK(base64url_decode("Zm9vYg")   == "foob");
    CHECK(base64url_decode("Zm9vYmE")  == "fooba");
    CHECK(base64url_decode("Zm9vYmFy") == "foobar");
}

TEST_CASE("base64url uses URL-safe alphabet (- and _, not + and /)") {
    // 0xFF, 0xEF — bit patterns that map to the high values 62 and 63 in
    // the alphabet, which are `-` and `_` respectively (vs `+` and `/`
    // in standard base64).
    const std::string in = "\xfb\xff";
    const std::string encoded = base64url_encode(in);
    CHECK(encoded.find('+') == std::string::npos);
    CHECK(encoded.find('/') == std::string::npos);
    CHECK(base64url_decode(encoded) == in);
}

TEST_CASE("base64url tolerates trailing = padding (forward compat)") {
    // Some senders auto-pad with =. Decoder accepts but doesn't require it.
    CHECK(base64url_decode("Zg==")   == "f");
    CHECK(base64url_decode("Zm8=")   == "fo");
    CHECK(base64url_decode("Zm9v")   == "foo");  // exact-length, no padding
}

TEST_CASE("base64url rejects malformed input") {
    CHECK_THROWS_AS(base64url_decode("Z"),    std::runtime_error);   // tail of 1
    CHECK_THROWS_AS(base64url_decode("Zm9!"), std::runtime_error);   // invalid char
    CHECK_THROWS_AS(base64url_decode("Z@"),   std::runtime_error);   // invalid char in tail
}

TEST_CASE("base64url handles binary bytes (0x00..0xFF) without loss") {
    std::string in;
    in.reserve(256);
    for (int i = 0; i < 256; ++i) in.push_back(static_cast<char>(i));
    CHECK(base64url_decode(base64url_encode(in)) == in);
}

// ---------------------------------------------------------------------------
// preset_url end-to-end

namespace {

Preset make_simple_preset() {
    Preset p;
    p.name = "test_url_preset";
    p.scene.layers[0].name = "layer 0";
    p.scene.layers[0].generator.type = GeneratorType::ModularChord;
    p.scene.layers[0].generator.chord.N = 200;
    p.scene.layers[0].generator.chord.k = 2.0;
    return p;
}

}  // namespace

TEST_CASE("encode_preset_url produces a string with the caustic:p1: prefix") {
    const Preset p = make_simple_preset();
    const std::string url = encode_preset_url(p);
    CHECK(url.size() > 11);
    CHECK(url.substr(0, 11) == "caustic:p1:");
}

TEST_CASE("Preset round-trips through encode_preset_url / decode_preset_url") {
    const Preset original = make_simple_preset();
    const std::string url = encode_preset_url(original);
    const Preset back = decode_preset_url(url);
    CHECK(back.name == original.name);
    CHECK(back.scene.layers.size() == original.scene.layers.size());
    CHECK(back.scene.layers[0].generator.type == original.scene.layers[0].generator.type);
    CHECK(back.scene.layers[0].generator.chord.N == original.scene.layers[0].generator.chord.N);
    CHECK(back.scene.layers[0].generator.chord.k == original.scene.layers[0].generator.chord.k);
}

TEST_CASE("encode_preset_url is deterministic — same preset → same URL") {
    const Preset p = make_simple_preset();
    const std::string a = encode_preset_url(p);
    const std::string b = encode_preset_url(p);
    CHECK(a == b);
}

TEST_CASE("decode_preset_url tolerates whitespace at either end") {
    const Preset original = make_simple_preset();
    const std::string url = encode_preset_url(original);

    const std::string with_leading  = "  \n" + url;
    const std::string with_trailing = url + "\n\t  ";
    const std::string with_both     = " \t" + url + "\n";

    CHECK_NOTHROW((void)decode_preset_url(with_leading));
    CHECK_NOTHROW((void)decode_preset_url(with_trailing));
    CHECK_NOTHROW((void)decode_preset_url(with_both));
}

TEST_CASE("decode_preset_url rejects missing prefix") {
    CHECK_THROWS_AS(decode_preset_url("hello world"),   std::runtime_error);
    CHECK_THROWS_AS(decode_preset_url("caustic:Zm9v"),  std::runtime_error);  // wrong version tag
    CHECK_THROWS_AS(decode_preset_url("CAUSTIC:p1:Zg"), std::runtime_error);  // case-sensitive
    CHECK_THROWS_AS(decode_preset_url(""),              std::runtime_error);
}

TEST_CASE("decode_preset_url rejects malformed payload") {
    CHECK_THROWS_AS(decode_preset_url("caustic:p1:"),       std::runtime_error);  // empty body → JSON parse fail
    CHECK_THROWS_AS(decode_preset_url("caustic:p1:Z@@"),    std::runtime_error);  // bad base64
    CHECK_THROWS_AS(decode_preset_url("caustic:p1:Zm9v"),   std::runtime_error);  // "foo" → not JSON
}

TEST_CASE("is_preset_url sniffs correctly without throwing") {
    CHECK(is_preset_url("caustic:p1:Zg"));
    CHECK(is_preset_url("   caustic:p1:Zg\n"));
    CHECK_FALSE(is_preset_url("hello"));
    CHECK_FALSE(is_preset_url("caustic:p2:Zg"));     // future version
    CHECK_FALSE(is_preset_url("https://caustic.app/")); // not a preset URL
    CHECK_FALSE(is_preset_url(""));
}

TEST_CASE("bundled presets all round-trip cleanly through the URL format") {
    const fs::path repo_presets = "presets";
    const fs::path alt          = fs::path("..") / "presets";
    const fs::path dir = fs::exists(repo_presets) ? repo_presets : alt;
    if (!fs::exists(dir)) {
        MESSAGE("skipping bundled-preset URL round-trip — presets/ not found");
        return;
    }
    int checked = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") continue;
        const Preset original = load_preset(entry.path());
        const std::string url = encode_preset_url(original);

        // URL is sniffable + decodable.
        CHECK(is_preset_url(url));
        const Preset back = decode_preset_url(url);

        // Spot-check structural equality on fields that the JSON schema
        // round-trips: name, layer count, first generator type.
        CHECK(back.name == original.name);
        CHECK(back.scene.layers.size() == original.scene.layers.size());
        CHECK(back.scene.layers[0].generator.type ==
              original.scene.layers[0].generator.type);
        ++checked;
    }
    CHECK(checked > 0);
}
