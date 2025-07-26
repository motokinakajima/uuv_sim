#ifndef FIELD_H
#define FIELD_H

#include "vec2.h"
#include <vector>

struct Gaussian {
    Pos2 center;
    double amplitude;
    double sigma;
};

class Field {
public:
    Field(int num_gaussians);
    Field(int num_gaussians, double min_amp, double max_amp, double min_sigma, double max_sigma);

    void randomize_gaussians(double min_amp, double max_amp, double min_sigma, double max_sigma);
    void set_gaussians(const std::vector<Gaussian>& gaussians);

    double get_scalar(double x, double y) const;
    double get_scalar(const Pos2& pos) const;
    
    std::vector<Gaussian> get_gaussians() const;
    int get_num_gaussians() const;

private:
    std::vector<Gaussian> gaussians;
    double rand_range(double min, double max);
};

#endif // FIELD_H