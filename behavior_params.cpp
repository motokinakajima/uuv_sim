#include "behavior_params.h"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
std::string trim(const std::string& input) {
    size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
        begin++;
    }

    size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        end--;
    }

    return input.substr(begin, end - begin);
}

bool apply_param(const std::string& key, double value, BehaviorParams& out) {
    if (key == "max_velocity") {
        out.max_velocity = value;
    } else if (key == "avoidance_radius") {
        out.avoidance_radius = value;
    } else if (key == "avoidance_gain") {
        out.avoidance_gain = value;
    } else if (key == "quark_gain") {
        out.quark_gain = value;
    } else if (key == "quark_max_force") {
        out.quark_max_force = value;
    } else if (key == "quark_saturation_distance") {
        out.quark_saturation_distance = value;
    } else if (key == "directional_derivative_gain") {
        out.directional_derivative_gain = value;
    } else if (key == "directional_eps") {
        out.directional_eps = value;
    } else if (key == "linear_drag_gain") {
        out.linear_drag_gain = value;
    } else {
        return false;
    }

    return true;
}
} // namespace

bool load_behavior_params(const std::string& filename,
                          BehaviorParams& out_params,
                          std::string* error_message) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        if (error_message != nullptr) {
            *error_message = "Could not open parameter file: " + filename;
        }
        return false;
    }

    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;

        std::string content = trim(line);
        if (content.empty() || content[0] == '#') {
            continue;
        }

        size_t separator = content.find('=');
        if (separator == std::string::npos) {
            if (error_message != nullptr) {
                *error_message = "Invalid line format at line " + std::to_string(line_number);
            }
            return false;
        }

        std::string key = trim(content.substr(0, separator));
        std::string value_text = trim(content.substr(separator + 1));

        try {
            double value = std::stod(value_text);
            if (!apply_param(key, value, out_params)) {
                if (error_message != nullptr) {
                    *error_message = "Unknown parameter '" + key + "' at line " + std::to_string(line_number);
                }
                return false;
            }
        } catch (const std::invalid_argument&) {
            if (error_message != nullptr) {
                *error_message = "Invalid numeric value for '" + key + "' at line " + std::to_string(line_number);
            }
            return false;
        } catch (const std::out_of_range&) {
            if (error_message != nullptr) {
                *error_message = "Out-of-range value for '" + key + "' at line " + std::to_string(line_number);
            }
            return false;
        }
    }

    return true;
}
