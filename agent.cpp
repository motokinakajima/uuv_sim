#include "agent.h"
#include "world_state.h"
#include "neighbor_info.h"
#include <cmath>

Agent::Agent(const int id) { this->id = id; }

Agent::Agent(const int id, const Pos2& position, const Vec2& velocity) {
    this->id = id;
    this->position = position;
    this->current_velocity = velocity;
}

void Agent::update_velocity(double delta_t) {
    current_velocity = current_velocity + (acceleration * delta_t);
    double vel_len = current_velocity.len();
    if (vel_len > max_velocity) current_velocity = current_velocity * (max_velocity / vel_len);
}

void Agent::update_position(double delta_t) {
    update_velocity(delta_t);
    position = position + (current_velocity * delta_t);
}

Pos2 Agent::get_position() {
    return position;
}

Vec2 Agent::get_current_velocity() {
    return current_velocity;
}

Vec2 Agent::get_acceleration() {
    return acceleration;
}

void Agent::set_position(const Pos2& position) {
    this->position = position;
}

void Agent::set_current_velocity(const Vec2& velocity) {
    this->current_velocity = velocity;
}

void Agent::set_acceleration(const Vec2& acceleration) {
    this->acceleration = acceleration;
}

double Agent::get_max_velocity() {
    return max_velocity;
}

void Agent::set_max_velocity(const double max_val) {
    max_velocity = max_val;
}

double Agent::get_field_value(const Field& field) {
    return field.get_scalar(position);
}

void Agent::update_with_world(const WorldState& world, double delta_t) {
    update_neighbors(world);
    
    Vec2 appliedForce = make_decision();
    
    acceleration = appliedForce;
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

    Vec2 acc = avoidance_force * 2.0f + seek_low_field_force;
    
    return acc;
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