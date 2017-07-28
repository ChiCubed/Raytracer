#include "render.h"

#include <cmath>

const float stepScale = 0.95;

float getDist(const vec3& camera, const vec3& direction, float near, float far) {
    float depth = near;
    for (int i = 0; i < MAX_MARCH_STEPS; i++) {
        float dist = scene(camera + depth * direction) * stepScale;
        if (abs(dist) < EPSILON) return depth;
        depth += dist;
        if (depth >= far) break;
    }
    return far;
}

vec3 direction(float fov, const vec2& screenSize, const vec2& coord) {
    vec2 xy = coord - screenSize * 0.5f;
                                    // radians(fov) * 0.5f
    float z = screenSize(1) / tan(fov * 90.0f / PI);

    vec3 tmp = {xy(0),xy(1),-z};
    return tmp.normalized();
}

Eigen::Matrix4f viewMatrix(const vec3& camera, const vec3& target, const vec3& up) {
    vec3 f = (target-camera).normalized();
    vec3 s = f.cross(up).normalized();
    vec3 u = s.cross(f);
    Eigen::Matrix4f m;
    m <<   s(0), u(0),-f(0),    0,
           s(1), u(1),-f(1),    0,
           s(2), u(2),-f(2),    0,
              0,    0,    0,    1;
    return m;
}

vec3 estimateNormal(const vec3& p) {
    vec3 n = {scene(p+vec3::UnitX()*EPSILON) - scene(p-vec3::UnitX()*EPSILON),
              scene(p+vec3::UnitY()*EPSILON) - scene(p-vec3::UnitY()*EPSILON),
              scene(p+vec3::UnitZ()*EPSILON) - scene(p-vec3::UnitZ()*EPSILON)};
    return n.normalized();
}

float shadow(const vec3& p, const vec3& direction, float near, float far, float k) {
    float depth = near;
    float res = 1.0;
    float step = far / (MAX_MARCH_STEPS/4.0f);
    // for (int i=0; i < MAX_MARCH_STEPS; ++i)
    for (;depth < far;) {
        float dist = scene(p + depth * direction);
        res = min(res, k*dist/depth);

        // Cheat a bit to make our soft shadows more beautiful.
        depth += min(dist, step*8.0f);//clamp(dist,EPSILON,far*8/MAX_MARCH_STEPS);
        if (dist < EPSILON || depth >= far) {
            break;
        }
    }
    return clamp(res,0.0f,1.0f);
}

vec3 phongContrib(const vec3& k_d, const vec3& k_s, float alpha, const vec3& p, const vec3& normal,
                  const vec3& cam, const vec3& viewerNormal,
                  const vec3& lightPos, const vec3& lightColour, float intensity) {
    vec3 N = normal;
    // relativePos is used for attenuation calc
    vec3 relativePos = lightPos - p;
    vec3 L = relativePos.normalized();
    vec3 V = viewerNormal;
    vec3 R = reflect((vec3)-L, N).normalized();

    float dotLN = L.dot(N);
    float dotRV = R.dot(V);

    if (dotLN < 0.0) {
        return vec3::Zero();
    }

    // Completely accurate phong below. Probably slower.
    //  return (k_d * dotLN + dotRV<0.0 ? vec3::Zero() : (vec3)(k_s * powf(dotRV, alpha))) * intensity;
    
    // Let's calculate the attenuation!
    float squareDist = relativePos.squaredNorm();
    // Replace 0.001f with 1 / (light range squared)
    float attenuatedIntensity = 1.0f - squareDist*0.003f;

    if (attenuatedIntensity <= 0.0f) {
        // Completely unlit
        return vec3::Zero();
    }

    attenuatedIntensity *= attenuatedIntensity * intensity;

    // The following code uses approximations.
    if (dotRV < 0.0) {
        // no specular term
        return (k_d.cwiseProduct(lightColour)*dotLN) * attenuatedIntensity;
    }

    int gamma = 8;

    // the following code is slower
    // float calculated_specular = std::pow((1 - alpha*(1-dotRV)/gamma), gamma);

    // faster calculation of the specular term:
    // repeated squaring.
    // this method obviously only works when
    // gamma is a power of two.
    int gamma_log2 = 3;
    float calculated_specular = (1 - alpha*(1-dotRV)/gamma);
    for (int i=0; i<gamma_log2; ++i) {
        calculated_specular *= calculated_specular;
    }

    return (k_d*dotLN + (vec3)(k_s * calculated_specular)).cwiseProduct(lightColour) * attenuatedIntensity;
}

vec3 lighting(const vec3& k_a, const vec3& k_d, const vec3& k_s, float alpha,
              const vec3& p, const vec3& cam, float ambientIntensity, std::vector<Light> lights) {
    vec3 colour = k_a * ambientIntensity;

    vec3 normal = estimateNormal(p);
    vec3 viewerNormal = (cam - p).normalized();

    for (int i = 0; i < lights.size(); ++i) {
        vec3 tmp = phongContrib(k_d, k_s, alpha, p, normal, cam, viewerNormal,
                                lights[i].pos, lights[i].colour, lights[i].intensity);

        // We cheat a bit more to make sure our soft shadows look good.
        if (tmp != vec3::Zero()) {
            tmp *= shadow(p, (lights[i].pos-p).normalized(), EPSILON*32,
                          (lights[i].pos-p).norm() - EPSILON*32, 8);
        }
        
        colour += tmp;
    }

    return clamp(colour,0.0f,1.0f);
}