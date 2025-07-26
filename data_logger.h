#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <fstream>
#include <vector>
#include "agent.h"
#include "field.h"

class DataLogger {
public:
    DataLogger(const std::string& filename);
    ~DataLogger();
    
    void start_simulation(const Field& field);
    void log_timestep(double time, const std::vector<Agent*>& agents);
    void finish_simulation();

private:
    std::ofstream file;
    bool first_timestep;
};

#endif // DATA_LOGGER_H
