#include "Vector3.h"
#include <cmath>

namespace Revora {

const Vector3 Vector3::Zero    = {0.0f, 0.0f, 0.0f};
const Vector3 Vector3::Up      = {0.0f, 1.0f, 0.0f};
const Vector3 Vector3::Forward = {0.0f, 0.0f, 1.0f};
const Vector3 Vector3::Right   = {1.0f, 0.0f, 0.0f};

Vector3::Vector3(float x, float y, float z)
    : x(x), y(y), z(z) {}

Vector3 Vector3::operator+(const Vector3& rhs) const {
    return {x + rhs.x, y + rhs.y, z + rhs.z};
}

Vector3 Vector3::operator-(const Vector3& rhs) const {
    return {x - rhs.x, y - rhs.y, z - rhs.z};
}

Vector3 Vector3::operator*(float scalar) const {
    return {x * scalar, y * scalar, z * scalar};
}

Vector3 Vector3::operator/(float scalar) const {
    float inv = 1.0f / scalar;
    return {x * inv, y * inv, z * inv};
}

Vector3& Vector3::operator+=(const Vector3& rhs) {
    x += rhs.x; y += rhs.y; z += rhs.z;
    return *this;
}

Vector3& Vector3::operator-=(const Vector3& rhs) {
    x -= rhs.x; y -= rhs.y; z -= rhs.z;
    return *this;
}

Vector3& Vector3::operator*=(float scalar) {
    x *= scalar; y *= scalar; z *= scalar;
    return *this;
}

Vector3 Vector3::operator-() const {
    return {-x, -y, -z};
}

float Vector3::Length() const {
    return std::sqrt(x * x + y * y + z * z);
}

float Vector3::LengthSquared() const {
    return x * x + y * y + z * z;
}

Vector3 Vector3::Normalized() const {
    float len = Length();
    if (len < 1e-8f) {
        return Zero;
    }
    return *this / len;
}

float Vector3::Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Vector3::Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vector3 Vector3::Lerp(const Vector3& a, const Vector3& b, float t) {
    return a + (b - a) * t;
}

Vector3 operator*(float scalar, const Vector3& v) {
    return v * scalar;
}

} // namespace Revora
