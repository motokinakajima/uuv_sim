#include "vec2.h"
#include <cmath>

// Constructors
Vec2::Vec2() : x(0.0f), y(0.0f) {
}

Vec2::Vec2(float x, float y) : x(x), y(y) {
}

// Getters and Setters
float Vec2::getX() const {
    return x;
}

void Vec2::setX(float x) {
    this->x = x;
}

float Vec2::getY() const {
    return y;
}

void Vec2::setY(float y) {
    this->y = y;
}

// Basic operations
Vec2 Vec2::operator+(const Vec2& other) const {
    return Vec2(x + other.x, y + other.y);
}

Vec2 Vec2::operator-(const Vec2& other) const {
    return Vec2(x - other.x, y - other.y);
}

Vec2 Vec2::operator*(float scalar) const {
    return Vec2(x * scalar, y * scalar);
}

Vec2 Vec2::operator/(float scalar) const {
    return Vec2(x / scalar, y / scalar);
}

// Dot and Cross Product
float Vec2::dot(const Vec2& other) const {
    return x * other.x + y * other.y;
}

float Vec2::cross(const Vec2& other) const {
    return x * other.y - y * other.x;
}

float Vec2::len() {
    return std::sqrt( x * x + y * y );
}