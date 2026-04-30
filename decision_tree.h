#ifndef UUV_SIM_DECISION_TREE_H
#define UUV_SIM_DECISION_TREE_H

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

enum class TreeFeature {
    SpeedMagnitude,
    AccelerationMagnitude,
    FieldDelta,
    NeighborCount,
    NearestNeighborDistance,
    MeanNeighborDistance,
};

enum class GainAction {
    Decrease = -1,
    Hold = 0,
    Increase = 1,
};

struct TreeObservation {
    double speed_magnitude = 0.0;
    double acceleration_magnitude = 0.0;
    double field_delta = 0.0;
    double neighbor_count = 0.0;
    double nearest_neighbor_distance = 0.0;
    double mean_neighbor_distance = 0.0;
};

struct DecisionTreeNode {
    bool is_leaf = false;
    TreeFeature feature = TreeFeature::SpeedMagnitude;
    double threshold = 0.0;
    int left_child_id = -1;
    int right_child_id = -1;
    std::array<GainAction, 4> leaf_actions{
        GainAction::Hold,
        GainAction::Hold,
        GainAction::Hold,
        GainAction::Hold,
    };
};

class DecisionTree {
public:
    void clear();
    bool empty() const;

    void set_root(int root_id);
    bool has_root() const;

    bool add_node(int id, const DecisionTreeNode& node);
    bool build_from_specs(const std::vector<std::string>& node_specs,
                          int root_id,
                          std::string* error_message = nullptr);

    std::array<GainAction, 4> evaluate(const TreeObservation& observation) const;

    static DecisionTree default_tree();

private:
    std::unordered_map<int, DecisionTreeNode> nodes;
    int root_id = -1;

    static double feature_value(const TreeObservation& observation, TreeFeature feature);
    static bool parse_node_spec(const std::string& spec,
                                int* out_id,
                                DecisionTreeNode* out_node,
                                std::string* error_message);
    bool validate_subtree(int node_id,
                          std::unordered_map<int, bool>& visiting,
                          std::unordered_map<int, bool>& visited,
                          std::string* error_message) const;
};

#endif // UUV_SIM_DECISION_TREE_H