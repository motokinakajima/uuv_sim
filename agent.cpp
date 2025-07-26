#include "agent.h"
#include "world_state.h"
#include <cmath>

Agent::Agent() = default;

Agent::Agent(const Pos2& position, const Vec2& velocity) {
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

// NEW METHOD: Agent updates itself based on world knowledge
void Agent::update_with_world(const WorldState& world, double delta_t) {
    // Agent perceives its environment
    auto nearbyAgents = find_nearby_agents(world, 50.0);  // Look within 50 units
    double fieldValue = world.field->get_scalar(position.getX(), position.getY());
    
    // Agent makes intelligent decision
    Vec2 desiredVelocity = make_decision(world);
    
    // Agent updates itself
    velocity = desiredVelocity;
    update_position(delta_t);
}

// Agent's intelligence: Find nearby agents
std::vector<Agent*> Agent::find_nearby_agents(const WorldState& world, double radius) {
    std::vector<Agent*> nearby;
    
    for (Agent* other : world.allAgents) {
        if (other == this) continue;  // Don't include myself
        
        // Calculate distance
        Vec2 diff = other->get_position() - position;
        double distance = std::sqrt(diff.dot(diff));  // sqrt(x²+y²)
        
        if (distance <= radius) {
            nearby.push_back(other);
        }
    }
    return nearby;
}

// Agent's intelligence: Make decisions based on what it perceives
Vec2 Agent::make_decision(const WorldState& world) {
    Vec2 desiredVelocity = velocity;  // Start with current velocity
    
    // Simple behavior: Move away from the field gradient
    double fieldHere = world.field->get_scalar(position.getX(), position.getY());
    double fieldRight = world.field->get_scalar(position.getX() + 1, position.getY());
    double fieldUp = world.field->get_scalar(position.getX(), position.getY() + 1);
    
    // Calculate gradient (direction of steepest increase)
    Vec2 gradient(fieldRight - fieldHere, fieldUp - fieldHere);
    
    // Move against the gradient (towards lower field values)
    desiredVelocity = desiredVelocity - gradient * 0.1f;
    
    return desiredVelocity;
}