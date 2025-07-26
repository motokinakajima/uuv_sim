#include "data_logger.h"

DataLogger::DataLogger(const std::string& filename) : first_timestep(true) {
    file.open(filename);
}

DataLogger::~DataLogger() {
    if (file.is_open()) {
        file.close();
    }
}

void DataLogger::start_simulation(const Field& field) {
    file << "{\n";
    file << "  \"simulation\": {\n";
    file << "    \"field_params\": [" << field.get_param1() << ", " << field.get_param2() << ", " << field.get_param3() << ", " << field.get_param4() << "],\n";
    file << "    \"timesteps\": [\n";
    first_timestep = true;
}

void DataLogger::log_timestep(double time, const std::vector<Agent*>& agents) {
    if (!first_timestep) {
        file << ",\n";
    }
    
    file << "      {\n";
    file << "        \"time\": " << time << ",\n";
    file << "        \"agents\": [\n";
    
    for (size_t j = 0; j < agents.size(); j++) {
        Agent* agent = agents[j];
        Pos2 pos = agent->get_position();
        Vec2 vel = agent->get_velocity();
        
        file << "          {\n";
        file << "            \"id\": " << agent->id << ",\n";
        file << "            \"x\": " << pos.getX() << ",\n";
        file << "            \"y\": " << pos.getY() << ",\n";
        file << "            \"vx\": " << vel.getX() << ",\n";
        file << "            \"vy\": " << vel.getY() << "\n";
        file << "          }";
        
        if (j < agents.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "        ]\n";
    file << "      }";
    
    first_timestep = false;
}

void DataLogger::finish_simulation() {
    file << "\n    ]\n";
    file << "  }\n";
    file << "}\n";
}
