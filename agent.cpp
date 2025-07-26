#include "agent.h"
#include "world_state.h"
#include "neighbor_info.h"
#include <cmath>

Agent::Agent(const int id) { this->id = id; }

Agent::Agent(const int id, const Pos2& position, const Vec2& velocity) {
    this->id = id;
    this->position = position;
    this->velocity = velocity;
}

void Agent::update_position(double delta_t) {
    position = position + (velocity * delta_t);
}

Pos2 Agent::get_position() {
    return position;
}

Vec2 Agent::get_velocity() {
    return velocity;
}

void Agent::set_position(const Pos2& position) {
    this->position = position;
}

void Agent::set_velocity(const Vec2& velocity) {
    this->velocity = velocity;
}

double Agent::get_field_value(const Field& field) {
    return field.get_scalar(position);
}

// NEW METHOD: Agent updates itself based on world knowledge
void Agent::update_with_world(const WorldState& world, double delta_t) {
    update_neighbors(world);
    
    Vec2 desiredVelocity = make_decision(world);
    
    velocity = desiredVelocity;
    update_position(delta_t);
}


Vec2 Agent::make_decision(const WorldState& world) {
    Vec2 desiredVelocity = velocity;  // Start with current velocity
    
    // Simple behavior: Move away from the field gradient
    double fieldHere = world.field->get_scalar(position);
    double fieldRight = world.field->get_scalar(Pos2(position.getX() + 1, position.getY()));
    double fieldUp = world.field->get_scalar(Pos2(position.getX(), position.getY() + 1));
    
    // Calculate gradient (direction of steepest increase)
    Vec2 gradient(fieldRight - fieldHere, fieldUp - fieldHere);
    
    // Move against the gradient (towards lower field values)
    desiredVelocity = desiredVelocity - gradient * 0.1f;
    
    return desiredVelocity;
}

void Agent::update_neighbors(const WorldState& world) {
    neighbor_infos.clear();
    
    for (Agent* other : world.allAgents) {
        if (other == this) continue;

        Vec2 relative_pos = other->get_position() - position;
        double other_field_val = world.field->get_scalar(other->get_position());

        NeighborInfo neighbor_info;
        neighbor_info.agent_id = other->id;
        neighbor_info.relative_position = relative_pos;
        neighbor_info.field_val = other_field_val;
        
        neighbor_infos.push_back(neighbor_info);
    }
}