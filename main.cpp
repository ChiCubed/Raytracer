#include <iostream>
#include <vector>
#include <limits>

#include <Eigen/Dense>
#include <SDL2/SDL.h>

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

float scene(const Eigen::Vector3f& p) {
    // return opUnion(
    //         sdBox(p,{1,1,1}),
    //         opUnion(
    //         sdSphere(opTranslate(p,{2,0,0}),1.0f),
    //         sdPlane(p,{0,1,0,1})
    //         )
    // );
    return opUnion(
            sdSphere(opRepetition(p,{8,8,8}),2),//+sin((p(0)+p(1)+p(2))*5.0f)*0.1f,
            p(1)); //p(1) + 1.0f + sin(p(0)*5.0f)*0.2f + cos(p(2)*5.0f)*0.2f
}


void drawPixelToArray(unsigned char array[], int w, int h, int x, int y,
                      unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    int offset = 4 * (x + w*y);
    array[offset+2]=r;
    array[offset+1]=g;
    array[offset+0]=b;
    array[offset+3]=a;
}


int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Raymarcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_SetRelativeMouseMode(SDL_TRUE);


    bool quit = false;
    int t = 0;

    Eigen::Vector3f cam = {0,2,5};

    unsigned char* pixels = new unsigned char[WIDTH*HEIGHT*4];

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    std::vector<Light> lights = {{{5.0f,5.0f,0.0f},{0,1,1},0.6f},
                                 {{-5.0f,5.0f,0.0f},{1,0,1},0.6f}};
    float ambientIntensity = 0.5f;

    Eigen::Vector3f angle = {0, 0, 0};

    const Uint8 *keyboardState = SDL_GetKeyboardState(NULL);
    while (1) {
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

        const Uint64 start = SDL_GetPerformanceCounter();

        // speed things up a *bit*
        #pragma omp parallel for
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
                    drawPixelToArray(pixels, WIDTH, HEIGHT, i, j, 0, 0, 0, 255);
                } else {
                    vec3 c = lighting(AMBIENT_COLOUR, DIFFUSE_COLOUR, SPECULAR_COLOUR,
                                      SHININESS, cam + worldDir*dist, cam,
                                      ambientIntensity, lights);
                    c *= 255;
                    drawPixelToArray(pixels, WIDTH, HEIGHT, i, j, c(0), c(1), c(2), 255);
                }
            }
        }

        SDL_UpdateTexture(texture, NULL, pixels, WIDTH*4);
        
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        const Uint64 end = SDL_GetPerformanceCounter();
        const static Uint64 freq = SDL_GetPerformanceFrequency();
        const double seconds = (end-start)/static_cast<double>(freq);
        std::cout << "Frame time: " << seconds * 1000.0 << "ms" << std::endl;
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    delete[] pixels;
    pixels = NULL;

    SDL_Quit();

    return 0;
}