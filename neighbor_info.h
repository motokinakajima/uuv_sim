#ifndef NEIGHBOR_INFO_H
#define NEIGHBOR_INFO_H

#include "vec2.h"

struct NeighborInfo {
    int agent_id;
    Vec2 relative_position;
    double field_val;
};

#endif // NEIGHBOR_INFO_H