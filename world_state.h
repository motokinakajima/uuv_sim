#ifndef WORLD_STATE_H
#define WORLD_STATE_H

#include <vector>
#include "agent.h"
#include "field.h"

struct WorldState {
    std::vector<Agent*> allAgents;
    Field* field;
    double currentTime;

    WorldState(std::vector<Agent*> agents, Field* f, double time) 
        : allAgents(agents), field(f), currentTime(time) {}
};

#endif // WORLD_STATE_H
