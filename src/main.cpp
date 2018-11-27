#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iterator>

using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int NUM_SNOWFLAKES = 500;
const int BITMAP_NUM_TEXTURES_PER_COL = 16;
const int BITMAP_NUM_TEXTURES_PER_ROW = 16;

SDL_Window *window = nullptr;
SDL_GLContext context = NULL;
GLuint programObj;
GLuint VAO, VBO, TBO, EBO;

// simulation variables
const glm::vec3 GRAVITY(0.f, -250.f, 0.f); 
const int AIR_RESISTANCE_FACTOR = 3 * GRAVITY.y / 4;
const int NUM_SCALES = 10;
const int MIN_SNOWFLAKE_SCALE = 4;
const int MIN_ROTATE_SPEED = 5;

bool init();
bool initGL();
void render(float delta);
void update(float delta);
void setupGLBuffers();
void loadTextures();
void setupSnowflakes();
void close();

struct Snowflake {
    glm::vec3 pos;          // position in world space
    int scale;               // scale factor in world space
    float rotationAngle;    // current rotation in degrees
    float rotationSpeed;    // degrees of rotation / second
    float textureCoords[8];

    Snowflake(glm::vec3 pos, int scale, float angle, float rotSpeed) : 
        pos(pos), scale(scale), rotationAngle(angle), rotationSpeed(rotSpeed) {}
};

vector<Snowflake> snowflakes;

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cout << "Failed to initialize SDL" << endl;
        return false;
    }

    int imgFlags = IMG_INIT_PNG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        cout << "Failed to initialize SDL_image" << endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow("Snowflakes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (window == nullptr) {
        cout << "Failed to create window" << endl;
        return false;
    }

    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        cout << "Failed to create context" << endl;
        return false;
    }

    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        cout << "Failed to initialize GLEW" << endl;
        return false;
    }

    if (!initGL()) {
        cout << "Failed to initialize OpenGL and shaders" << endl;
        return false;
    }
    return true;
}

string fileToString(const string& file) {
    ifstream ifs(file);
    stringstream ss;

    while (ifs >> ss.rdbuf());
    return ss.str();
}

void printShaderLog(GLuint shader) {
    if (glIsShader(shader) ) {
        int infoLogLength = 0;
        int maxLength = infoLogLength;

        glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &maxLength );
        char* infoLog = new char[ maxLength ];

        glGetShaderInfoLog( shader, maxLength, &infoLogLength, infoLog );
        if (infoLogLength > 0) {
            printf("%s\n", &(infoLog[0]));
        }

        delete[] infoLog;
    } else {
        cout << to_string(shader) << " is not a shader" << endl;
    }
}

bool initGL() {
    glEnable(GL_TEXTURE_2D);
    GLint compileSuccess;
    string temp;

    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    temp = fileToString("./shaders/vertex.glsl");
    const char* vShaderSource = temp.c_str();
    glShaderSource(vShader, 1, &vShaderSource, NULL);
    glCompileShader(vShader);

    glGetShaderiv(vShader, GL_COMPILE_STATUS, &compileSuccess);
    if (!compileSuccess) {
        cout << "Failed to compile vertex shader" << endl;
        printShaderLog(vShader);
        return false;
    }

    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    temp = fileToString("./shaders/fragment.glsl");
    const char* fShaderSource = temp.c_str();
    glShaderSource(fShader, 1, &fShaderSource, NULL);
    glCompileShader(fShader);

    glGetShaderiv(fShader, GL_COMPILE_STATUS, &compileSuccess);
    if (!compileSuccess) {
        cout << "Failed to compile fragment shader" << endl;
        printShaderLog(fShader);
        return false;
    }

    programObj = glCreateProgram();
    glAttachShader(programObj, vShader);
    glAttachShader(programObj, fShader);
    glLinkProgram(programObj);

    // flag shaders for deletion on program delete
    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return true;
}

void setupGLBuffers() {
    glCreateVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    const float rect[] = {
        -1, 1, 0,      
        1, 1, 0,      
        1, -1, 0,    
        -1, -1, 0,  
    };

    const int indices[] = {
        0, 3, 1,
        1, 2, 3
    };

    const float textureWidth = 1.f / (float) BITMAP_NUM_TEXTURES_PER_COL;
    const float textureHeight = 1.f / (float) BITMAP_NUM_TEXTURES_PER_ROW;

        float texCoords[] = {
            0, textureHeight,
            textureWidth, textureHeight,
            textureWidth, 0,
            0, 0
        };

    glCreateBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);

    glCreateBuffers(1, &TBO);

    glCreateBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // store buffer/pointer calls in the VAO
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, TBO);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void loadTextures() {
    GLuint snowflakeID;
    SDL_Surface* texture;
    SDL_Surface* snowFlakeImage = IMG_Load("snowflakes.png");
    glGenTextures(1, &snowflakeID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, snowflakeID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, snowFlakeImage->w, snowFlakeImage->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, snowFlakeImage->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    glUniform1i(glGetUniformLocation(programObj, "tex"), 0);
}

void setupSnowflakes() {

    const float textureWidth = 1.f / (float) BITMAP_NUM_TEXTURES_PER_COL;
    const float textureHeight = 1.f / (float) BITMAP_NUM_TEXTURES_PER_ROW;

    for (int i = 0; i < NUM_SNOWFLAKES; ++i) {
        int x = rand() % SCREEN_WIDTH;
        int y = rand() % SCREEN_HEIGHT;
        int rotationSpeed = rand() % 41 - 20;  // range [-20, 20] degrees / second

        // Ensure snowflakes have a minimum rotation speed
        if (rotationSpeed < 0) rotationSpeed -= MIN_ROTATE_SPEED;
        else rotationSpeed += MIN_ROTATE_SPEED;

        int scale;

        int textureIndex = i % 256;
        float texX = (textureIndex % BITMAP_NUM_TEXTURES_PER_COL) * textureWidth;
        float texY = (textureIndex / BITMAP_NUM_TEXTURES_PER_ROW) * textureHeight;

        float texCoords[] = {
            texX, texY + textureHeight,
            texX + textureWidth, texY + textureHeight,
            texX + textureWidth, texY,
            texX, texY 
        };

        // create 90% more "small" snowflakes than "large"
        if (rand() % 10 < 9) scale = rand() % NUM_SCALES + MIN_SNOWFLAKE_SCALE;
        else scale = rand() % NUM_SCALES + (NUM_SCALES + MIN_SNOWFLAKE_SCALE);

        Snowflake s = Snowflake(glm::vec3(x, y, 0), scale, 0, rotationSpeed);
        std::copy(std::begin(texCoords), std::end(texCoords), std::begin(s.textureCoords));
        snowflakes.push_back(s);
    }
}

void render(float delta) {
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(programObj);
    glBindVertexArray(VAO);

    glm::mat4 projection = glm::ortho(0.f, (float) SCREEN_WIDTH, 0.f, (float) SCREEN_HEIGHT, -1.f, 1.f);
    glm::mat4 view = glm::lookAt(
            glm::vec3(0.f, 0.f, 1.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f)
            );

    for (auto &s : snowflakes) {
        glBindBuffer(GL_ARRAY_BUFFER, TBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(s.textureCoords), s.textureCoords, GL_STATIC_DRAW);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, s.pos);
        model = glm::rotate(model, glm::radians(s.rotationAngle), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(s.scale, s.scale, 1.f));
        glm::mat4 mvp = projection * view * model;

        glUniformMatrix4fv(glGetUniformLocation(programObj, "mvp"), 1, GL_FALSE, &mvp[0][0]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void update(float delta) {
    const int maxScale = 2 * NUM_SCALES - 1;

    for (auto &s : snowflakes) {
        glm::vec3 vel = GRAVITY;
        vel.y -= (1.f - ((float) (s.scale - MIN_SNOWFLAKE_SCALE) / maxScale)) * AIR_RESISTANCE_FACTOR;
        s.pos += delta * vel;
        s.pos.x += 0.75 * s.rotationSpeed * glm::sin(2 * glm::radians(s.rotationAngle)) * delta;
        // reset position if off screen
        if (s.pos.y + s.scale <= 0 || s.pos.x + s.scale < 0 || s.pos.x - s.scale > SCREEN_WIDTH) {
            s.pos.x = rand() % SCREEN_WIDTH;
            s.pos.y = s.scale + SCREEN_HEIGHT;
        }
        s.rotationAngle += s.rotationSpeed * delta;
    }
}

void close() {
    cout << "Shutting down..." << endl;
    SDL_DestroyWindow(window);
    window = nullptr;

    glDeleteProgram(programObj);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    SDL_Quit();
}


int main(int argc, char ** argv) {
    if (init()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.f, 0.f, 0.f, 1.0f);

        bool quit = false;
        SDL_Event e;
        Uint32 ticks, prevTicks, prevFrameTicks;
        short frames = 0;
        prevTicks = SDL_GetTicks();
        prevFrameTicks = prevTicks;

        int texWidth, texHeight;
        setupGLBuffers();
        loadTextures();
        setupSnowflakes();

        SDL_StartTextInput();
        while (!quit) {
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) quit = true;
            }
            // performance measuring
            frames++;
            ticks = SDL_GetTicks();
            float deltaTime = (ticks - prevFrameTicks) / 1000.0; // change in time (seconds)
            prevFrameTicks = ticks;

            if (ticks - prevTicks >= 1000) { // for every second
                cout << to_string(1000.0 / frames) << " ms/frame" << endl;
                frames = 0;
                prevTicks = ticks;
            }

            update(deltaTime);
            render(deltaTime);


            SDL_GL_SwapWindow(window);
        }
        SDL_StopTextInput();
    }

    close();
    return 0;
}
