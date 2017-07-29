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

#include "df.cpp"
#include "render.cpp"

#include <cmath>

const int WIDTH = 640;
const int HEIGHT = 480;
const int FPS = 60;

const float FOV = 45.0f;

const float NEAR_DIST = 0.0f;
const float FAR_DIST  = 100.0f;

// these colours are between 0 and 1 rather
// than 0 and 255.
const vec3  AMBIENT_COLOUR  = {0.1,0.1,0.1},
            DIFFUSE_COLOUR  = {0.7,0.7,0.7},
            SPECULAR_COLOUR = {1.0,1.0,1.0};
const float SHININESS       = 8;

float scene(const vec3& p) {
    return sdSphere(p);
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
    int t = 0;

    Eigen::Vector3f cam = {0,2,5};

    std::vector<Light> lights = {{{5.0f,5.0f,0.0f},{0,1,1},0.6f},
                                 {{-5.0f,5.0f,0.0f},{1,0,1},0.6f}};
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

    // Specify vertex data layout
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    while (1) {
        const Uint64 start = SDL_GetPerformanceCounter();

        t = SDL_GetTicks() - t;
        if (t < 1000 / FPS) {
            SDL_Delay(1000/FPS - t);
        }

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
                               event.motion.xrel*0.5f,
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
            cam += XYZ(direction);
        }
        if (keyboardState[SDL_SCANCODE_DOWN] || keyboardState[SDL_SCANCODE_S]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){0,0,1,0};
            cam += XYZ(direction);
        }
        if (keyboardState[SDL_SCANCODE_LEFT] || keyboardState[SDL_SCANCODE_A]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){1,0,0,0};
            cam += XYZ(direction);
        }
        if (keyboardState[SDL_SCANCODE_RIGHT] || keyboardState[SDL_SCANCODE_D]) {
            Eigen::Vector4f direction = viewToWorld*(Eigen::Vector4f){-1,0,0,0};
            cam += XYZ(direction);
        }

        for (int i = 0; i < WIDTH; ++i) {
            Eigen::Vector3f viewDir, worldDir;
            Eigen::Vector4f tViewDir, eWorldDir;
            for (int j = 0; j < HEIGHT; ++j) {
                viewDir = direction(FOV, {WIDTH, HEIGHT}, {WIDTH-i, HEIGHT-j});

                tViewDir = {viewDir(0),viewDir(1),viewDir(2),1};
                eWorldDir = viewToWorld * tViewDir;
                worldDir = XYZ(eWorldDir);
                
                float dist = getDist(cam, worldDir, NEAR_DIST, FAR_DIST);
                
                if (dist > FAR_DIST - EPSILON) {
                    // nothing was hit
                } else {
                    vec3 c = lighting(AMBIENT_COLOUR, DIFFUSE_COLOUR, SPECULAR_COLOUR,
                                      SHININESS, cam + worldDir*dist, cam,
                                      ambientIntensity, lights);
                    c *= 255;
                }
            }
        }

        // Clear screen
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw triangles
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Present window
        SDL_GL_SwapWindow(window);

        const Uint64 end = SDL_GetPerformanceCounter();
        const static Uint64 freq = SDL_GetPerformanceFrequency();
        const double seconds = (end-start)/static_cast<double>(freq);
        std::cout << "Frame time: " << seconds * 1000.0 << "ms" << std::endl;
    }

    glDeleteProgram(shaderProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}