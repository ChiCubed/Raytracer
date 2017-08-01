#include "df.h"

#include <cmath>
#include <limits>

const float PI = 3.1415926535f;

inline float min(float a, float b) {
    return a < b ? a : b;
}

inline float max(float a, float b) {
    return a > b ? a : b;
}

inline float mix(float x, float y, float a) {
    return x * (1-a) + y * a;
}

inline float clamp(float x, float a, float b) {
    // clamp x to range [a,b]
    // if b < a has undefined behaviour.
    return min(max(x,a),b);
}

inline float mod(float a, float b) {
    return a - b * floor(a/b);
}

inline float radians(float d) {
    return d / 180.0f * PI;
}

// http://www.iquilezles.org/www/articles/smin/smin.htm
float smin(float a, float b, float k) {
    float h = clamp(0.5f+0.5f*(b-a)/k, 0, 1);
    return mix(b, a, h) - k*h*(1.0 - h);
}

Eigen::Matrix4f yawPitchRollMatrix(const vec3& angle) {
    Eigen::Affine3f     yaw = (Eigen::Affine3f)Eigen::AngleAxisf(radians(angle(0)), vec3::UnitY()),
                      pitch = (Eigen::Affine3f)Eigen::AngleAxisf(radians(angle(1)), vec3::UnitX()),
                       roll = (Eigen::Affine3f)Eigen::AngleAxisf(radians(angle(2)), vec3::UnitZ());
   
    return (yaw*pitch*roll).matrix();
}

Eigen::Matrix4f translationMatrix(const vec3& trans) {
    Eigen::Matrix4f m;
    m << 1,0,0,trans(0),
         0,1,0,trans(1),
         0,0,1,trans(2),
         0,0,0,1;
    return m;
}

template<int I>
Eigen::Matrix<float, I, 1> clamp(const Eigen::Matrix<float, I, 1>& v, float a, float b) {
    return v.cwiseMax(a).cwiseMin(b);
}

template<int I>
Eigen::Matrix<float, I, 1> reflect(const Eigen::Matrix<float, I, 1>& x,
                                   const Eigen::Matrix<float, I, 1>& n) {
    return x - 2.0*n.dot(x)*n;
}

template<int I>
Eigen::Matrix<float, I, 1> mod(const Eigen::Matrix<float, I, 1>& a,
                               const Eigen::Matrix<float, I, 1>& b) {
    Eigen::Matrix<float, I, 1> result;
    for (int i = 0; i < I; ++i) {
        result(i) = mod(a(i),b(i));
    }
    return result;
}

template<int I>
Eigen::Vector2f  XY(const Eigen::Matrix<float, I, 1>& a) {
    return {a(0),a(1)};
}

template<int I>
Eigen::Vector2f  XZ(const Eigen::Matrix<float, I, 1>& a) {
    return {a(0),a(2)};
}

template<int I>
Eigen::Vector2f  YZ(const Eigen::Matrix<float, I, 1>& a) {
    return {a(1),a(2)};
}

template<int I>
Eigen::Vector3f XYZ(const Eigen::Matrix<float, I, 1>& a) {
    return {a(0),a(1),a(2)};
}



float sdSphere(const vec3& p, float r) { return p.norm() - r; }

float sdBox(const vec3& p, const vec3& size) {
    vec3 dist = p.cwiseAbs() - size;

    return min(max(dist(0), max(dist(1), dist(2))), 0.0f) +
        dist.cwiseMax(0.0f).norm();
}

float sdTorus(const vec3& p, const vec2& size) {
    vec2 q = {XZ(p).norm() - size(0), p(1)};
    return q.norm() - size(1);
}

float sdCylinder(const vec3& p, const vec3& size) {
    return (XZ(p) - XY(size)).norm() - size(2);
}

float sdCone(const vec2& p, const vec2& size) {
    vec2 q = {XY(p).norm(), p(2)};
    return size.dot(q);
}

float sdPlane(const vec3& p, const vec4& n) {
    return p.dot(XYZ(n)) + n(3);
}

float sdCapsule(const vec3& p, const vec3& a, const vec3& b, float r) {
    vec3 pa = p-a, ba = b-a;
    float h = clamp(pa.dot(ba)/ba.squaredNorm(), 0.0f, 1.0f);
    return (pa - ba*h).norm() - r;
}

float sdEllipsoid(const vec3& p, const vec3& r) {
    return (p.cwiseQuotient(r).norm() - 1.0f) * min(min(r(0),r(1)),r(2));
}

float opUnion(float da, float db) {
    return min(da,db);
}

float opSubtraction(float da, float db) {
    return max(-da, db);
}

float opIntersection(float da, float db) {
    return max(da, db);
}

float opBlend(float da, float db) {
    return smin(da, db, 0.2f);
}

vec3 opRepetition(const vec3& p, const vec3& c) {
    return mod(p,c) - c*0.5f;
}

vec3 opTranslate(const vec3& p, const vec3& trans) {
    return p - trans;
}

vec3 opTransform(const vec3& p, const Eigen::Matrix4f& m) {
    // vec4 dummy = {p(0),p(1),p(2),0.0f};
    // alternatively:
    // //         m.householderQr().solve(dummy);
    // vec4 tmp = m.inverse() * dummy;
    // return XYZ(tmp);

    // https://stackoverflow.com/a/2625420
    // Calculate the inverse of an affine matrix
    // provided it only contains rotations and translations.
    // Multiply it by p along the way.
    // Equivalent to inv(M) * (x - b) (see the stackoverflow answer)
    return m.block<3,3>(0,0).transpose() * (p - m.block<3,1>(0,3));
}

// vec3 opScale(const vec3& p, float s) { }