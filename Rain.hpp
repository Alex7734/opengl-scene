#ifndef RAIN_SYSTEM_HPP
#define RAIN_SYSTEM_HPP

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.hpp"

struct RainDrop {
    glm::vec3 position;
    glm::vec3 velocity;
    float spawnDelay;
    bool  isFalling;
};

class RainSystem {
public:
    RainSystem(int maxDropsCount, gps::Shader rainShader);

    void init();
    void update(float deltaTime);
    void uploadToGPU();

    void draw(const glm::mat4& projection, const glm::mat4& view);

    void destroy();

private:
    glm::vec3 randomSpawnAbove();

    gps::Shader rainShader;           
    std::vector<RainDrop> drops;

    GLuint VAO = 0;
    GLuint VBO = 0;
    int maxDrops;
    bool isInitialized;
};

#endif
