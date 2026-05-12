#include "Matrix4x4.h"
#include "MathConstants.h"
#include <cmath>
#include <cstring>

namespace Revora {

Matrix4x4 Matrix4x4::Identity() {
    Matrix4x4 result;
    std::memset(&result, 0, sizeof(result));
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    return result;
}

Matrix4x4 Matrix4x4::Translation(float x, float y, float z) {
    Matrix4x4 result = Identity();
    // 行優先・行ベクトル方式: 平行移動は最終行に格納
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

Matrix4x4 Matrix4x4::Translation(const Vector3& v) {
    return Translation(v.x, v.y, v.z);
}

Matrix4x4 Matrix4x4::Scaling(float x, float y, float z) {
    Matrix4x4 result;
    std::memset(&result, 0, sizeof(result));
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1.0f;
    return result;
}

Matrix4x4 Matrix4x4::RotationX(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Matrix4x4 result = Identity();
    result.m[1][1] =  c;
    result.m[1][2] =  s;
    result.m[2][1] = -s;
    result.m[2][2] =  c;
    return result;
}

Matrix4x4 Matrix4x4::RotationY(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Matrix4x4 result = Identity();
    result.m[0][0] =  c;
    result.m[0][2] = -s;
    result.m[2][0] =  s;
    result.m[2][2] =  c;
    return result;
}

Matrix4x4 Matrix4x4::RotationZ(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Matrix4x4 result = Identity();
    result.m[0][0] =  c;
    result.m[0][1] =  s;
    result.m[1][0] = -s;
    result.m[1][1] =  c;
    return result;
}

Matrix4x4 Matrix4x4::LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
    // 左手座標系: z 軸はカメラ前方 (eye → target)
    Vector3 zAxis = (target - eye).Normalized();
    Vector3 xAxis = Vector3::Cross(up, zAxis).Normalized();
    Vector3 yAxis = Vector3::Cross(zAxis, xAxis);

    // 行優先・行ベクトル方式のビュー行列
    Matrix4x4 result;
    std::memset(&result, 0, sizeof(result));

    result.m[0][0] = xAxis.x;
    result.m[0][1] = yAxis.x;
    result.m[0][2] = zAxis.x;

    result.m[1][0] = xAxis.y;
    result.m[1][1] = yAxis.y;
    result.m[1][2] = zAxis.y;

    result.m[2][0] = xAxis.z;
    result.m[2][1] = yAxis.z;
    result.m[2][2] = zAxis.z;

    result.m[3][0] = -Vector3::Dot(xAxis, eye);
    result.m[3][1] = -Vector3::Dot(yAxis, eye);
    result.m[3][2] = -Vector3::Dot(zAxis, eye);
    result.m[3][3] = 1.0f;

    return result;
}

Matrix4x4 Matrix4x4::PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) {
    float h = 1.0f / std::tan(fovY * 0.5f);
    float w = h / aspect;
    float range = farZ / (farZ - nearZ);

    Matrix4x4 result;
    std::memset(&result, 0, sizeof(result));

    result.m[0][0] = w;
    result.m[1][1] = h;
    result.m[2][2] = range;
    result.m[2][3] = 1.0f;
    result.m[3][2] = -range * nearZ;

    return result;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const {
    Matrix4x4 result;
    std::memset(&result, 0, sizeof(result));
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += m[i][k] * rhs.m[k][j];
            }
        }
    }
    return result;
}

Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& rhs) {
    *this = *this * rhs;
    return *this;
}

Matrix4x4 Matrix4x4::Transposed() const {
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = m[j][i];
        }
    }
    return result;
}

} // namespace Revora
