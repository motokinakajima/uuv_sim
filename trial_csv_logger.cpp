#include "trial_csv_logger.h"

#include <iomanip>
#include <stdexcept>
#include <utility>

namespace {
void prepare_stream(std::ofstream& stream) {
    stream.setf(std::ios::fmtflags(0), std::ios::floatfield);
    stream << std::setprecision(10);
}
} // namespace

TrialCsvLogger::TrialCsvLogger(std::string prefix)
    : prefix(std::move(prefix)) {
}

int TrialCsvLogger::action_to_int(GainAction action) {
    return static_cast<int>(action);
}

void TrialCsvLogger::start(const Field& field,
                           int agent_count,
                           unsigned int seed,
                           unsigned int field_seed,
                           int max_steps,
                           int num_gaussians) {
    if (started) {
        return;
    }

    metadata_file.open(prefix + "_metadata.csv");
    field_file.open(prefix + "_field_gaussians.csv");
    step_file.open(prefix + "_step_summary.csv");
    agent_file.open(prefix + "_agent_steps.csv");

    if (!metadata_file.is_open() || !field_file.is_open() || !step_file.is_open() || !agent_file.is_open()) {
        throw std::runtime_error("Failed to open one or more detailed CSV files for writing.");
    }

    prepare_stream(metadata_file);
    prepare_stream(field_file);
    prepare_stream(step_file);
    prepare_stream(agent_file);

    metadata_file << "agent_count,seed,field_seed,max_steps,num_gaussians\n";
    metadata_file << agent_count << ',' << seed << ',' << field_seed << ',' << max_steps << ',' << num_gaussians << '\n';

    field_file << "gaussian_id,center_x,center_y,amplitude,sigma\n";
    const auto gaussians = field.get_gaussians();
    for (size_t i = 0; i < gaussians.size(); ++i) {
        const auto& gaussian = gaussians[i];
        field_file << i << ','
                   << gaussian.center.getX() << ','
                   << gaussian.center.getY() << ','
                   << gaussian.amplitude << ','
                   << gaussian.sigma << '\n';
    }

    step_file << "step,time,best_field,mean_field,cumulative_best_mean,avg_speed,swarm_radius\n";
    agent_file << "step,time,agent_id,x,y,vx,vy,ax,ay,speed,acceleration_magnitude,field_value,field_delta,neighbor_count,nearest_neighbor_distance,mean_neighbor_distance,avoidance_gain,quark_gain,directional_derivative_gain,linear_drag_gain,action_avoidance,action_quark,action_directional_derivative,action_linear_drag\n";

    started = true;
}

void TrialCsvLogger::log_step(int step,
                              double time,
                              const std::vector<Agent*>& agents,
                              double best_field,
                              double mean_field,
                              double cumulative_best_mean,
                              double avg_speed,
                              double swarm_radius) {
    if (!started) {
        return;
    }

    step_file << step << ','
              << time << ','
              << best_field << ','
              << mean_field << ','
              << cumulative_best_mean << ','
              << avg_speed << ','
              << swarm_radius << '\n';

    for (Agent* agent : agents) {
        const Pos2 position = agent->get_position();
        Vec2 velocity = agent->get_current_velocity();
        Vec2 acceleration = agent->get_acceleration();
        const BehaviorParams gains = agent->get_current_behavior_params();
        const TreeObservation observation = agent->get_last_tree_observation();
        const std::array<GainAction, 4> actions = agent->get_last_tree_actions();

        agent_file << step << ','
                   << time << ','
                   << agent->id << ','
                   << position.getX() << ','
                   << position.getY() << ','
                   << velocity.getX() << ','
                   << velocity.getY() << ','
                   << acceleration.getX() << ','
                   << acceleration.getY() << ','
                   << velocity.len() << ','
                   << acceleration.len() << ','
                   << agent->get_last_field_value() << ','
                   << observation.field_delta << ','
                   << observation.neighbor_count << ','
                   << observation.nearest_neighbor_distance << ','
                   << observation.mean_neighbor_distance << ','
                   << gains.avoidance_gain << ','
                   << gains.quark_gain << ','
                   << gains.directional_derivative_gain << ','
                   << gains.linear_drag_gain << ','
                   << action_to_int(actions[0]) << ','
                   << action_to_int(actions[1]) << ','
                   << action_to_int(actions[2]) << ','
                   << action_to_int(actions[3]) << '\n';
    }
}

void TrialCsvLogger::finish() {
    if (!started) {
        return;
    }

    metadata_file.flush();
    field_file.flush();
    step_file.flush();
    agent_file.flush();

    metadata_file.close();
    field_file.close();
    step_file.close();
    agent_file.close();
    started = false;
}