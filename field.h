#ifndef FIELD_H
#define FIELD_H

class Field {
public:
    Field(double k1, double k2, double k3, double k4);
    double get_scalar(double x, double y);

private:
    // f(x,y) = k1*x^2 + k2*xy + k3*y^2 + k4
    double k1, k2, k3, k4;
};

#endif // FIELD_H