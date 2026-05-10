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
        case GeneratorType::ModularChord:   return "modular_chord";
        case GeneratorType::Hypotrochoid:   return "hypotrochoid";
        case GeneratorType::Epitrochoid:    return "epitrochoid";
        case GeneratorType::Lissajous:      return "lissajous";
        case GeneratorType::Rose:           return "rose";
        case GeneratorType::Superformula:   return "superformula";
        case GeneratorType::Phyllotaxis:    return "phyllotaxis";
        case GeneratorType::PolygonChord:   return "polygon_chord";
        case GeneratorType::LinearEnvelope: return "linear_envelope";
    }
    return "modular_chord";
}

inline GeneratorType generator_type_from_string(const std::string& s) {
    if (s == "modular_chord")   return GeneratorType::ModularChord;
    if (s == "hypotrochoid")    return GeneratorType::Hypotrochoid;
    if (s == "epitrochoid")     return GeneratorType::Epitrochoid;
    if (s == "lissajous")       return GeneratorType::Lissajous;
    if (s == "rose")            return GeneratorType::Rose;
    if (s == "superformula")    return GeneratorType::Superformula;
    if (s == "phyllotaxis")     return GeneratorType::Phyllotaxis;
    if (s == "polygon_chord")   return GeneratorType::PolygonChord;
    if (s == "linear_envelope") return GeneratorType::LinearEnvelope;
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

// --- nested object writers/readers --------------------------------------

inline void generator_to_json(nlohmann::json& g, const GeneratorSpec& gs) {
    g["type"] = generator_type_to_string(gs.type);
    auto& gp = g["params"] = nlohmann::json::object();
    switch (gs.type) {
        case GeneratorType::ModularChord:
            gp["N"] = gs.chord.N;
            gp["k"] = gs.chord.k;
            break;
        case GeneratorType::Hypotrochoid:
            gp["R"] = gs.hypo.R;
            gp["r"] = gs.hypo.r;
            gp["d"] = gs.hypo.d;
            gp["samples"] = gs.hypo.samples;
            break;
        case GeneratorType::Epitrochoid:
            gp["R"] = gs.epi.R;
            gp["r"] = gs.epi.r;
            gp["d"] = gs.epi.d;
            gp["samples"] = gs.epi.samples;
            break;
        case GeneratorType::Lissajous:
            gp["A"] = gs.liss.A;
            gp["B"] = gs.liss.B;
            gp["a"] = gs.liss.a;
            gp["b"] = gs.liss.b;
            gp["phi"] = gs.liss.phi;
            gp["samples"] = gs.liss.samples;
            break;
        case GeneratorType::Rose:
            gp["n"] = gs.rose.n;
            gp["d"] = gs.rose.d;
            gp["samples"] = gs.rose.samples;
            break;
        case GeneratorType::Superformula:
            gp["m"] = gs.supf.m;
            gp["n1"] = gs.supf.n1;
            gp["n2"] = gs.supf.n2;
            gp["n3"] = gs.supf.n3;
            gp["a"] = gs.supf.a;
            gp["b"] = gs.supf.b;
            gp["samples"] = gs.supf.samples;
            break;
        case GeneratorType::Phyllotaxis:
            gp["N"] = gs.phyl.N;
            gp["alpha"] = gs.phyl.alpha;
            gp["k"] = gs.phyl.k;
            break;
        case GeneratorType::PolygonChord:
            gp["n_sides"] = gs.poly.n_sides;
            gp["N"] = gs.poly.N;
            gp["k"] = gs.poly.k;
            gp["rotation_rad"] = gs.poly.rotation_rad;
            break;
        case GeneratorType::LinearEnvelope:
            gp["a_start"] = nlohmann::json::array({gs.lenv.a_start.x, gs.lenv.a_start.y});
            gp["a_end"]   = nlohmann::json::array({gs.lenv.a_end.x,   gs.lenv.a_end.y});
            gp["b_start"] = nlohmann::json::array({gs.lenv.b_start.x, gs.lenv.b_start.y});
            gp["b_end"]   = nlohmann::json::array({gs.lenv.b_end.x,   gs.lenv.b_end.y});
            gp["N"] = gs.lenv.N;
            gp["k"] = gs.lenv.k;
            break;
    }
}

inline void generator_from_json(const nlohmann::json& g, GeneratorSpec& gs) {
    gs.type = generator_type_from_string(g.at("type").get<std::string>());
    const auto& gp = g.at("params");
    switch (gs.type) {
        case GeneratorType::ModularChord:
            gs.chord.N = gp.value("N", gs.chord.N);
            gs.chord.k = gp.value("k", gs.chord.k);
            break;
        case GeneratorType::Hypotrochoid:
            gs.hypo.R       = gp.value("R", gs.hypo.R);
            gs.hypo.r       = gp.value("r", gs.hypo.r);
            gs.hypo.d       = gp.value("d", gs.hypo.d);
            gs.hypo.samples = gp.value("samples", gs.hypo.samples);
            break;
        case GeneratorType::Epitrochoid:
            gs.epi.R       = gp.value("R", gs.epi.R);
            gs.epi.r       = gp.value("r", gs.epi.r);
            gs.epi.d       = gp.value("d", gs.epi.d);
            gs.epi.samples = gp.value("samples", gs.epi.samples);
            break;
        case GeneratorType::Lissajous:
            gs.liss.A       = gp.value("A", gs.liss.A);
            gs.liss.B       = gp.value("B", gs.liss.B);
            gs.liss.a       = gp.value("a", gs.liss.a);
            gs.liss.b       = gp.value("b", gs.liss.b);
            gs.liss.phi     = gp.value("phi", gs.liss.phi);
            gs.liss.samples = gp.value("samples", gs.liss.samples);
            break;
        case GeneratorType::Rose:
            gs.rose.n       = gp.value("n",       gs.rose.n);
            gs.rose.d       = gp.value("d",       gs.rose.d);
            gs.rose.samples = gp.value("samples", gs.rose.samples);
            break;
        case GeneratorType::Superformula:
            gs.supf.m       = gp.value("m",       gs.supf.m);
            gs.supf.n1      = gp.value("n1",      gs.supf.n1);
            gs.supf.n2      = gp.value("n2",      gs.supf.n2);
            gs.supf.n3      = gp.value("n3",      gs.supf.n3);
            gs.supf.a       = gp.value("a",       gs.supf.a);
            gs.supf.b       = gp.value("b",       gs.supf.b);
            gs.supf.samples = gp.value("samples", gs.supf.samples);
            break;
        case GeneratorType::Phyllotaxis:
            gs.phyl.N     = gp.value("N",     gs.phyl.N);
            gs.phyl.alpha = gp.value("alpha", gs.phyl.alpha);
            gs.phyl.k     = gp.value("k",     gs.phyl.k);
            break;
        case GeneratorType::PolygonChord:
            gs.poly.n_sides      = gp.value("n_sides",      gs.poly.n_sides);
            gs.poly.N            = gp.value("N",            gs.poly.N);
            gs.poly.k            = gp.value("k",            gs.poly.k);
            gs.poly.rotation_rad = gp.value("rotation_rad", gs.poly.rotation_rad);
            break;
        case GeneratorType::LinearEnvelope: {
            auto read_vec2 = [&](const char* key, Vec2 fallback) -> Vec2 {
                if (gp.contains(key) && gp.at(key).is_array() && gp.at(key).size() == 2) {
                    return {gp.at(key).at(0).get<double>(), gp.at(key).at(1).get<double>()};
                }
                return fallback;
            };
            gs.lenv.a_start = read_vec2("a_start", gs.lenv.a_start);
            gs.lenv.a_end   = read_vec2("a_end",   gs.lenv.a_end);
            gs.lenv.b_start = read_vec2("b_start", gs.lenv.b_start);
            gs.lenv.b_end   = read_vec2("b_end",   gs.lenv.b_end);
            gs.lenv.N       = gp.value("N", gs.lenv.N);
            gs.lenv.k       = gp.value("k", gs.lenv.k);
            break;
        }
    }
}

inline void style_to_json(nlohmann::json& s, const StyleSpec& ss) {
    auto& cm = s["color_map"] = nlohmann::json::object();
    cm["type"] = colormap_type_to_string(ss.colormap_type);
    switch (ss.colormap_type) {
        case ColorMapType::Solid:
            cm["color"] = color_to_hex(ss.solid_color);
            break;
        case ColorMapType::LinearGradient:
            cm["start"] = color_to_hex(ss.gradient_start);
            cm["end"]   = color_to_hex(ss.gradient_end);
            break;
        case ColorMapType::HsvSweep:
            cm["hue_start"]  = ss.hue_start;
            cm["hue_end"]    = ss.hue_end;
            cm["saturation"] = ss.hsv_saturation;
            cm["value"]      = ss.hsv_value;
            break;
        case ColorMapType::Diverging:
            cm["negative"] = color_to_hex(ss.div_negative);
            cm["midpoint"] = color_to_hex(ss.div_midpoint);
            cm["positive"] = color_to_hex(ss.div_positive);
            break;
    }
    s["color_indexer"] = indexer_to_string(ss.color_indexer);
    auto& stroke = s["stroke"] = nlohmann::json::object();
    stroke["width_min"]     = ss.stroke_width_min;
    stroke["width_max"]     = ss.stroke_width_max;
    stroke["width_indexer"] = indexer_to_string(ss.stroke_width_indexer);
    stroke["opacity"]       = ss.opacity;
    s["cyclic"] = ss.cyclic;
}

inline void style_from_json(const nlohmann::json& s, StyleSpec& ss) {
    const auto& cm = s.at("color_map");
    ss.colormap_type = colormap_type_from_string(cm.at("type").get<std::string>());
    switch (ss.colormap_type) {
        case ColorMapType::Solid:
            ss.solid_color = color_from_hex(cm.at("color").get<std::string>());
            break;
        case ColorMapType::LinearGradient:
            ss.gradient_start = color_from_hex(cm.at("start").get<std::string>());
            ss.gradient_end   = color_from_hex(cm.at("end").get<std::string>());
            break;
        case ColorMapType::HsvSweep:
            ss.hue_start      = cm.value("hue_start", ss.hue_start);
            ss.hue_end        = cm.value("hue_end",   ss.hue_end);
            ss.hsv_saturation = cm.value("saturation", ss.hsv_saturation);
            ss.hsv_value      = cm.value("value",      ss.hsv_value);
            break;
        case ColorMapType::Diverging:
            ss.div_negative = color_from_hex(cm.at("negative").get<std::string>());
            ss.div_midpoint = color_from_hex(cm.at("midpoint").get<std::string>());
            ss.div_positive = color_from_hex(cm.at("positive").get<std::string>());
            break;
    }
    ss.color_indexer = indexer_from_string(s.at("color_indexer").get<std::string>());
    const auto& stroke = s.at("stroke");
    ss.stroke_width_min = stroke.value("width_min", ss.stroke_width_min);
    ss.stroke_width_max = stroke.value("width_max", ss.stroke_width_max);
    ss.stroke_width_indexer = indexer_from_string(
        stroke.value("width_indexer", std::string("by_chord_index")));
    ss.opacity = stroke.value("opacity", ss.opacity);
    ss.cyclic = s.value("cyclic", ss.cyclic);
}

inline void transform_to_json(nlohmann::json& j, const LayerTransform& t) {
    j["translate"] = nlohmann::json::array({t.translate.x, t.translate.y});
    j["rotate_rad"] = t.rotate_rad;
    j["scale"] = t.scale;
    j["mirror_x"] = t.mirror_x;
    j["mirror_y"] = t.mirror_y;
}

inline void transform_from_json(const nlohmann::json& j, LayerTransform& t) {
    if (j.contains("translate")) {
        const auto& tr = j.at("translate");
        if (tr.is_array() && tr.size() == 2) {
            t.translate.x = tr.at(0).get<double>();
            t.translate.y = tr.at(1).get<double>();
        }
    }
    t.rotate_rad = j.value("rotate_rad", 0.0);
    t.scale      = j.value("scale", 1.0);
    t.mirror_x   = j.value("mirror_x", false);
    t.mirror_y   = j.value("mirror_y", false);
}

inline void layer_to_json(nlohmann::json& j, const Layer& l) {
    j["name"] = l.name;
    generator_to_json(j["generator"] = nlohmann::json::object(), l.generator);
    style_to_json(j["style"] = nlohmann::json::object(), l.style);
    transform_to_json(j["transform"] = nlohmann::json::object(), l.transform);
    j["visible"] = l.visible;
}

inline void layer_from_json(const nlohmann::json& j, Layer& l) {
    l.name = j.value("name", std::string{});
    generator_from_json(j.at("generator"), l.generator);
    style_from_json(j.at("style"), l.style);
    if (j.contains("transform")) transform_from_json(j.at("transform"), l.transform);
    l.visible = j.value("visible", true);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// nlohmann/json ADL hooks for Preset

inline void to_json(nlohmann::json& j, const Preset& p) {
    j = nlohmann::json::object();
    j["version"] = p.version;
    j["name"] = p.name;

    auto& sc = j["scene"] = nlohmann::json::object();
    sc["background"] = detail::color_to_hex(p.scene.background);
    auto& layers = sc["layers"] = nlohmann::json::array();
    for (const auto& l : p.scene.layers) {
        nlohmann::json lj = nlohmann::json::object();
        detail::layer_to_json(lj, l);
        layers.push_back(std::move(lj));
    }

    auto& c = j["camera"] = nlohmann::json::object();
    c["pan_x_px"] = p.camera.pan_x_px;
    c["pan_y_px"] = p.camera.pan_y_px;
    c["zoom"]     = p.camera.zoom;
}

inline void from_json(const nlohmann::json& j, Preset& p) {
    p.version = j.at("version").get<int>();
    if (p.version != 1 && p.version != 2) {
        throw std::runtime_error("unsupported preset version " + std::to_string(p.version));
    }
    p.name = j.value("name", std::string{});
    p.scene.layers.clear();

    if (p.version == 1) {
        // v1 → v2 auto-promote: the top-level generator + style become a single
        // layer with identity transform. v1's style.background moves to
        // scene.background.
        Layer layer;
        layer.name = "layer 0";
        detail::generator_from_json(j.at("generator"), layer.generator);
        const auto& s = j.at("style");
        detail::style_from_json(s, layer.style);
        p.scene.background = detail::color_from_hex(
            s.value("background", std::string("#0a0a0a")));
        p.scene.layers.push_back(std::move(layer));
        p.version = 2;  // upgrade in memory
    } else {
        // v2 native
        const auto& sc = j.at("scene");
        p.scene.background = detail::color_from_hex(
            sc.value("background", std::string("#0a0a0a")));
        if (sc.contains("layers")) {
            for (const auto& lj : sc.at("layers")) {
                Layer l;
                detail::layer_from_json(lj, l);
                p.scene.layers.push_back(std::move(l));
            }
        }
    }

    if (p.scene.layers.empty()) {
        // Always at least one layer in memory so the UI has something to edit.
        p.scene.layers.push_back(Layer{});
        p.scene.layers.back().name = "layer 0";
    }

    if (j.contains("camera")) {
        const auto& c = j.at("camera");
        p.camera.pan_x_px = c.value("pan_x_px", 0.0);
        p.camera.pan_y_px = c.value("pan_y_px", 0.0);
        p.camera.zoom     = c.value("zoom", 1.0);
    }
}

// ---------------------------------------------------------------------------
// File I/O + XDG path

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
