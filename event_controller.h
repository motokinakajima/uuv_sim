#ifndef EVENT_CONTROLLER_H
#define EVENT_CONTROLLER_H

#include <vector>
#include <string>

// Forward declarations
class Agent;
class Field;

class EventController {
public:
    EventController();
    void step();
    void run();
    void run_with_logger(std::string filename);
    void stop();
    double get_t();
    void update_state();

    void add_agent(Agent* agent);

    std::vector<Agent*> agents;
    Field* field;

private:
    double cur_t = 0;
};

#endif  // EVENT_CONTROLLER_H