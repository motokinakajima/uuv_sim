#ifndef FIELD_H
#define FIELD_H

#include "vec2.h"

class Field {
public:
    Field(double k1, double k2, double k3, double k4);

    double get_scalar(double x, double y) const;
    double get_scalar(const Pos2& pos) const;

    double get_param1() const { return k1; }
    double get_param2() const { return k2; }
    double get_param3() const { return k3; }
    double get_param4() const { return k4; }

private:
    // f(x,y) = k1*x^2 + k2*xy + k3*y^2 + k4
    double k1, k2, k3, k4;
};

#endif // FIELD_H