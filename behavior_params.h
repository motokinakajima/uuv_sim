#ifndef UUV_SIM_BEHAVIOR_PARAMS_H
#define UUV_SIM_BEHAVIOR_PARAMS_H

#include <string>

struct BehaviorParams {
    double max_velocity = 5.0;

    double avoidance_radius = 20.0;
    double avoidance_gain = 2.0;

    double quark_gain = 0.6;
    double quark_max_force = 1.4;
    double quark_saturation_distance = 20.0;

    double directional_derivative_gain = 3.5;
    double directional_eps = 1e-6;

    double linear_drag_gain = 0.25;
};

bool load_behavior_params(const std::string& filename,
                          BehaviorParams& out_params,
                          std::string* error_message = nullptr);

#endif // UUV_SIM_BEHAVIOR_PARAMS_H
