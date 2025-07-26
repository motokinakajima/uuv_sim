#ifndef UUV_SIM_AGENT_H
#define UUV_SIM_AGENT_H

#include <vector>
#include "vec2.h"

// Forward declaration to avoid circular includes
struct WorldState;

class Agent {
public:
    Agent();
    Agent(const Pos2& position, const Vec2& velocity);

    void update_position(double delta_t);
    void update_with_world(const WorldState& world, double delta_t);
    
    Pos2 get_position();
    Vec2 get_velocity();
    void set_position(const Pos2& position);
    void set_velocity(const Vec2& velocity);

private:
    Pos2 position;
    Vec2 velocity;
    
    // Agent's intelligence methods
    std::vector<Agent*> find_nearby_agents(const WorldState& world, double radius = 50.0);
    Vec2 make_decision(const WorldState& world);
};

#endif // UUV_SIM_AGENT_H
