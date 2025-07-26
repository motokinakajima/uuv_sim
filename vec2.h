#ifndef UUV_SIM_VEC2_H
#define UUV_SIM_VEC2_H

class Vec2 {
public:
    // Constructors
    Vec2();
    Vec2(float x, float y);

    // Getters and Setters
    float getX() const;
    void setX(float x);
    float getY() const;
    void setY(float y);

    // Basic operations
    Vec2 operator+(const Vec2& other) const;
    Vec2 operator-(const Vec2& other) const;
    Vec2 operator*(float scalar) const;
    Vec2 operator/(float scalar) const;

    // Dot and Cross Product
    float dot(const Vec2& other) const;
    float cross(const Vec2& other) const;

private:
    float x;
    float y;
};

// Type alias for clarity in agent-based simulation
using Pos2 = Vec2;

#endif // UUV_SIM_VEC2_H