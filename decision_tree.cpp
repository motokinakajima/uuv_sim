#include "decision_tree.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace {
std::string trim(const std::string& input) {
    size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
        begin++;
    }

    size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        end--;
    }

    return input.substr(begin, end - begin);
}

std::vector<std::string> split_csv(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(input);
    while (std::getline(ss, token, ',')) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

std::string lowercase_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

bool parse_feature(const std::string& text, TreeFeature* out_feature) {
    const std::string feature = lowercase_copy(trim(text));
    if (feature == "speed" || feature == "speed_magnitude" || feature == "velocity" || feature == "velocity_magnitude") {
        *out_feature = TreeFeature::SpeedMagnitude;
        return true;
    }
    if (feature == "accel" || feature == "acceleration" || feature == "acceleration_magnitude") {
        *out_feature = TreeFeature::AccelerationMagnitude;
        return true;
    }
    if (feature == "field_delta" || feature == "field_change" || feature == "delta_f") {
        *out_feature = TreeFeature::FieldDelta;
        return true;
    }
    if (feature == "neighbor_count" || feature == "neighbors" || feature == "neighbor_num") {
        *out_feature = TreeFeature::NeighborCount;
        return true;
    }
    if (feature == "nearest_neighbor_distance" || feature == "nearest_dist" || feature == "min_neighbor_distance") {
        *out_feature = TreeFeature::NearestNeighborDistance;
        return true;
    }
    if (feature == "mean_neighbor_distance" || feature == "avg_neighbor_distance" || feature == "mean_dist") {
        *out_feature = TreeFeature::MeanNeighborDistance;
        return true;
    }
    return false;
}

bool parse_action(const std::string& text, GainAction* out_action) {
    const std::string value = lowercase_copy(trim(text));
    if (value == "-1" || value == "decrease" || value == "dec" || value == "down") {
        *out_action = GainAction::Decrease;
        return true;
    }
    if (value == "0" || value == "hold" || value == "stay" || value == "keep") {
        *out_action = GainAction::Hold;
        return true;
    }
    if (value == "1" || value == "increase" || value == "inc" || value == "up") {
        *out_action = GainAction::Increase;
        return true;
    }
    return false;
}

std::array<GainAction, 4> make_hold_actions() {
    return {
        GainAction::Hold,
        GainAction::Hold,
        GainAction::Hold,
        GainAction::Hold,
    };
}
} // namespace

void DecisionTree::clear() {
    nodes.clear();
    root_id = -1;
}

bool DecisionTree::empty() const {
    return nodes.empty() || root_id < 0;
}

void DecisionTree::set_root(int new_root_id) {
    root_id = new_root_id;
}

bool DecisionTree::has_root() const {
    return root_id >= 0;
}

bool DecisionTree::add_node(int id, const DecisionTreeNode& node) {
    return nodes.emplace(id, node).second;
}

double DecisionTree::feature_value(const TreeObservation& observation, TreeFeature feature) {
    switch (feature) {
        case TreeFeature::SpeedMagnitude:
            return observation.speed_magnitude;
        case TreeFeature::AccelerationMagnitude:
            return observation.acceleration_magnitude;
        case TreeFeature::FieldDelta:
            return observation.field_delta;
        case TreeFeature::NeighborCount:
            return observation.neighbor_count;
        case TreeFeature::NearestNeighborDistance:
            return observation.nearest_neighbor_distance;
        case TreeFeature::MeanNeighborDistance:
            return observation.mean_neighbor_distance;
    }

    return 0.0;
}

bool DecisionTree::parse_node_spec(const std::string& spec,
                                   int* out_id,
                                   DecisionTreeNode* out_node,
                                   std::string* error_message) {
    const std::vector<std::string> tokens = split_csv(spec);
    if (tokens.size() < 2) {
        if (error_message != nullptr) {
            *error_message = "Tree node spec must contain at least an id and a node type.";
        }
        return false;
    }

    try {
        *out_id = std::stoi(tokens[0]);
    } catch (const std::exception&) {
        if (error_message != nullptr) {
            *error_message = "Invalid node id in tree spec: " + tokens[0];
        }
        return false;
    }

    const std::string node_type = lowercase_copy(tokens[1]);
    if (node_type == "split") {
        if (tokens.size() != 6) {
            if (error_message != nullptr) {
                *error_message = "Split node spec must be: id,split,feature,threshold,left_id,right_id";
            }
            return false;
        }

        DecisionTreeNode node;
        node.is_leaf = false;
        if (!parse_feature(tokens[2], &node.feature)) {
            if (error_message != nullptr) {
                *error_message = "Unknown tree feature: " + tokens[2];
            }
            return false;
        }

        try {
            node.threshold = std::stod(tokens[3]);
            node.left_child_id = std::stoi(tokens[4]);
            node.right_child_id = std::stoi(tokens[5]);
        } catch (const std::exception&) {
            if (error_message != nullptr) {
                *error_message = "Invalid numeric value in split node spec: " + spec;
            }
            return false;
        }

        *out_node = node;
        return true;
    }

    if (node_type == "leaf") {
        if (tokens.size() != 6) {
            if (error_message != nullptr) {
                *error_message = "Leaf node spec must be: id,leaf,action0,action1,action2,action3";
            }
            return false;
        }

        DecisionTreeNode node;
        node.is_leaf = true;
        node.leaf_actions = make_hold_actions();

        for (size_t i = 0; i < 4; ++i) {
            GainAction action = GainAction::Hold;
            if (!parse_action(tokens[i + 2], &action)) {
                if (error_message != nullptr) {
                    *error_message = "Invalid leaf action: " + tokens[i + 2];
                }
                return false;
            }
            node.leaf_actions[i] = action;
        }

        *out_node = node;
        return true;
    }

    if (error_message != nullptr) {
        *error_message = "Unknown tree node type: " + tokens[1];
    }
    return false;
}

bool DecisionTree::validate_subtree(int node_id,
                                    std::unordered_map<int, bool>& visiting,
                                    std::unordered_map<int, bool>& visited,
                                    std::string* error_message) const {
    if (visited[node_id]) {
        return true;
    }
    if (visiting[node_id]) {
        if (error_message != nullptr) {
            *error_message = "Cycle detected in decision tree at node " + std::to_string(node_id);
        }
        return false;
    }

    auto it = nodes.find(node_id);
    if (it == nodes.end()) {
        if (error_message != nullptr) {
            *error_message = "Missing decision tree node: " + std::to_string(node_id);
        }
        return false;
    }

    visiting[node_id] = true;
    const DecisionTreeNode& node = it->second;
    bool ok = true;

    if (!node.is_leaf) {
        ok = validate_subtree(node.left_child_id, visiting, visited, error_message)
             && validate_subtree(node.right_child_id, visiting, visited, error_message);
    }

    visiting[node_id] = false;
    visited[node_id] = true;
    return ok;
}

bool DecisionTree::build_from_specs(const std::vector<std::string>& node_specs,
                                    int new_root_id,
                                    std::string* error_message) {
    clear();

    for (const std::string& spec : node_specs) {
        int node_id = -1;
        DecisionTreeNode node;
        if (!parse_node_spec(spec, &node_id, &node, error_message)) {
            clear();
            return false;
        }

        if (!add_node(node_id, node)) {
            if (error_message != nullptr) {
                *error_message = "Duplicate decision tree node id: " + std::to_string(node_id);
            }
            clear();
            return false;
        }
    }

    root_id = new_root_id;
    if (nodes.find(root_id) == nodes.end()) {
        if (error_message != nullptr) {
            *error_message = "Root node not found in decision tree: " + std::to_string(root_id);
        }
        clear();
        return false;
    }

    std::unordered_map<int, bool> visiting;
    std::unordered_map<int, bool> visited;
    if (!validate_subtree(root_id, visiting, visited, error_message)) {
        clear();
        return false;
    }

    return true;
}

std::array<GainAction, 4> DecisionTree::evaluate(const TreeObservation& observation) const {
    std::array<GainAction, 4> fallback = make_hold_actions();
    if (empty()) {
        return fallback;
    }

    int current_id = root_id;
    for (int depth = 0; depth < 64; ++depth) {
        auto it = nodes.find(current_id);
        if (it == nodes.end()) {
            return fallback;
        }

        const DecisionTreeNode& node = it->second;
        if (node.is_leaf) {
            return node.leaf_actions;
        }

        const double value = feature_value(observation, node.feature);
        const int next_id = (value < node.threshold) ? node.left_child_id : node.right_child_id;
        if (next_id == current_id) {
            return fallback;
        }
        current_id = next_id;
    }

    return fallback;
}

DecisionTree DecisionTree::default_tree() {
    DecisionTree tree;

    DecisionTreeNode root;
    root.is_leaf = false;
    root.feature = TreeFeature::FieldDelta;
    root.threshold = 0.0;
    root.left_child_id = 1;
    root.right_child_id = 2;
    tree.add_node(0, root);

    DecisionTreeNode improving_branch;
    improving_branch.is_leaf = false;
    improving_branch.feature = TreeFeature::SpeedMagnitude;
    improving_branch.threshold = 0.8;
    improving_branch.left_child_id = 3;
    improving_branch.right_child_id = 4;
    tree.add_node(1, improving_branch);

    DecisionTreeNode worsening_branch;
    worsening_branch.is_leaf = false;
    worsening_branch.feature = TreeFeature::NearestNeighborDistance;
    worsening_branch.threshold = 10.0;
    worsening_branch.left_child_id = 5;
    worsening_branch.right_child_id = 6;
    tree.add_node(2, worsening_branch);

    DecisionTreeNode leaf3;
    leaf3.is_leaf = true;
    leaf3.leaf_actions = {GainAction::Hold, GainAction::Decrease, GainAction::Increase, GainAction::Decrease};
    tree.add_node(3, leaf3);

    DecisionTreeNode leaf4;
    leaf4.is_leaf = true;
    leaf4.leaf_actions = {GainAction::Decrease, GainAction::Hold, GainAction::Decrease, GainAction::Hold};
    tree.add_node(4, leaf4);

    DecisionTreeNode leaf5;
    leaf5.is_leaf = true;
    leaf5.leaf_actions = {GainAction::Increase, GainAction::Decrease, GainAction::Increase, GainAction::Decrease};
    tree.add_node(5, leaf5);

    DecisionTreeNode leaf6;
    leaf6.is_leaf = true;
    leaf6.leaf_actions = {GainAction::Hold, GainAction::Increase, GainAction::Increase, GainAction::Decrease};
    tree.add_node(6, leaf6);

    tree.set_root(0);
    return tree;
}