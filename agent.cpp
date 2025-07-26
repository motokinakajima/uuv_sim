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

void Agent::update_with_world(const WorldState& world, double delta_t) {
    update_neighbors(world);
    
    Vec2 desiredVelocity = make_decision();
    
    velocity = desiredVelocity;
    update_position(delta_t);
}


//temporal
Vec2 Agent::make_decision() {
    if (neighbor_infos.empty()) {
        return Vec2(0.1f, 0.1f);
    }
    
    Vec2 avoidance_force(0, 0);
    Vec2 seek_low_field_force(0, 0);
    
    NeighborInfo self_info;
    for (const auto& neighbor : neighbor_infos) {
        if (neighbor.agent_id == id) {
            self_info = neighbor;
            break;
        }
    }
    
    for (const auto& neighbor : neighbor_infos) {
        if (neighbor.agent_id == id) continue;
        
        Vec2 direction = neighbor.relative_position;
        float distance = std::sqrt(direction.dot(direction));
        
        if (distance < 20.0f) {
            Vec2 repulsion = direction * (-1.0f / (distance + 0.1f));
            avoidance_force = avoidance_force + repulsion;
        }
        
        if (neighbor.field_val < self_info.field_val) {
            Vec2 attraction = direction * (0.3f / (distance + 1.0f));
            seek_low_field_force = seek_low_field_force + attraction;
        }
    }
    
    Vec2 new_velocity = velocity + avoidance_force * 2.0f + seek_low_field_force;
    
    float speed = std::sqrt(new_velocity.dot(new_velocity));
    if (speed > 5.0f) {
        new_velocity = new_velocity * (5.0f / speed);
    }
    
    return new_velocity;
}

void Agent::update_neighbors(const WorldState& world) {
    neighbor_infos.clear();
    
    for (Agent* other : world.allAgents) {
        Vec2 relative_pos = other->get_position() - position;
        double other_field_val = world.field->get_scalar(other->get_position());

        NeighborInfo neighbor_info;
        neighbor_info.agent_id = other->id;
        neighbor_info.relative_position = relative_pos;
        neighbor_info.field_val = other_field_val;
        
        neighbor_infos.push_back(neighbor_info);
    }
}