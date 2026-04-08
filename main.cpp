#include <iostream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "event_controller.h"
#include "agent.h"
#include "field.h"
#include "data_logger.h"
#include "behavior_params.h"

namespace {

struct BatchOptions {
    int agent_count = 3;
    int max_steps = 2500;
    int window = 150;
    int hold_steps = 150;
    int num_gaussians = 20;
    unsigned int seed = 1;
    unsigned int field_seed = 20260407;
    double eps_v = 0.03;
    double eps_f = 5e-4;
    float init_pos_range = 20.0f;
    float init_vel_range = 1.0f;
};

struct BatchSummary {
    int steps_executed = 0;
    int converged = 0;
    int converge_step = -1;
    int agent_count = 0;
    unsigned int seed = 0;
    unsigned int field_seed = 0;
    int max_steps = 0;
    double best_field_final = 0.0;
    double mean_field_final = 0.0;
    double best_field_min_over_run = 0.0;
    double improvement_from_start = 0.0;
    double swarm_radius_final = 0.0;
    double avg_speed_final = 0.0;
    double runtime_ms = 0.0;
};

std::unordered_map<std::string, std::string> parse_args(int argc, char** argv) {
    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key.rfind("--", 0) != 0) {
            continue;
        }

        std::string value = "true";
        if (i + 1 < argc) {
            std::string maybe_value = argv[i + 1];
            if (maybe_value.rfind("--", 0) != 0) {
                value = maybe_value;
                ++i;
            }
        }
        args[key] = value;
    }
    return args;
}

bool has_flag(const std::unordered_map<std::string, std::string>& args, const std::string& key) {
    return args.find(key) != args.end();
}

int parse_int_or_default(const std::unordered_map<std::string, std::string>& args,
                         const std::string& key,
                         int default_value) {
    auto it = args.find(key);
    if (it == args.end()) {
        return default_value;
    }
    return std::stoi(it->second);
}

unsigned int parse_uint_or_default(const std::unordered_map<std::string, std::string>& args,
                                   const std::string& key,
                                   unsigned int default_value) {
    auto it = args.find(key);
    if (it == args.end()) {
        return default_value;
    }
    return static_cast<unsigned int>(std::stoul(it->second));
}

double parse_double_or_default(const std::unordered_map<std::string, std::string>& args,
                               const std::string& key,
                               double default_value) {
    auto it = args.find(key);
    if (it == args.end()) {
        return default_value;
    }
    return std::stod(it->second);
}

float parse_float_or_default(const std::unordered_map<std::string, std::string>& args,
                             const std::string& key,
                             float default_value) {
    auto it = args.find(key);
    if (it == args.end()) {
        return default_value;
    }
    return std::stof(it->second);
}

BatchOptions options_from_args(const std::unordered_map<std::string, std::string>& args) {
    BatchOptions opt;
    opt.agent_count = parse_int_or_default(args, "--agent-count", opt.agent_count);
    opt.max_steps = parse_int_or_default(args, "--max-steps", opt.max_steps);
    opt.window = parse_int_or_default(args, "--window", opt.window);
    opt.hold_steps = parse_int_or_default(args, "--hold", opt.hold_steps);
    opt.num_gaussians = parse_int_or_default(args, "--num-gaussians", opt.num_gaussians);
    opt.seed = parse_uint_or_default(args, "--seed", opt.seed);
    opt.field_seed = parse_uint_or_default(args, "--field-seed", opt.field_seed);
    opt.eps_v = parse_double_or_default(args, "--eps-v", opt.eps_v);
    opt.eps_f = parse_double_or_default(args, "--eps-f", opt.eps_f);
    opt.init_pos_range = parse_float_or_default(args, "--init-pos-range", opt.init_pos_range);
    opt.init_vel_range = parse_float_or_default(args, "--init-vel-range", opt.init_vel_range);

    opt.agent_count = std::max(1, opt.agent_count);
    opt.max_steps = std::max(1, opt.max_steps);
    opt.window = std::max(1, opt.window);
    opt.hold_steps = std::max(1, opt.hold_steps);
    opt.num_gaussians = std::max(1, opt.num_gaussians);

    return opt;
}

double compute_swarm_radius(const std::vector<Agent*>& agents) {
    if (agents.empty()) {
        return 0.0;
    }

    double cx = 0.0;
    double cy = 0.0;
    for (Agent* agent : agents) {
        Pos2 pos = agent->get_position();
        cx += pos.getX();
        cy += pos.getY();
    }
    cx /= static_cast<double>(agents.size());
    cy /= static_cast<double>(agents.size());

    double max_dist = 0.0;
    for (Agent* agent : agents) {
        Pos2 pos = agent->get_position();
        double dx = static_cast<double>(pos.getX()) - cx;
        double dy = static_cast<double>(pos.getY()) - cy;
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist > max_dist) {
            max_dist = dist;
        }
    }

    return max_dist;
}

BatchSummary run_batch_experiment(const BatchOptions& opt) {
    auto run_start = std::chrono::steady_clock::now();

    EventController controller;
    Field field(opt.num_gaussians, opt.field_seed);
    controller.field = &field;

    std::mt19937 init_rng(opt.seed);
    std::uniform_real_distribution<float> pos_dist(-opt.init_pos_range, opt.init_pos_range);
    std::uniform_real_distribution<float> vel_dist(-opt.init_vel_range, opt.init_vel_range);

    std::vector<std::unique_ptr<Agent>> agent_storage;
    agent_storage.reserve(opt.agent_count);

    for (int i = 0; i < opt.agent_count; ++i) {
        Pos2 pos(pos_dist(init_rng), pos_dist(init_rng));
        Vec2 vel(vel_dist(init_rng), vel_dist(init_rng));
        agent_storage.push_back(std::make_unique<Agent>(i + 1, pos, vel));
        controller.add_agent(agent_storage.back().get());
    }

    double best_start = std::numeric_limits<double>::infinity();
    for (Agent* agent : controller.agents) {
        double fv = field.get_scalar(agent->get_position());
        if (fv < best_start) {
            best_start = fv;
        }
    }

    double best_min = best_start;
    double best_now = best_start;
    double mean_now = 0.0;
    double avg_speed_now = 0.0;
    std::deque<double> best_history;
    best_history.push_back(best_start);

    int hold_counter = 0;
    int steps = 0;
    int converged = 0;
    int converge_step = -1;

    for (steps = 1; steps <= opt.max_steps; ++steps) {
        controller.step();

        double field_sum = 0.0;
        double speed_sum = 0.0;
        best_now = std::numeric_limits<double>::infinity();

        for (Agent* agent : controller.agents) {
            double fv = field.get_scalar(agent->get_position());
            field_sum += fv;
            if (fv < best_now) {
                best_now = fv;
            }

            Vec2 vel = agent->get_current_velocity();
            speed_sum += vel.len();
        }

        mean_now = field_sum / static_cast<double>(controller.agents.size());
        avg_speed_now = speed_sum / static_cast<double>(controller.agents.size());
        if (best_now < best_min) {
            best_min = best_now;
        }

        best_history.push_back(best_now);
        if (best_history.size() > static_cast<size_t>(opt.window + 1)) {
            best_history.pop_front();
        }

        bool slow_enough = avg_speed_now < opt.eps_v;
        bool stable_best = false;
        if (best_history.size() == static_cast<size_t>(opt.window + 1)) {
            stable_best = std::abs(best_now - best_history.front()) < opt.eps_f;
        }

        if (slow_enough && stable_best) {
            hold_counter++;
        } else {
            hold_counter = 0;
        }

        if (hold_counter >= opt.hold_steps) {
            converged = 1;
            converge_step = steps;
            break;
        }
    }

    auto run_end = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed_ms = run_end - run_start;

    BatchSummary summary;
    summary.steps_executed = std::min(steps, opt.max_steps);
    summary.converged = converged;
    summary.converge_step = converge_step;
    summary.agent_count = opt.agent_count;
    summary.seed = opt.seed;
    summary.field_seed = opt.field_seed;
    summary.max_steps = opt.max_steps;
    summary.best_field_final = best_now;
    summary.mean_field_final = mean_now;
    summary.best_field_min_over_run = best_min;
    summary.improvement_from_start = best_start - best_now;
    summary.swarm_radius_final = compute_swarm_radius(controller.agents);
    summary.avg_speed_final = avg_speed_now;
    summary.runtime_ms = elapsed_ms.count();
    return summary;
}

void print_summary_json(const BatchSummary& s) {
    std::cout
        << "{"
        << "\"agent_count\":" << s.agent_count << ","
        << "\"seed\":" << s.seed << ","
        << "\"field_seed\":" << s.field_seed << ","
        << "\"max_steps\":" << s.max_steps << ","
        << "\"steps_executed\":" << s.steps_executed << ","
        << "\"converged\":" << s.converged << ","
        << "\"converge_step\":" << s.converge_step << ","
        << "\"best_field_final\":" << s.best_field_final << ","
        << "\"mean_field_final\":" << s.mean_field_final << ","
        << "\"best_field_min_over_run\":" << s.best_field_min_over_run << ","
        << "\"improvement_from_start\":" << s.improvement_from_start << ","
        << "\"swarm_radius_final\":" << s.swarm_radius_final << ","
        << "\"avg_speed_final\":" << s.avg_speed_final << ","
        << "\"runtime_ms\":" << s.runtime_ms
        << "}"
        << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    const auto args = parse_args(argc, argv);
    const bool batch_mode = has_flag(args, "--batch-run");

    BehaviorParams params;
    std::string param_error;
    if (!load_behavior_params("behavior_params.cfg", params, &param_error)) {
        if (!batch_mode) {
            std::cout << "Using default behavior params: " << param_error << std::endl;
        }
    } else if (!batch_mode) {
        std::cout << "Loaded behavior_params.cfg" << std::endl;
    }

    Agent::set_behavior_params(params);

    if (batch_mode) {
        BatchOptions opt = options_from_args(args);
        BatchSummary summary = run_batch_experiment(opt);
        print_summary_json(summary);
        return 0;
    }

    EventController controller;
    
    // Create a field with 5 Gaussian peaks/valleys
    Field field(20);
    controller.field = &field;
    
    Agent agent1(1, Pos2(10.0f, 10.0f), Vec2(1.0f, 0.5f));
    Agent agent2(2, Pos2(15.0f, 12.0f), Vec2(-0.5f, 1.0f));
    Agent agent3(3, Pos2(8.0f, 15.0f), Vec2(0.8f, -0.3f));
    
    controller.add_agent(&agent1);
    controller.add_agent(&agent2);
    controller.add_agent(&agent3);

    std::cout << "Starting simulation..." << std::endl;

    controller.run_with_logger("simulation_data.json");
    
    std::cout << "Simulation complete! Data saved to simulation_data.json" << std::endl;
    return 0;
}
