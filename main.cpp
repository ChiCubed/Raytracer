#include <iostream>
#include <fstream>
#include <vector>
#include <limits>

#include <Eigen/Dense>
#include <SDL2/SDL.h>

#define GL3_PROTOTYPES 1

// include a different OpenGL path based on OS.
#if defined(__APPLE__) && defined(__MACH__)
    #include <OpenGL/gl3.h>
#else
    #ifdef _WIN32
        #include <windows.h>
    #endif

    #include <GL/gl3.h>
#endif

#include <cmath>

const int WIDTH = 640;
const int HEIGHT = 480;
const int FPS = 120;

const int MAX_NUM_LIGHTS = 128;
const int MAX_NUM_DIRECTIONAL_LIGHTS = 128;

const float PI = 3.1415926535897932384626433832795;

float radians(float angle) {
    return angle * PI / 180.0f;
}

Eigen::Matrix4f yawPitchRollMatrix(const Eigen::Vector3f& angle) {
    Eigen::Affine3f     yaw = (Eigen::Affine3f)Eigen::AngleAxisf(radians(angle(0)), Eigen::Vector3f::UnitY()),
                      pitch = (Eigen::Affine3f)Eigen::AngleAxisf(radians(angle(1)), Eigen::Vector3f::UnitX()),
                       roll = (Eigen::Affine3f)Eigen::AngleAxisf(radians(angle(2)), Eigen::Vector3f::UnitZ());
   
    return (yaw*pitch*roll).matrix();
}

// The padding exists
// for communication with GLSL.
struct Light {
    Eigen::Vector3f pos;
    GLfloat padding;
    Eigen::Vector3f color;
    GLfloat intensity;
};

struct DirectionalLight {
    Eigen::Vector3f direction;
    GLfloat padding;
    Eigen::Vector3f color;
    GLfloat intensity;
};

Light createLight(const Eigen::Vector3f& pos, const Eigen::Vector3f& color, float intensity) {
    return {pos, 0, color, intensity};
}

DirectionalLight createDirectionalLight(const Eigen::Vector3f& direction,
                                        const Eigen::Vector3f& color, float intensity) {
    return {direction, 0, color, intensity};
}


int main(int argc, char* argv[]) {
    // SDL setup stuff
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Raymarcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
    

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetSwapInterval(1);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);

    SDL_SetRelativeMouseMode(SDL_TRUE);

    // Program variables
    bool quit = false;

    Eigen::Vector3f cam = {0,0,5};

    std::vector<Light> lights = {
                                 createLight({ 5.0f,5.0f,0.0f},{0,1,1},0.6f),
                                 createLight({-5.0f,5.0f,0.0f},{1,0,1},0.6f)
                                 };
    std::vector<DirectionalLight> directionalLights = {
                                 createDirectionalLight({0.0f,-1.0f,0.0f},{1,1,1},0.6f)
                                 };
    float ambientIntensity = 0.5f;

    Eigen::Vector3f angle = {0, 0, 0};

    const Uint8 *keyboardState = SDL_GetKeyboardState(NULL);

    // Quad
    GLfloat vertices[] = {
        -1.0f, 1.0f, // top left
         1.0f, 1.0f, // top right
         1.0f,-1.0f, // bottom right
        -1.0f,-1.0f  // bottom left
    };

    // NOTE:
    //   basically all of this is sourced from
    //   https://open.gl/drawing,
    //   a tutorial website for OpenGL.

    // Create a Vertex Array Object.
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);

    // Create a Vertex Buffer Object.
    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Create an Element Buffer Object.
    GLuint ebo;
    glGenBuffers(1, &ebo);

    GLuint elements[] = {
        0, 1, 2, // Indices of the triangle verts
        2, 3, 0  // in the vertices[] array.
    };

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);


    // read vertex and fragment shaders
    std::ifstream vertFile("vertexShader.glsl");
    std::ifstream fragFile("fragmentShader.glsl");

    std::string tmpVert((std::istreambuf_iterator<char>(vertFile)),
                         std::istreambuf_iterator<char>());
    const GLchar* vertSource = tmpVert.c_str();

    std::string tmpFrag((std::istreambuf_iterator<char>(fragFile)),
                         std::istreambuf_iterator<char>());
    const GLchar* fragSource = tmpFrag.c_str();

    // compile vertex and fragment shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragSource, NULL);
    glCompileShader(fragmentShader);

    // error check the fragment shader
    GLint val = GL_FALSE;
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &val);
    if (val != GL_TRUE) return 1;

    // combine to a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    // this tells OpenGL to write
    // outColor to location 0.
    glBindFragDataLocation(shaderProgram, 0, "fragColor");

    // use shader
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);

    // Add uniform for camera, to allow
    // CPU and GPU communication.
    GLint camUniform = glGetUniformLocation(shaderProgram, "cameraPos");
    // Also a uniform for the projection matrix.
    GLint viewToWorldUniform = glGetUniformLocation(shaderProgram, "viewToWorld");
    // And for the time
    GLint timeUniform = glGetUniformLocation(shaderProgram, "time");
    // And for the screen size
    GLint screenSizeUniform = glGetUniformLocation(shaderProgram, "screenSize");
    // And for the number of lights
    GLint numLightsUniform = glGetUniformLocation(shaderProgram, "numLights");
    // And for the number of directional lights
    GLint numDirectionalLightsUniform = glGetUniformLocation(shaderProgram, "numDirectionalLights");

    // Specify vertex data layout
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // lights UBO
    GLuint lightsUbo, lightBlockIndex;
    GLuint lightBindingPoint = 0;

    lightBlockIndex = glGetUniformBlockIndex(shaderProgram, "LightBlock");
    glUniformBlockBinding(shaderProgram, lightBlockIndex, lightBindingPoint);

    glGenBuffers(1, &lightsUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, lightsUbo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Light)*MAX_NUM_LIGHTS, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferRange(GL_UNIFORM_BUFFER, lightBindingPoint, lightsUbo,
                      0, sizeof(Light)*MAX_NUM_LIGHTS);

    // directional lights UBO
    GLuint directionalLightsUbo, directionalLightBlockIndex;
    GLuint directionalLightBindingPoint = 1;

    directionalLightBlockIndex = glGetUniformBlockIndex(shaderProgram, "DirectionalLightBlock");
    glUniformBlockBinding(shaderProgram, directionalLightBlockIndex, directionalLightBindingPoint);

    glGenBuffers(1, &directionalLightsUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, directionalLightsUbo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalLight)*MAX_NUM_DIRECTIONAL_LIGHTS, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBufferRange(GL_UNIFORM_BUFFER, directionalLightBindingPoint, directionalLightsUbo,
                      0, sizeof(DirectionalLight)*MAX_NUM_DIRECTIONAL_LIGHTS);

    int ct = SDL_GetTicks(), lt;
    int it = ct; // initial time
    while (1) {
        lt = ct;
        ct = SDL_GetTicks();
        if (ct-lt < 1000 / FPS) SDL_Delay(1000 / FPS - ct+lt);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit = true;
                            break;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    angle += (Eigen::Vector3f){
                              -event.motion.xrel*0.5f,
                              -event.motion.yrel*0.5f,
                               0};
                    break;
            }
        }

        if (quit) {
            break;
        }

        // maps a view coordinate to a world
        // coordinate, based on the angle of
        // the camera
        
        Eigen::Matrix4f viewToWorld = yawPitchRollMatrix(angle);

        if (keyboardState[SDL_SCANCODE_UP] || keyboardState[SDL_SCANCODE_W]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){0,0,-1,0};
            cam += direction.block<3,1>(0,0)*0.2;
        }
        if (keyboardState[SDL_SCANCODE_DOWN] || keyboardState[SDL_SCANCODE_S]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){0,0, 1,0};
            cam += direction.block<3,1>(0,0)*0.2;
        }
        if (keyboardState[SDL_SCANCODE_LEFT] || keyboardState[SDL_SCANCODE_A]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){-1,0,0,0};
            cam += direction.block<3,1>(0,0)*0.2;
        }
        if (keyboardState[SDL_SCANCODE_RIGHT] || keyboardState[SDL_SCANCODE_D]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){ 1,0,0,0};
            cam += direction.block<3,1>(0,0)*0.2;
        }

        glUniform3fv(camUniform, 1, cam.data());
        // GL_TRUE in the expression below means we convert
        // the Eigen row-major matrix to an
        // OpenGL column-major matrix.
        glUniformMatrix4fv(viewToWorldUniform, 1, GL_FALSE, viewToWorld.data());
        glUniform1f(timeUniform, (it - ct) / 1000.0f);
        glUniform2f(screenSizeUniform, WIDTH, HEIGHT);
        glUniform1i(numLightsUniform, lights.size());
        // lightsUbo
        glBindBuffer(GL_UNIFORM_BUFFER, lightsUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Light)*lights.size(), lights.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        // directionalLightsUbo

        glUniform1i(numDirectionalLightsUniform, directionalLights.size());
        glBindBuffer(GL_UNIFORM_BUFFER, directionalLightsUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0,
                        sizeof(DirectionalLight)*directionalLights.size(), directionalLights.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw triangles
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Present window
        SDL_GL_SwapWindow(window);

        std::cout << "Frame time: " << ct-lt << "ms" << std::endl;
    }

    glDeleteProgram(shaderProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);

    glDeleteBuffers(1, &lightsUbo);
    glDeleteBuffers(1, &directionalLightsUbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}