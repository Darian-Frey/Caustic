#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include <caustic/preset.hpp>

namespace caustic {

namespace detail {

inline std::string color_to_hex(Color c) {
    auto byte = [](double v) -> int {
        return static_cast<int>(std::clamp(v * 255.0 + 0.5, 0.0, 255.0));
    };
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", byte(c.r), byte(c.g), byte(c.b));
    return buf;
}

inline Color color_from_hex(const std::string& s) {
    if (s.size() != 7 || s[0] != '#') {
        throw std::runtime_error("invalid color hex (expected #rrggbb): " + s);
    }
    auto parse = [&](int idx) -> double {
        return std::stoi(s.substr(1 + idx * 2, 2), nullptr, 16) / 255.0;
    };
    return {parse(0), parse(1), parse(2), 1.0};
}

inline std::string indexer_to_string(Indexer i) {
    switch (i) {
        case Indexer::ChordIndex:  return "by_chord_index";
        case Indexer::ChordLength: return "by_chord_length";
        case Indexer::Angle:       return "by_angle";
        case Indexer::CurveT:      return "by_curve_t";
    }
    return "by_chord_index";
}

inline Indexer indexer_from_string(const std::string& s) {
    if (s == "by_chord_index")  return Indexer::ChordIndex;
    if (s == "by_chord_length") return Indexer::ChordLength;
    if (s == "by_angle")        return Indexer::Angle;
    if (s == "by_curve_t")      return Indexer::CurveT;
    throw std::runtime_error("unknown indexer: " + s);
}

inline std::string generator_type_to_string(GeneratorType t) {
    switch (t) {
        case GeneratorType::ModularChord: return "modular_chord";
        case GeneratorType::Hypotrochoid: return "hypotrochoid";
        case GeneratorType::Epitrochoid:  return "epitrochoid";
        case GeneratorType::Lissajous:    return "lissajous";
    }
    return "modular_chord";
}

inline GeneratorType generator_type_from_string(const std::string& s) {
    if (s == "modular_chord") return GeneratorType::ModularChord;
    if (s == "hypotrochoid")  return GeneratorType::Hypotrochoid;
    if (s == "epitrochoid")   return GeneratorType::Epitrochoid;
    if (s == "lissajous")     return GeneratorType::Lissajous;
    throw std::runtime_error("unknown generator type: " + s);
}

inline std::string colormap_type_to_string(ColorMapType t) {
    switch (t) {
        case ColorMapType::Solid:          return "solid";
        case ColorMapType::LinearGradient: return "linear_gradient";
        case ColorMapType::HsvSweep:       return "hsv_sweep";
        case ColorMapType::Diverging:      return "diverging";
    }
    return "solid";
}

inline ColorMapType colormap_type_from_string(const std::string& s) {
    if (s == "solid")           return ColorMapType::Solid;
    if (s == "linear_gradient") return ColorMapType::LinearGradient;
    if (s == "hsv_sweep")       return ColorMapType::HsvSweep;
    if (s == "diverging")       return ColorMapType::Diverging;
    throw std::runtime_error("unknown colormap type: " + s);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// nlohmann/json ADL hooks

inline void to_json(nlohmann::json& j, const Preset& p) {
    j = nlohmann::json::object();
    j["version"] = p.version;
    j["name"] = p.name;

    auto& g = j["generator"];
    g["type"] = detail::generator_type_to_string(p.generator.type);
    auto& gp = g["params"] = nlohmann::json::object();
    switch (p.generator.type) {
        case GeneratorType::ModularChord:
            gp["N"] = p.generator.chord.N;
            gp["k"] = p.generator.chord.k;
            break;
        case GeneratorType::Hypotrochoid:
            gp["R"] = p.generator.hypo.R;
            gp["r"] = p.generator.hypo.r;
            gp["d"] = p.generator.hypo.d;
            gp["samples"] = p.generator.hypo.samples;
            break;
        case GeneratorType::Epitrochoid:
            gp["R"] = p.generator.epi.R;
            gp["r"] = p.generator.epi.r;
            gp["d"] = p.generator.epi.d;
            gp["samples"] = p.generator.epi.samples;
            break;
        case GeneratorType::Lissajous:
            gp["A"] = p.generator.liss.A;
            gp["B"] = p.generator.liss.B;
            gp["a"] = p.generator.liss.a;
            gp["b"] = p.generator.liss.b;
            gp["phi"] = p.generator.liss.phi;
            gp["samples"] = p.generator.liss.samples;
            break;
    }

    auto& s = j["style"];
    auto& cm = s["color_map"] = nlohmann::json::object();
    cm["type"] = detail::colormap_type_to_string(p.style.colormap_type);
    switch (p.style.colormap_type) {
        case ColorMapType::Solid:
            cm["color"] = detail::color_to_hex(p.style.solid_color);
            break;
        case ColorMapType::LinearGradient:
            cm["start"] = detail::color_to_hex(p.style.gradient_start);
            cm["end"]   = detail::color_to_hex(p.style.gradient_end);
            break;
        case ColorMapType::HsvSweep:
            cm["hue_start"]  = p.style.hue_start;
            cm["hue_end"]    = p.style.hue_end;
            cm["saturation"] = p.style.hsv_saturation;
            cm["value"]      = p.style.hsv_value;
            break;
        case ColorMapType::Diverging:
            cm["negative"] = detail::color_to_hex(p.style.div_negative);
            cm["midpoint"] = detail::color_to_hex(p.style.div_midpoint);
            cm["positive"] = detail::color_to_hex(p.style.div_positive);
            break;
    }
    s["color_indexer"] = detail::indexer_to_string(p.style.color_indexer);
    auto& stroke = s["stroke"] = nlohmann::json::object();
    stroke["width_min"]     = p.style.stroke_width_min;
    stroke["width_max"]     = p.style.stroke_width_max;
    stroke["width_indexer"] = detail::indexer_to_string(p.style.stroke_width_indexer);
    stroke["opacity"]       = p.style.opacity;
    s["background"] = detail::color_to_hex(p.style.background);
    s["cyclic"] = p.style.cyclic;

    auto& c = j["camera"];
    c["pan_x_px"] = p.camera.pan_x_px;
    c["pan_y_px"] = p.camera.pan_y_px;
    c["zoom"] = p.camera.zoom;
}

inline void from_json(const nlohmann::json& j, Preset& p) {
    p.version = j.at("version").get<int>();
    if (p.version != 1) {
        throw std::runtime_error("unsupported preset version " + std::to_string(p.version));
    }
    p.name = j.value("name", std::string{});

    const auto& g = j.at("generator");
    p.generator.type = detail::generator_type_from_string(g.at("type").get<std::string>());
    const auto& gp = g.at("params");
    switch (p.generator.type) {
        case GeneratorType::ModularChord:
            p.generator.chord.N = gp.value("N", p.generator.chord.N);
            p.generator.chord.k = gp.value("k", p.generator.chord.k);
            break;
        case GeneratorType::Hypotrochoid:
            p.generator.hypo.R       = gp.value("R", p.generator.hypo.R);
            p.generator.hypo.r       = gp.value("r", p.generator.hypo.r);
            p.generator.hypo.d       = gp.value("d", p.generator.hypo.d);
            p.generator.hypo.samples = gp.value("samples", p.generator.hypo.samples);
            break;
        case GeneratorType::Epitrochoid:
            p.generator.epi.R       = gp.value("R", p.generator.epi.R);
            p.generator.epi.r       = gp.value("r", p.generator.epi.r);
            p.generator.epi.d       = gp.value("d", p.generator.epi.d);
            p.generator.epi.samples = gp.value("samples", p.generator.epi.samples);
            break;
        case GeneratorType::Lissajous:
            p.generator.liss.A       = gp.value("A", p.generator.liss.A);
            p.generator.liss.B       = gp.value("B", p.generator.liss.B);
            p.generator.liss.a       = gp.value("a", p.generator.liss.a);
            p.generator.liss.b       = gp.value("b", p.generator.liss.b);
            p.generator.liss.phi     = gp.value("phi", p.generator.liss.phi);
            p.generator.liss.samples = gp.value("samples", p.generator.liss.samples);
            break;
    }

    const auto& s = j.at("style");
    const auto& cm = s.at("color_map");
    p.style.colormap_type = detail::colormap_type_from_string(cm.at("type").get<std::string>());
    switch (p.style.colormap_type) {
        case ColorMapType::Solid:
            p.style.solid_color = detail::color_from_hex(cm.at("color").get<std::string>());
            break;
        case ColorMapType::LinearGradient:
            p.style.gradient_start = detail::color_from_hex(cm.at("start").get<std::string>());
            p.style.gradient_end   = detail::color_from_hex(cm.at("end").get<std::string>());
            break;
        case ColorMapType::HsvSweep:
            p.style.hue_start      = cm.value("hue_start", p.style.hue_start);
            p.style.hue_end        = cm.value("hue_end",   p.style.hue_end);
            p.style.hsv_saturation = cm.value("saturation", p.style.hsv_saturation);
            p.style.hsv_value      = cm.value("value",      p.style.hsv_value);
            break;
        case ColorMapType::Diverging:
            p.style.div_negative = detail::color_from_hex(cm.at("negative").get<std::string>());
            p.style.div_midpoint = detail::color_from_hex(cm.at("midpoint").get<std::string>());
            p.style.div_positive = detail::color_from_hex(cm.at("positive").get<std::string>());
            break;
    }
    p.style.color_indexer = detail::indexer_from_string(s.at("color_indexer").get<std::string>());
    const auto& stroke = s.at("stroke");
    p.style.stroke_width_min = stroke.value("width_min", p.style.stroke_width_min);
    p.style.stroke_width_max = stroke.value("width_max", p.style.stroke_width_max);
    p.style.stroke_width_indexer = detail::indexer_from_string(
        stroke.value("width_indexer", std::string("by_chord_index")));
    p.style.opacity = stroke.value("opacity", p.style.opacity);
    p.style.background = detail::color_from_hex(s.value("background", std::string("#0a0a0a")));
    p.style.cyclic = s.value("cyclic", p.style.cyclic);

    if (j.contains("camera")) {
        const auto& c = j.at("camera");
        p.camera.pan_x_px = c.value("pan_x_px", 0.0);
        p.camera.pan_y_px = c.value("pan_y_px", 0.0);
        p.camera.zoom     = c.value("zoom", 1.0);
    }
}

// ---------------------------------------------------------------------------
// File I/O

inline void save_preset(const std::filesystem::path& path, const Preset& p) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot open for write: " + path.string());
    nlohmann::json j;
    to_json(j, p);
    out << j.dump(2) << "\n";
}

inline Preset load_preset(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open: " + path.string());
    nlohmann::json j;
    in >> j;
    Preset p;
    from_json(j, p);
    return p;
}

// XDG: $XDG_CONFIG_HOME/caustic/presets, falling back to $HOME/.config/...
inline std::filesystem::path user_preset_dir() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "caustic" / "presets";
    }
    const char* home = std::getenv("HOME");
    const std::filesystem::path base = (home && *home) ? std::filesystem::path(home) / ".config"
                                                       : std::filesystem::path(".");
    return base / "caustic" / "presets";
}

}  // namespace caustic
