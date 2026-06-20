#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/scene_render.hpp>

#include "plotter_renderer.hpp"
#include "svg_renderer.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kVersion = "caustic-cli 0.1.0";

// Output formats — picked by --format, or inferred from the -o extension if
// --format is omitted. The shared `margin` knob applies to all three.
enum class OutputFormat { Svg, Gcode, Hpgl };

void print_help() {
    std::cout <<
        "Usage: caustic-cli [OPTIONS] PRESET_FILE -o OUTPUT\n"
        "\n"
        "Required:\n"
        "  PRESET_FILE                Path to a preset JSON file\n"
        "  -o, --output FILE          Output file path. Format inferred from the\n"
        "                             extension (.svg / .gcode / .nc / .gc /\n"
        "                             .hpgl / .plt) unless --format is set.\n"
        "\n"
        "Format selection:\n"
        "  --format FMT               One of: svg, gcode, hpgl. Overrides the\n"
        "                             extension inference.\n"
        "\n"
        "SVG options:\n"
        "  --width FLOAT              Output viewBox width (default 1024)\n"
        "  --height FLOAT             Output viewBox height (default 1024)\n"
        "  --plotter                  SVG plotter mode: single colour, no opacity,\n"
        "                             chord set sorted, polylines as <polyline>\n"
        "  --simplify EPS             Douglas-Peucker tolerance (off by default;\n"
        "                             currently plumbed through but not applied)\n"
        "\n"
        "Plotter (G-code / HPGL) options:\n"
        "  --page-width-mm FLOAT      Page width in mm (default 200)\n"
        "  --page-height-mm FLOAT     Page height in mm (default 200)\n"
        "  --pen-up-z FLOAT           G-code Z height with pen lifted (default 5)\n"
        "  --pen-down-z FLOAT         G-code Z height with pen touching (default 0)\n"
        "  --travel-feedrate FLOAT    G-code rapid feedrate, mm/min (default 6000)\n"
        "  --draw-feedrate FLOAT      G-code drawing feedrate, mm/min (default 3000)\n"
        "  --plunge-feedrate FLOAT    G-code Z-axis feedrate, mm/min (default 1500)\n"
        "  --pen-number INT           HPGL pen carousel slot (default 1)\n"
        "\n"
        "Shared:\n"
        "  --margin FLOAT             Margin as fraction of page/canvas (default 0.05)\n"
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
    OutputFormat format = OutputFormat::Svg;
    bool format_explicit = false;  // true if --format was passed; controls extension inference

    // SVG knobs
    double width        = 1024.0;
    double height       = 1024.0;
    bool   plotter      = false;
    double simplify_eps = 0.0;

    // Shared
    double margin = 0.05;

    // Plotter knobs
    double page_width_mm    =  200.0;
    double page_height_mm   =  200.0;
    double pen_up_z         =    5.0;
    double pen_down_z       =    0.0;
    double travel_feedrate  = 6000.0;
    double draw_feedrate    = 3000.0;
    double plunge_feedrate  = 1500.0;
    int    pen_number       =    1;
};

bool parse_format(std::string_view s, OutputFormat& out) {
    if (s == "svg")   { out = OutputFormat::Svg;   return true; }
    if (s == "gcode") { out = OutputFormat::Gcode; return true; }
    if (s == "hpgl")  { out = OutputFormat::Hpgl;  return true; }
    return false;
}

// Map output-path extension → format. Returns true when an inference fired.
bool infer_format_from_extension(const std::string& path, OutputFormat& out) {
    const std::string ext = fs::path(path).extension().string();
    if (ext == ".svg")  { out = OutputFormat::Svg;   return true; }
    if (ext == ".gcode" || ext == ".nc" || ext == ".gc")
                        { out = OutputFormat::Gcode; return true; }
    if (ext == ".hpgl" || ext == ".plt")
                        { out = OutputFormat::Hpgl;  return true; }
    return false;
}

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

    auto need_int = [&](int& i, int& target, std::string_view name) -> bool {
        if (++i >= argc) {
            std::cerr << "missing argument to " << name << "\n";
            return false;
        }
        try {
            target = std::stoi(argv[i]);
            return true;
        } catch (const std::exception&) {
            std::cerr << "invalid integer for " << name << ": " << argv[i] << "\n";
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
        if (a == "--format") {
            if (++i >= argc) { std::cerr << "missing argument to --format\n"; return 1; }
            if (!parse_format(argv[i], out.format)) {
                std::cerr << "unknown --format: " << argv[i]
                          << " (expected svg, gcode, or hpgl)\n";
                return 1;
            }
            out.format_explicit = true;
            continue;
        }
        // SVG knobs
        if (a == "--width")    { if (!need_double(i, out.width,        a)) return 1; continue; }
        if (a == "--height")   { if (!need_double(i, out.height,       a)) return 1; continue; }
        if (a == "--simplify") { if (!need_double(i, out.simplify_eps, a)) return 1; continue; }
        if (a == "--plotter")  { out.plotter = true; continue; }
        // Shared
        if (a == "--margin")   { if (!need_double(i, out.margin,       a)) return 1; continue; }
        // Plotter knobs
        if (a == "--page-width-mm")   { if (!need_double(i, out.page_width_mm,   a)) return 1; continue; }
        if (a == "--page-height-mm")  { if (!need_double(i, out.page_height_mm,  a)) return 1; continue; }
        if (a == "--pen-up-z")        { if (!need_double(i, out.pen_up_z,        a)) return 1; continue; }
        if (a == "--pen-down-z")      { if (!need_double(i, out.pen_down_z,      a)) return 1; continue; }
        if (a == "--travel-feedrate") { if (!need_double(i, out.travel_feedrate, a)) return 1; continue; }
        if (a == "--draw-feedrate")   { if (!need_double(i, out.draw_feedrate,   a)) return 1; continue; }
        if (a == "--plunge-feedrate") { if (!need_double(i, out.plunge_feedrate, a)) return 1; continue; }
        if (a == "--pen-number")      { if (!need_int   (i, out.pen_number,      a)) return 1; continue; }

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
    // If --format wasn't passed, infer from the output extension. Silent
    // fallback to SVG if the extension isn't one we recognise — preserves
    // existing behaviour where unrecognised extensions (e.g. .out, .svg.tmp)
    // wrote SVG.
    if (!out.format_explicit && !out.output_path.empty()) {
        OutputFormat inferred = OutputFormat::Svg;
        if (infer_format_from_extension(out.output_path, inferred)) {
            out.format = inferred;
        }
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
        switch (args.format) {
            case OutputFormat::Svg: {
                caustic::SvgOptions opts;
                opts.width            = args.width;
                opts.height           = args.height;
                opts.margin           = args.margin;
                opts.plotter_mode     = args.plotter;
                opts.simplify_epsilon = args.simplify_eps;
                caustic::write_svg(args.output_path, layers,
                                   preset.scene.background, opts);
                break;
            }
            case OutputFormat::Gcode: {
                caustic::PlotterOptions opts;
                opts.width_mm        = args.page_width_mm;
                opts.height_mm       = args.page_height_mm;
                opts.margin          = args.margin;
                opts.pen_up_z        = args.pen_up_z;
                opts.pen_down_z      = args.pen_down_z;
                opts.travel_feedrate = args.travel_feedrate;
                opts.draw_feedrate   = args.draw_feedrate;
                opts.plunge_feedrate = args.plunge_feedrate;
                caustic::write_gcode(args.output_path, layers, opts);
                break;
            }
            case OutputFormat::Hpgl: {
                caustic::PlotterOptions opts;
                opts.width_mm   = args.page_width_mm;
                opts.height_mm  = args.page_height_mm;
                opts.margin     = args.margin;
                opts.pen_number = args.pen_number;
                caustic::write_hpgl(args.output_path, layers, opts);
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "output write failed: " << e.what() << "\n";
        return 4;
    }

    return 0;
}
