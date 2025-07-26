#include "event_controller.h"
#include "constants.h"
#include "world_state.h"
#include "data_logger.h"
#include <string>
#include <vector>

EventController::EventController() = default;

void EventController::step() {
    cur_t += DELTA_T;
    update_state();
}

void EventController::run() {
    while (cur_t < END_T) {
        step();
    }
}

void EventController::run_with_logger(std::string filename) {
    DataLogger logger(filename);
    logger.start_simulation(*this->field);
    while (cur_t < END_T) {
        logger.log_timestep(cur_t, agents);
        step();
    }
    logger.finish_simulation();
}

void EventController::stop() {
    return;
}

double EventController::get_t() {
    return cur_t;
}

void EventController::update_state() {
    WorldState state = WorldState(agents, field, cur_t);

    for (Agent* agent : agents) {
        agent->update_with_world(state, DELTA_T);
    }
}

void EventController::add_agent(Agent* agent) {
    agents.push_back(agent);
}