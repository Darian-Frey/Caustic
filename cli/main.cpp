#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/scene_render.hpp>

#include "svg_renderer.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kVersion = "caustic-cli 0.1.0";

void print_help() {
    std::cout <<
        "Usage: caustic-cli [OPTIONS] PRESET_FILE -o OUTPUT.svg\n"
        "\n"
        "Required:\n"
        "  PRESET_FILE                Path to a preset JSON file\n"
        "  -o, --output FILE          Output SVG path\n"
        "\n"
        "Options:\n"
        "  --width FLOAT              Output viewBox width (default 1024)\n"
        "  --height FLOAT             Output viewBox height (default 1024)\n"
        "  --margin FLOAT             Margin as fraction of canvas (default 0.05)\n"
        "  --plotter                  Plotter mode: single colour, no opacity,\n"
        "                             chord set sorted, polylines as <polyline>\n"
        "  --simplify EPS             Douglas-Peucker tolerance (off by default;\n"
        "                             currently plumbed through but not applied)\n"
        "  -h, --help                 Show this message\n"
        "  -V, --version              Show version\n"
        "\n"
        "Exit codes (SPEC.md \xc2\xa7" "5):\n"
        "  0  success\n"
        "  1  generic error / bad arguments\n"
        "  2  preset file not found or unreadable\n"
        "  3  preset validation failed\n"
        "  4  output write failed\n";
}

struct Args {
    std::string preset_path;
    std::string output_path;
    double width = 1024.0;
    double height = 1024.0;
    double margin = 0.05;
    bool plotter = false;
    double simplify_eps = 0.0;
};

// Return values: 0 = ok, 1 = error, -1 = stop (help/version already printed).
int parse_args(int argc, char** argv, Args& out) {
    auto need_double = [&](int& i, double& target, std::string_view name) -> bool {
        if (++i >= argc) {
            std::cerr << "missing argument to " << name << "\n";
            return false;
        }
        try {
            target = std::stod(argv[i]);
            return true;
        } catch (const std::exception&) {
            std::cerr << "invalid number for " << name << ": " << argv[i] << "\n";
            return false;
        }
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view a = argv[i];

        if (a == "-h" || a == "--help")    { print_help(); return -1; }
        if (a == "-V" || a == "--version") { std::cout << kVersion << "\n"; return -1; }

        if (a == "-o" || a == "--output") {
            if (++i >= argc) { std::cerr << "missing argument to " << a << "\n"; return 1; }
            out.output_path = argv[i];
            continue;
        }
        if (a == "--width")    { if (!need_double(i, out.width,        a)) return 1; continue; }
        if (a == "--height")   { if (!need_double(i, out.height,       a)) return 1; continue; }
        if (a == "--margin")   { if (!need_double(i, out.margin,       a)) return 1; continue; }
        if (a == "--simplify") { if (!need_double(i, out.simplify_eps, a)) return 1; continue; }
        if (a == "--plotter")  { out.plotter = true; continue; }

        if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option: " << a << "\n";
            return 1;
        }

        // Positional: first non-flag is the preset path.
        if (out.preset_path.empty()) {
            out.preset_path = argv[i];
            continue;
        }
        std::cerr << "unexpected positional argument: " << a << "\n";
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    const int parse_result = parse_args(argc, argv, args);
    if (parse_result == -1) return 0;
    if (parse_result != 0)  return parse_result;

    if (args.preset_path.empty()) {
        std::cerr << "no preset file given (use --help for usage)\n";
        return 1;
    }
    if (args.output_path.empty()) {
        std::cerr << "no output path given (use -o OUTPUT.svg)\n";
        return 1;
    }

    if (!fs::exists(args.preset_path)) {
        std::cerr << "preset file not found: " << args.preset_path << "\n";
        return 2;
    }

    caustic::Preset preset;
    try {
        preset = caustic::load_preset(args.preset_path);
    } catch (const std::exception& e) {
        std::cerr << "preset validation failed: " << e.what() << "\n";
        return 3;
    }

    try {
        const auto layers = caustic::build_renderables(preset.scene);
        caustic::SvgOptions opts;
        opts.width            = args.width;
        opts.height           = args.height;
        opts.margin           = args.margin;
        opts.plotter_mode     = args.plotter;
        opts.simplify_epsilon = args.simplify_eps;
        caustic::write_svg(args.output_path, layers, preset.scene.background, opts);
    } catch (const std::exception& e) {
        std::cerr << "output write failed: " << e.what() << "\n";
        return 4;
    }

    return 0;
}
