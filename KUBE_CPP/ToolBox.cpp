#include "ToolBox.h"
#include <fstream>
#include <regex>
#include <iostream>
#include <cstdlib>

// ---- Config Reading Methods ------------------------------------------------

int ToolBox::read_int_value(const std::string& path, const std::string& param_name) {
    std::ifstream in(path);
    if (!in) {
        die("Config nicht gefunden: " + path);
    }

    std::string all(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    // Build regex pattern: param_name followed by optional whitespace, '=', optional whitespace, and digits
    // Example: "c_fitness_lookback_EAs\s*=\s*([0-9]+)"
    std::string pattern = param_name + R"(\s*=\s*([0-9]+))";
    std::regex re(pattern);
    std::smatch m;

    if (std::regex_search(all, m, re)) {
        return std::stoi(m[1].str());
    }

    die(param_name + " nicht in Config gefunden.");
    return 0;
}

double ToolBox::read_double_value(const std::string& path, const std::string& param_name) {
    std::ifstream in(path);
    if (!in) {
        die("Config nicht gefunden: " + path);
    }

    std::string all(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    // Build regex pattern for double: param_name followed by '=' and a floating point number
    // Example: "c_lambda\s*=\s*([0-9]+\.?[0-9]*)"
    std::string pattern = param_name + R"(\s*=\s*([0-9]+\.?[0-9]*))";
    std::regex re(pattern);
    std::smatch m;

    if (std::regex_search(all, m, re)) {
        return std::stod(m[1].str());
    }

    die(param_name + " nicht in Config gefunden.");
    return 0.0;
}

std::string ToolBox::read_string_value(const std::string& path, const std::string& param_name) {
    std::ifstream in(path);
    if (!in) {
        die("Config nicht gefunden: " + path);
    }

    std::string all(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    // Build regex pattern for string: param_name followed by '=' and a quoted string
    // Example: "c_some_string\s*=\s*\"([^\"]*)\""
    std::string pattern = param_name + R"(\s*=\s*\"([^\"]*)\")";
    std::regex re(pattern);
    std::smatch m;

    if (std::regex_search(all, m, re)) {
        return m[1].str();
    }

    die(param_name + " nicht in Config gefunden.");
    return "";
}

std::string ToolBox::default_config_path() {
    return "C:\\MQL_Shared_Restore\\Includes\\Config1\\KUBE_config.mqh";
}

// ---- Helper Methods --------------------------------------------------------

void ToolBox::die(const std::string& msg, int rc) {
    std::fprintf(stderr, "[ToolBox FATAL] %s\n", msg.c_str());
    std::exit(rc);
}
