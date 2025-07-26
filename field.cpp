#include "field.h"
#include "vec2.h"

Field::Field(double k1, double k2, double k3, double k4) {
    this->k1 = k1;
    this->k2 = k2;
    this->k3 = k3;
    this->k4 = k4;
}

double Field::get_scalar(double x, double y) const {
    return k1*x*x + k2*x*y + k3*y*y + k4;
}

double Field::get_scalar(const Pos2& pos) const {
    return get_scalar(pos.getX(), pos.getY());
}