#include <iostream>
#include "event_controller.h"
#include "agent.h"
#include "field.h"
#include "data_logger.h"
#include "behavior_params.h"

int main() {
    EventController controller;

    BehaviorParams params;
    std::string param_error;
    if (load_behavior_params("behavior_params.cfg", params, &param_error)) {
        std::cout << "Loaded behavior_params.cfg" << std::endl;
    } else {
        std::cout << "Using default behavior params: " << param_error << std::endl;
    }
    Agent::set_behavior_params(params);
    
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
