#include "field.h"
#include "vec2.h"
#include <cmath>
#include <random>
#include <vector>
#include <stdexcept>
#include <string>

Field::Field(int num_gaussians) {
    gaussians.reserve(num_gaussians);
    randomize_gaussians(-100.0, 100.0, 5.0, 25.0);
}

Field::Field(int num_gaussians, double min_amp, double max_amp, double min_sigma, double max_sigma) {
    gaussians.reserve(num_gaussians);
    randomize_gaussians(min_amp, max_amp, min_sigma, max_sigma);
}

void Field::randomize_gaussians(double min_amp, double max_amp, double min_sigma, double max_sigma) {
    gaussians.clear();
    
    int target_size = gaussians.capacity();
    
    for (int i = 0; i < target_size; i++) {
        Gaussian g;
        g.center = Pos2(rand_range(-50.0, 50.0), rand_range(-50.0, 50.0));
        g.amplitude = rand_range(min_amp, max_amp);
        g.sigma = rand_range(min_sigma, max_sigma);
        gaussians.push_back(g);
    }
}

void Field::set_gaussians(const std::vector<Gaussian>& new_gaussians) {
    gaussians = new_gaussians;
}

double Field::get_scalar(double x, double y) const {
    double result = 0.0;
    
    for (const auto& g : gaussians) {
        double dx = x - g.center.getX();
        double dy = y - g.center.getY();
        double dist_sq = dx * dx + dy * dy;
        double gaussian_val = g.amplitude * std::exp(-dist_sq / (2.0 * g.sigma * g.sigma));
        result += gaussian_val;
    }
    
    return result;
}

double Field::get_scalar(const Pos2& pos) const {
    return get_scalar(pos.getX(), pos.getY());
}

std::vector<Gaussian> Field::get_gaussians() const {
    return gaussians;
}

int Field::get_num_gaussians() const {
    return gaussians.size();
}

double Field::rand_range(double min, double max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(min, max);
    return dist(gen);
}