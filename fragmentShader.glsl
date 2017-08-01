#version 410 core

uniform vec3 cameraPos;
uniform mat4 viewToWorld; // converts a point in cam space to world space

uniform float time; // time in seconds
uniform vec2 screenSize; // screen resolution

// Because the uniform 'Lights'
// is laid out in std140 format,
// this is laid out as:
// vec3 pos
// float padding
// vec3 color
// float intensity
// No padding is required before
// intensity as color and intensity
// make a block of 16 bytes.
struct Light {
    vec3 pos;
    vec3 color; // no transparency
    float intensity;
};

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};


const int MAX_NUM_LIGHTS = 128;
const int MAX_NUM_DIRECTIONAL_LIGHTS = 128;
layout(std140) uniform LightBlock {
    Light lights[MAX_NUM_LIGHTS];
};
layout(std140) uniform DirectionalLightBlock {
    DirectionalLight directionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
};
uniform int numLights;
uniform int numDirectionalLights;



in vec4 gl_FragCoord;

out vec4 fragColor;

const float PI = 3.1415926535897932384626433832795;
const float stepScale = 0.90;
const float EPSILON = 0.001;

const float FOV = 45.0;

const int MAX_MARCH_STEPS = 128;


vec3 AMBIENT_COLOUR = vec3(0.1,0.1,0.1);
vec3 DIFFUSE_COLOUR = vec3(0.7,0.7,0.7);
vec3 SPECULAR_COLOUR = vec3(1.0,1.0,1.0);
float SHININESS = 8.0;
float ambientIntensity = 1.0;

float NEAR_DIST = EPSILON;
float FAR_DIST  = 128.0;






// https://gist.github.com/neilmendoza/4512992
// angle in radians
mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}



// Distance functions and helpers

// http://www.iquilezles.org/www/articles/smin/smin.htm
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b,a,h) - k*h*(1.0-h);
}

// http://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdSphere(vec3 p, float s) { return length(p) - s; }

float udBox(vec3 p, vec3 b) {
    return length(max(abs(p)-b,0.0));
}

float sdBox(vec3 p, vec3 b) {
    vec3 d = abs(p)-b;
    return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

float udRoundBox(vec3 p, vec3 b, float r) {
    return length(max(abs(p)-b,0.0))-r;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz)-t.x, p.y);
    return length(q) - t.y;
}

float sdCylinder(vec3 p, vec3 c) {
    return length(p.xz-c.xy) - c.z;
}

float sdCone(vec3 p, vec2 c) {
    // c must be normalized
    float q = length(p.xy);
    return dot(c, vec2(q,p.z));
}

float sdPlane(vec3 p, vec4 n) {
    // n must be normalized
    return dot(p, n.xyz) + n.w;
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p-a, ba = b-a;
    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*h)-r;
}

// These functions take in
// two distances and perform
// operations.
float opU(float d1, float d2) { return min(d1, d2); }
float opS(float d1, float d2) { return max(-d1, d2); }
float opI(float d1, float d2) { return max(d1, d2); }
float opBlend(float d1, float d2) { return smin(d1, d2, 0.1); }


// These functions take as arguments
// vectors and return them in a deformed
// state.
vec3 opRep(vec3 p, vec3 c) {
    return mod(p,c) - 0.5*c;
}

// trans stands for 'transform', not 'translate'
vec3 opTrans(vec3 p, mat4 m) {
    // https://stackoverflow.com/a/2625420
    // inverse of an affine matrix
    // (m)
    // this obviously assumes m is affine
    return transpose(mat3(m)) * (p - m[3].xyz);
}


// These functions relate to scene definition
// and calculations.

// Scene SDF
// inigo quillez
float mandelbulb(vec3 p) {
    vec3 w = p;
    float m = dot(w,w);
	float dz = 1.0;
    
	for(int i=0; i<4; i++){
#if 1
        float m2 = m*m;
        float m4 = m2*m2;
		dz = 8.0*sqrt(m4*m2*m)*dz + 1.0;

        float x = w.x; float x2 = x*x; float x4 = x2*x2;
        float y = w.y; float y2 = y*y; float y4 = y2*y2;
        float z = w.z; float z2 = z*z; float z4 = z2*z2;

        float k3 = x2 + z2;
        float k2 = inversesqrt( k3*k3*k3*k3*k3*k3*k3 );
        float k1 = x4 + y4 + z4 - 6.0*y2*z2 - 6.0*x2*y2 + 2.0*z2*x2;
        float k4 = x2 - y2 + z2;

        w.x = p.x +  64.0*x*y*z*(x2-z2)*k4*(x4-6.0*x2*z2+z4)*k1*k2;
        w.y = p.y + -16.0*y2*k3*k4*k4 + k1*k1;
        w.z = p.z +  -8.0*y*k4*(x4*x4 - 28.0*x4*x2*z2 + 70.0*x4*z4 - 28.0*x2*z2*z4 + z4*z4)*k1*k2;
#else
		dz = 8.0*pow(m,3.5)*dz + 1.0;
        
        float r = length(w);
        float b = 8.0*acos( clamp(w.y/r, -1.0, 1.0));
        float a = 8.0*atan( w.x, w.z );
        w = p + pow(r,8.0) * vec3( sin(b)*sin(a), cos(b), sin(b)*cos(a) );
#endif  

        m = dot(w,w);
		if(m > 4.0)break;
    }

    return 0.25*log(m)*sqrt(m)/dz;
}

float scene(vec3 p) {
    float spheres = sdSphere(vec3(mod(p.x,1.0) - 0.5, p.y, mod(p.z,1.0) - 0.5),0.3);
    float plane = p.y + 0.3;
    float bulb = mandelbulb(p*0.1-vec3(0,2,0))*10.0;
    return opBlend(plane,opU(bulb,spheres));
}

// normal estimation
vec3 estimateNormal(vec3 p) {
    vec2 eps = vec2(EPSILON, 0.0);
    return normalize(vec3(
                scene(p+eps.xyy) - scene(p-eps.xyy),
                scene(p+eps.yxy) - scene(p-eps.yxy),
                scene(p+eps.yyx) - scene(p-eps.yyx)));
}

float castRay(vec3 camera, vec3 direction, float near, float far) {
    float depth = near;
    for (int i=0; i<MAX_MARCH_STEPS; ++i) {
        float dist = scene(camera + depth*direction) * stepScale;
        if (abs(dist) < EPSILON) return depth;
        depth += dist;
        if (depth >= far) break;
    }
    return far;
}

// Pixel color calculation functions

// This function takes a point and the direction to the light.
// It approximates a soft shadow.
float shadow(vec3 p, vec3 direction, float near, float far, float k) {
    float depth = near;
    float res = 1.0;
    float step = far / float(MAX_MARCH_STEPS / 4);
    for (;depth<far;) {
        float dist = scene(p + depth*direction);
        res = min(res, k*dist/depth);

        depth += min(dist, step*8.0);
        if (dist < EPSILON || depth >= far) {
            break;
        }
    }
    return clamp(res, 0.0, 1.0);
}

// phong lighting with attenuation
vec3 phongLighting(vec3 k_d, vec3 k_s, float alpha, vec3 p, vec3 normal,
                   vec3 cam, vec3 viewerNormal, vec3 lightPos, vec3 lightColor, float intensity) {
    vec3 N = normal;
    // for attenuation calculation
    vec3 relativePos = lightPos - p;
    vec3 L = normalize(relativePos);
    vec3 V = viewerNormal;
    vec3 R = normalize(reflect(-L, N));

    float dotLN = dot(L,N);
    float dotRV = dot(R,V);

    if (dotLN < 0.0) {
        // The surface receives no light
        return vec3(0.0);
    }

    float squareDist = dot(relativePos, relativePos);

    float reciprocalLightRangeSquared = 0.003;
    float attenuatedIntensity = 1.0 - squareDist * reciprocalLightRangeSquared;

    if (attenuatedIntensity < 0.0) {
        // It's a completely unlit point
        return vec3(0.0);
    }

    attenuatedIntensity *= attenuatedIntensity * intensity;

    if (dotRV < 0.0) {
        return (k_d*dotLN)*lightColor * attenuatedIntensity;
    }

    int gamma = 8;

    // Repeated squaring
    // of the specular term
    int gamma_log2 = 3;
    float calculated_specular = (1.0 - alpha * (1.0-dotRV)/float(gamma));
    for (int i=0; i<gamma_log2; ++i) {
        calculated_specular *= calculated_specular;
    }

    return (k_d*dotLN + k_s*calculated_specular)*lightColor * attenuatedIntensity;
}

// directional phong
vec3 directionalPhongLighting(vec3 k_d, vec3 k_s, float alpha, vec3 p, vec3 normal,
                   vec3 cam, vec3 viewerNormal, vec3 lightDirection, vec3 lightColor, float intensity) {
    vec3 N = normal;
    vec3 L = -lightDirection;
    vec3 V = viewerNormal;
    vec3 R = normalize(reflect(-L, N));

    float dotLN = dot(L,N);
    float dotRV = dot(R,V);

    if (dotLN < 0.0) return vec3(0.0);
    if (dotRV < 0.0) return (k_d*dotLN)*lightColor * intensity;

    int gamma = 8;
    int gamma_log2 = 3;
    float calculated_specular = (1.0 - alpha * (1.0-dotRV)/float(gamma));
    for (int i=0; i<gamma_log2; ++i) {
        calculated_specular *= calculated_specular;
    }

    return (k_d*dotLN + k_s*calculated_specular)*lightColor * intensity;
}

// gets a pixel color
vec3 lighting(vec3 k_a, vec3 k_d, vec3 k_s, float alpha, vec3 p,
              vec3 cam, float ambientIntensity,
              Light lights[MAX_NUM_LIGHTS], int numLights) {
    vec3 color = k_a * ambientIntensity;

    vec3 normal = estimateNormal(p);
    vec3 viewerNormal = normalize(cam - p);

    for (int i=0; i<numLights; ++i) {
        vec3 tmp = phongLighting(k_d, k_s, alpha, p, normal, cam, viewerNormal,
                                 lights[i].pos, lights[i].color, lights[i].intensity);
        
        if (tmp != vec3(0.0)) {
            vec3 d = lights[i].pos - p;
            float len = length(d);
            tmp *= shadow(p, d/len, EPSILON*16.0,
                          len, 8.0);
        }

        color += tmp;
    }

    // and now directional lighting
    for (int i=0; i<numDirectionalLights; ++i) {
        vec3 tmp = directionalPhongLighting(k_d, k_s, alpha, p, normal, cam, viewerNormal,
                                 directionalLights[i].direction, directionalLights[i].color, directionalLights[i].intensity);
        
        if (tmp != vec3(0.0)) {
            // The shadow 'far' plane is set arbitrarily to 100.
            tmp *= shadow(p, -directionalLights[i].direction, EPSILON*16.0,
                          100, 8.0);
        }

        color += tmp;
    }

    return clamp(color, 0.0, 1.0);
}

// These are view / world space calculation functions.

vec2 normalizeCoordinates(vec2 coord, vec2 size) {
    coord /= size;
    coord *= 2.0;
    coord -= 1.0;
    return coord;
}

vec2 centerCoordinates(vec2 coord, vec2 size) {
    return coord - 0.5*size;
}

// returns direction of the ray
// given fov, screen coordinate and size.
vec3 direction(float fov, vec2 coord, vec2 size) {
    vec2 xy = centerCoordinates(coord, size);
    float z = screenSize.y / tan(radians(fov) * 0.5);
    vec3 tmp = vec3(xy.xy, -z);
    return normalize(tmp);
}

void main() {
    vec3 rayDir = direction(FOV, gl_FragCoord.xy, screenSize);
    vec3 worldDir = (viewToWorld * vec4(rayDir,1.0)).xyz;
    
    float dist = castRay(cameraPos, worldDir, NEAR_DIST, FAR_DIST);
                
    if (dist > FAR_DIST - EPSILON) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        vec3 c = lighting(AMBIENT_COLOUR, DIFFUSE_COLOUR, SPECULAR_COLOUR,
                          SHININESS, cameraPos + worldDir*dist, cameraPos,
                          ambientIntensity, lights, numLights);
        fragColor = vec4(c, 1.0);
    }
}