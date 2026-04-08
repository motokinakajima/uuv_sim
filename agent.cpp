#include "agent.h"
#include "world_state.h"
#include "neighbor_info.h"
#include <cmath>

BehaviorParams Agent::behavior_params;

Agent::Agent(const int id) {
    this->id = id;
    max_velocity = behavior_params.max_velocity;
}

Agent::Agent(const int id, const Pos2& position, const Vec2& velocity) {
    this->id = id;
    this->position = position;
    this->current_velocity = velocity;
    max_velocity = behavior_params.max_velocity;
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

void Agent::set_behavior_params(const BehaviorParams& params) {
    behavior_params = params;
}

BehaviorParams Agent::get_behavior_params() {
    return behavior_params;
}

double Agent::get_field_value(const Field& field) {
    return field.get_scalar(position);
}

void Agent::update_with_world(const WorldState& world, double delta_t) {
    update_neighbors(world);
    double self_field_value = world.field->get_scalar(position);
    
    Vec2 appliedForce = make_decision(self_field_value);
    
    acceleration = appliedForce;

    // Store one-step memory before moving for the next directional estimate.
    prev_position = position;
    prev_field_value = self_field_value;
    has_prev_sample = true;

    update_position(delta_t);
}


//temporal
Vec2 Agent::make_decision(double self_field_value) {
    if (neighbor_infos.empty()) {
        return Vec2(0.1f, 0.1f);
    }
    
    Vec2 avoidance_force(0, 0);
    Vec2 quark_force(0, 0);
    Vec2 directional_force(0, 0);
    
    for (const auto& neighbor : neighbor_infos) {
        if (neighbor.agent_id == id) continue;
        
        Vec2 direction = neighbor.relative_position;
        float distance = std::sqrt(direction.dot(direction));
        if (distance < 1e-4f) continue;
        
        if (distance < static_cast<float>(behavior_params.avoidance_radius)) {
            Vec2 repulsion = direction * (-1.0f / (distance + 0.1f));
            avoidance_force = avoidance_force + repulsion;
        }

        Vec2 unit_direction = direction * (1.0f / distance);
        float saturation = distance / (distance + static_cast<float>(behavior_params.quark_saturation_distance));
        float magnitude = static_cast<float>(behavior_params.quark_max_force) * saturation;
        quark_force = quark_force + (unit_direction * magnitude);
    }

    if (has_prev_sample) {
        Vec2 delta_pos = position - prev_position;
        double step_sq = delta_pos.dot(delta_pos);

        if (step_sq > behavior_params.directional_eps) {
            double delta_f = self_field_value - prev_field_value;
            double projection_scale = delta_f / (step_sq + behavior_params.directional_eps);
            Vec2 projected_gradient = delta_pos * static_cast<float>(projection_scale);
            directional_force = projected_gradient * static_cast<float>(-behavior_params.directional_derivative_gain);
        }
    }

    Vec2 drag_force = current_velocity * static_cast<float>(-behavior_params.linear_drag_gain);

    Vec2 acc = avoidance_force * static_cast<float>(behavior_params.avoidance_gain)
               + quark_force * static_cast<float>(behavior_params.quark_gain)
               + directional_force
               + drag_force;
    
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