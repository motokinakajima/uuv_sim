#ifndef UUV_SIM_TRIAL_CSV_LOGGER_H
#define UUV_SIM_TRIAL_CSV_LOGGER_H

#include <fstream>
#include <string>

#include "agent.h"
#include "field.h"

class TrialCsvLogger {
public:
    explicit TrialCsvLogger(std::string prefix);

    void start(const Field& field,
               int agent_count,
               unsigned int seed,
               unsigned int field_seed,
               int max_steps,
               int num_gaussians);
    void log_step(int step,
                  double time,
                  const std::vector<Agent*>& agents,
                  double best_field,
                  double mean_field,
                  double cumulative_best_mean,
                  double avg_speed,
                  double swarm_radius);
    void finish();

private:
    std::string prefix;
    std::ofstream metadata_file;
    std::ofstream field_file;
    std::ofstream step_file;
    std::ofstream agent_file;
    bool started = false;

    static int action_to_int(GainAction action);
};

#endif // UUV_SIM_TRIAL_CSV_LOGGER_H