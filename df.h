#ifndef DISTANCE_FUNCTIONS_H
#define DISTANCE_FUNCTIONS_H

#include <Eigen/Dense>

using vec2 = Eigen::Vector2f;
using vec3 = Eigen::Vector3f;
using vec4 = Eigen::Vector4f;

// The distance functions are sourced from here:
// http://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

// Since we use Eigen, we must pass all our
// vectors / matrices by reference.

float sdSphere(const vec3& p, float r = 1.0f);

float sdBox(const vec3& p, const vec3& size = {1,1,1});

float sdTorus(const vec3& p, const vec2& size = {1,0});

float sdCylinder(const vec3& p, const vec3& size = {1,1,1});

// size must be normalized
float sdCone(const vec3& p, const vec2& size = {1,0});

// n must be normalized
float sdPlane(const vec3& p, const vec4& n = {0,1,0,0});

// a capsule / line with two points and a radius
float sdCapsule(const vec3& p, const vec3& a, const vec3& b, float r = 1.0f);

float sdEllipsoid(const vec3& p, const vec3& r = {1,1,1});



float opUnion(float da, float db);

float opSubtraction(float da, float db);

float opIntersection(float da, float db);


// How the following group of operations work:
// They all take a point as the first argument.
// It is transformed in such a way that it can
// now be used in a signed distance function.
// For example:
// sdSphere(opRepetition(p, c), r);
vec3 opRepetition(const vec3& p, const vec3& c);

// Essentially equivalent to creating a transformation
// matrix and calling opTransformMatrix...
// but a lot faster.
vec3 opTranslate(const vec3& p, const vec3& trans);

// Apply a transformation matrix.
// Use for rotation or translation.
// Don't use this for scaling.
vec3 opTransform(const vec3& p, const Eigen::Matrix4f& m);

// Scaling is done as follows:
// sdf(p/s)*s;
// Since we can't pass functions (yet...)
// and nothing is a class type, we
// don't have a scaling function.
// float opScale(const vec3& p, float s);

#endif