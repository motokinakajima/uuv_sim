#ifndef UUV_SIM_AGENT_H
#define UUV_SIM_AGENT_H

#include <vector>
#include "vec2.h"
#include "field.h"
#include "neighbor_info.h"

// Forward declaration to avoid circular includes
struct WorldState;

class Agent {
public:
    int id;

    Agent(const int id);
    Agent(const int id, const Pos2& position, const Vec2& velocity);

    void update_position(double delta_t);
    void update_with_world(const WorldState& world, double delta_t);
    
    Pos2 get_position();
    Vec2 get_velocity();
    void set_position(const Pos2& position);
    void set_velocity(const Vec2& velocity);

    double get_field_value(const Field& field);

private:
    Pos2 position;
    Vec2 velocity;

    std::vector<NeighborInfo> neighbor_infos;

    Vec2 make_decision();
    void update_neighbors(const WorldState& world);
};

#endif // UUV_SIM_AGENT_H
