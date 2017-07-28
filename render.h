#ifndef RENDER_RAYMARCHER_H
#define RENDER_RAYMARCHER_H

#include <vector>

#include <Eigen/Dense>

using vec2 = Eigen::Vector2f;
using vec3 = Eigen::Vector3f;
using vec4 = Eigen::Vector4f;

const int MAX_MARCH_STEPS = 128;
const float EPSILON = 0.001f;


struct Light {
    vec3 pos;
    vec3 colour;
    float intensity;
};


// algorithms nabbed from
// http://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions/

// This function is redefined by main to actually contain something.
float scene(const vec3& p);

// Get the distance from a camera to the surface of the scene.
// Scene is a function used to calculate the distance of the current
// position from the scene.
float getDist(const vec3& camera, const vec3& direction, float near, float far);

// Gets the normalized direction of the ray given the
// coordinate on screen.
vec3 direction(float fov, const vec2& screenSize, const vec2& coord);

// Transforms a ray from view space to world coords.
// Takes the target of the camera and its up vector.
Eigen::Matrix4f viewMatrix(const vec3& camera, const vec3& target, const vec3& up);

vec3 estimateNormal(const vec3& p);

// https://www.shadertoy.com/view/lt33z7
vec3 lighting(const vec3& k_a, const vec3& k_d, const vec3& k_s, float alpha, const vec3& p,
              const vec3& cam, float ambientIntensity, std::vector<Light> lights);

#endif