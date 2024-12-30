#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif
#pragma comment(lib, "winmm.lib")

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.hpp"
#include "Model3D.hpp"
#include "Camera.hpp"
#include "SkyBox.hpp"
#include "Rain.hpp" 

#include <iostream>
#include <windows.h>

int glWindowWidth = 1280.0f;
int glWindowHeight = 720.0f;
int retina_width, retina_height;
GLFWwindow* glWindow = NULL;
float lastTimeStamp = 0.0;

const unsigned int SHADOW_WIDTH = 9048;
const unsigned int SHADOW_HEIGHT = 9048;

// Matrices
glm::mat4 model;
GLuint modelLoc;
glm::mat4 view;
GLuint viewLoc;
glm::mat4 projection;
GLuint projectionLoc;
glm::mat3 normalMatrix;
GLuint normalMatrixLoc;

// ----------------------------------------------------------------------
// The "sun" (directional light)
// ----------------------------------------------------------------------
float lightAngle = 0.0f;
glm::mat4 lightRotation;

glm::vec3 baseSunPos(-140.0f, 60.0f, 70.0f);
glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);

glm::vec3  lightDir;
GLuint     lightDirLoc;

glm::vec3 lightColor;
GLuint     lightColorLoc;

// ----------------------------------------------------------------------
// Camera
// ----------------------------------------------------------------------
gps::Camera myCamera(
    glm::vec3(0.0f, 40.0f, 5.5f),
    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f)
);
float cameraSpeed = 0.5f;
bool pressedKeys[1024];
bool firstMouse = true;
float lastX = glWindowWidth / 2.0f;
float lastY = glWindowHeight / 2.0f;
float yaw = -90.0f;
float pitch = 0.0f;

// ----------------------------------------------------------------------
// Models
// ----------------------------------------------------------------------
gps::Model3D scene;
gps::Model3D lightCube;
gps::Model3D screenQuad;
gps::Model3D heli;

// ----------------------------------------------------------------------
// Shaders
// ----------------------------------------------------------------------
gps::Shader myCustomShader;   
gps::Shader lightShader;
gps::Shader screenQuadShader;
gps::Shader depthMapShader;
gps::Shader skyboxShader;
gps::Shader rainShader;

// ----------------------------------------------------------------------
// Shadows
// ----------------------------------------------------------------------
GLuint shadowMapFBO;
GLuint depthMapTexture;
glm::mat4 lightSpaceTrMatrix;
bool showDepthMap = false;

// ----------------------------------------------------------------------
// Point Lights 
// ----------------------------------------------------------------------
#define NUM_OF_POINT_LIGHTS 4
glm::vec3 gPointLightPositions[NUM_OF_POINT_LIGHTS] = {
    glm::vec3(-111.0f, 11.0f, 15.0f),
    glm::vec3(-25.0f, 14.66f, -3.0f),
    glm::vec3(15.1f, 12.7f, -57.5f),
    glm::vec3(75.24, 14.8, -2.65)
};

glm::vec3 gPointLightColor = glm::vec3(1.0f, 0.9f, 0.7f);
float gPointLightAmbient = 0.2f;
float gPointLightDiffuse = 1.0f;
float gPointLightSpecular = 1.0f;
float gConstantAtt = 1.0f;
float gLinearAtt = 0.02f;
float gQuadraticAtt = 0.001f;
bool  pointLightEnabled = false; 


// ----------------------------------------------------------------------
// Skybox
// ----------------------------------------------------------------------
std::vector<const GLchar*> faces;
gps::SkyBox mySkyBox;

// ----------------------------------------------------------------------
// Helicopter
// ----------------------------------------------------------------------
float heliAngle = 0.0f;
bool helicopterRotating = false;
float deltaTimeHeli;

// ----------------------------------------------------------------------
// Camera animation
// ----------------------------------------------------------------------
std::vector<glm::vec3> cameraPathPoints = {
    glm::vec3(-220.463f, 76.1223f, 60.5421f),
    glm::vec3(-188.463f, 68.1223f, 55.5421f),
    glm::vec3(-135.346f, 18.5052f, 21.6539f),
    glm::vec3(-79.5788f, 26.3867f, -9.06033f),
    glm::vec3(177.952f, 93.1747f, -34.8625f)
};

int  currentPathIndex = 0;
bool cameraAnimationActive = false;
float travelTime = 3.0f;
float pauseTime = 4.0f;
float segmentTimer = 0.0f;
bool isTraveling = true;

// ----------------------------------------------------------------------
// Night / Day Mode
// ----------------------------------------------------------------------
bool nightMode = false;
glm::vec3 daySunColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 nightSunColor = glm::vec3(0.2f, 0.2f, 0.4f);

// ----------------------------------------------------------------------
// ----------[ Fog, Wind, Rain & Thunder ]-------------------------------
// ----------------------------------------------------------------------
bool  fogEnabled = false;         
float gFogDensity = 0.02f;         
glm::vec4 gFogColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

bool windEnabled = false;
bool rainEnabled = false;

bool isThunderActive = false;
float thunderTimer = 0.0f;
float nextThunderInterval = 10.0f;
float thunderDuration = 1.0; 
float thunderStartTime = 0.0f;

RainSystem gRain(0, rainShader);

// ----------------------------------------------------------------------
// Check for GL errors
// ----------------------------------------------------------------------
GLenum glCheckError_(const char* file, int line) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
        case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
        case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FBO";   break;
        default:                               error = "UNKNOWN";       break;
        }
        std::cerr << "[OpenGL Error] " << error << " | " << file << " (" << line << ")\n";
    }
    return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

// ----------------------------------------------------------------------
// Day / Night handler
// ----------------------------------------------------------------------
void handleDayAndNightMode() {
    faces.clear();
    if (nightMode) {
        faces.push_back("skybox/marslike01rt.tga");
        faces.push_back("skybox/marslike01lf.tga");
        faces.push_back("skybox/marslike01up.tga");
        faces.push_back("skybox/marslike01dn.tga");
        faces.push_back("skybox/marslike01bk.tga");
        faces.push_back("skybox/marslike01ft.tga");
    }
    else {
        faces.push_back("skybox/desertsky_rt.tga");
        faces.push_back("skybox/desertsky_lf.tga");
        faces.push_back("skybox/desertsky_up.tga");
        faces.push_back("skybox/desertsky_dn.tga");
        faces.push_back("skybox/desertsky_bk.tga");
        faces.push_back("skybox/desertsky_ft.tga");
    }

    mySkyBox.Load(faces);

    lightColor = nightMode ? nightSunColor : daySunColor;
    myCustomShader.useShaderProgram();
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));
}


// ----------------------------------------------------------------------
// Mouse, Window & Keyboard callbacks
// ----------------------------------------------------------------------
void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        firstMouse = false;
        lastX = (float)xpos;
        lastY = (float)ypos;
    }

    float x_offset = (float)xpos - lastX;
    float y_offset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.1f;
    x_offset *= sensitivity;
    y_offset *= sensitivity;

    yaw += x_offset;
    pitch += y_offset;

    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    myCamera.rotate(pitch, yaw);
}
void windowResizeCallback(GLFWwindow* window, int width, int height)
{
    glWindowWidth = width;
    glWindowHeight = height;

    glfwGetFramebufferSize(window, &retina_width, &retina_height);
    glViewport(0, 0, retina_width, retina_height);

    float aspectRatio = (float)retina_width / (float)retina_height;
    projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);

    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}


void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        showDepthMap = !showDepthMap;
        std::cout << "[INFO] showDepthMap = "
            << (showDepthMap ? "true" : "false") << std::endl;
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        pointLightEnabled = !pointLightEnabled;
        std::cout << "[INFO] pointLightEnabled = "
            << (pointLightEnabled ? "true" : "false") << std::endl;
    }

    if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        helicopterRotating = !helicopterRotating;
        std::cout << "[INFO] Helicopter rotating = "
            << (helicopterRotating ? "true" : "false") << std::endl;
    }

    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        fogEnabled = !fogEnabled;
        std::cout << "[INFO] Fog enabled = "
            << (fogEnabled ? "true" : "false") << std::endl;
    }

    if (key == GLFW_KEY_G && action == GLFW_PRESS)
    {
        windEnabled = !windEnabled;
        std::cout << "[INFO] Wind enabled = "
            << (windEnabled ? "true" : "false") << std::endl;
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS)
    {
        rainEnabled = !rainEnabled;
        std::cout << "[INFO] Rain enabled = "
            << (rainEnabled ? "true" : "false") << std::endl;

        if (rainEnabled) {
            gRain.init();
        } else {
            gRain.destroy();
        }
    }

    if (key == GLFW_KEY_N && action == GLFW_PRESS) {
        nightMode = !nightMode;
        std::cout << "[INFO] Night mode = " << (nightMode ? "ON" : "OFF") << std::endl;
        handleDayAndNightMode();
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        cameraAnimationActive = true;
        currentPathIndex = 0;
        segmentTimer = 0.0f;
        isTraveling = true;
        myCamera.setCameraPosition(cameraPathPoints[0]);
        std::cout << "[INFO] Starting camera animation.\n";
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS)
            pressedKeys[key] = true;
        else if (action == GLFW_RELEASE)
            pressedKeys[key] = false;
    }
}

void processMovement()
{
    float currentSpeed = cameraSpeed;

    if (!cameraAnimationActive) {
        if (pressedKeys[GLFW_KEY_SPACE]) {
            currentSpeed *= 4.0f;
        }

        if (pressedKeys[GLFW_KEY_W]) {
            myCamera.move(gps::MOVE_FORWARD, currentSpeed);
        }
        if (pressedKeys[GLFW_KEY_S]) {
            myCamera.move(gps::MOVE_BACKWARD, currentSpeed);
        }
        if (pressedKeys[GLFW_KEY_A]) {
            myCamera.move(gps::MOVE_LEFT, currentSpeed);
        }
        if (pressedKeys[GLFW_KEY_D]) {
            myCamera.move(gps::MOVE_RIGHT, currentSpeed);
        }
    }

    if (pressedKeys[GLFW_KEY_J]) {
        lightAngle -= 1.0f;
    }
    if (pressedKeys[GLFW_KEY_L]) {
        lightAngle += 1.0f;
    }

    if (pressedKeys[GLFW_KEY_1]) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (pressedKeys[GLFW_KEY_2]) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    }

    if (pressedKeys[GLFW_KEY_3]) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

// ----------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------
bool initOpenGLWindow()
{
    if (!glfwInit()) {
        std::cerr << "[ERROR] Could not start GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

#if defined (__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    glWindow = glfwCreateWindow(glWindowWidth, glWindowHeight,
        "GP Project - Alex Mihoc",
        nullptr, nullptr);
    if (!glWindow) {
        std::cerr << "ERROR: Could not open window with GLFW3\n";
        glfwTerminate();
        return false;
    }

    glfwSetWindowSizeCallback(glWindow, windowResizeCallback);
    glfwSetKeyCallback(glWindow, keyboardCallback);
    glfwSetCursorPosCallback(glWindow, mouseCallback);
    glfwSetInputMode(glWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(glWindow);
    glfwSwapInterval(1);

#if !defined (__APPLE__)
    glewExperimental = GL_TRUE;
    glewInit();
#endif

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* ver = glGetString(GL_VERSION);
    std::cout << "Renderer: " << renderer << "\n";
    std::cout << "OpenGL version supported: " << ver << "\n";

    glfwGetWindowSize(glWindow, &glWindowWidth, &glWindowHeight);
    glfwGetFramebufferSize(glWindow, &retina_width, &retina_height);
    glViewport(0, 0, retina_width, retina_height);

    return true;
}

void initOpenGLState() {
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glViewport(0, 0, retina_width, retina_height);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);

    glEnable(GL_FRAMEBUFFER_SRGB);
}

void initSkybox() {
    faces.push_back("skybox/desertsky_rt.tga");
    faces.push_back("skybox/desertsky_lf.tga");
    faces.push_back("skybox/desertsky_up.tga");
    faces.push_back("skybox/desertsky_dn.tga");
    faces.push_back("skybox/desertsky_bk.tga");
    faces.push_back("skybox/desertsky_ft.tga");

    mySkyBox.Load(faces);
}

void initObjects() {
    scene.LoadModel("objects/scene/scene.obj");
    lightCube.LoadModel("objects/cube/cube.obj");
    screenQuad.LoadModel("objects/quad/quad.obj");
    heli.LoadModel("objects/heli/helicopter.obj");
}

void initShaders() {
    myCustomShader.loadShader("shaders/shaderStart.vert", "shaders/shaderStart.frag");
    myCustomShader.useShaderProgram();

    lightShader.loadShader("shaders/lightCube.vert", "shaders/lightCube.frag");
    lightShader.useShaderProgram();

    screenQuadShader.loadShader("shaders/screenQuad.vert", "shaders/screenQuad.frag");
    screenQuadShader.useShaderProgram();

    depthMapShader.loadShader("shaders/shadow.vert", "shaders/shadow.frag");
    depthMapShader.useShaderProgram();

    skyboxShader.loadShader("shaders/skyboxShader.vert", "shaders/skyboxShader.frag");
    skyboxShader.useShaderProgram();

    rainShader.loadShader("shaders/rainShader.vert", "shaders/rainShader.frag");
    rainShader.useShaderProgram();

    gRain = RainSystem(50000, rainShader);
}

void initUniforms() {
    myCustomShader.useShaderProgram();

    model = glm::mat4(1.0f);
    modelLoc = glGetUniformLocation(myCustomShader.shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    view = myCamera.getViewMatrix();
    viewLoc = glGetUniformLocation(myCustomShader.shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    normalMatrixLoc = glGetUniformLocation(myCustomShader.shaderProgram, "normalMatrix");
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    float aspectRatio = (float)glWindowWidth / (float)glWindowHeight;
    projection = glm::perspective(glm::radians(45.0f),
        aspectRatio,
        0.1f, 1000.0f);
    projectionLoc = glGetUniformLocation(myCustomShader.shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    lightDirLoc = glGetUniformLocation(myCustomShader.shaderProgram, "lightDir");

    lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    lightColorLoc = glGetUniformLocation(myCustomShader.shaderProgram, "lightColor");
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    lightShader.useShaderProgram();
    glUniformMatrix4fv(glGetUniformLocation(lightShader.shaderProgram, "projection"),
        1, GL_FALSE, glm::value_ptr(projection));

    depthMapShader.useShaderProgram();
    glUniform1i(glGetUniformLocation(depthMapShader.shaderProgram, "shadowMap"), 0);
}

void initFBO() {
    glGenFramebuffers(1, &shadowMapFBO);

    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_WIDTH, SHADOW_HEIGHT,
        0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f,1.0f,1.0f,1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D, depthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Shadow map framebuffer is not complete!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ----------------------------------------------------------------------
// Light Space Matrix
// ----------------------------------------------------------------------
glm::mat4 computeLightSpaceTrMatrix() {
    glm::mat4 rotateSun = glm::rotate(glm::mat4(1.0f),
        glm::radians(lightAngle),
        glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 currentSunPos = glm::vec3(rotateSun * glm::vec4(baseSunPos, 1.0f));

    glm::mat4 lightView = glm::lookAt(
        currentSunPos,
        sceneCenter,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float orthoSize = 50.0f;
    float nearPlane = -100.0f;
    float farPlane = 300.0f;

    glm::mat4 lightProjection = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        nearPlane, farPlane
    );

    lightSpaceTrMatrix = lightProjection * lightView;
    return lightSpaceTrMatrix;
}

// ----------------------------------------------------------------------
// Camera Animation
// ----------------------------------------------------------------------
void updateCameraAnimation(float deltaTime) {
    if (!cameraAnimationActive) {
        return;
    }

    if (currentPathIndex >= (int)cameraPathPoints.size() - 1) {
        cameraAnimationActive = false;
        return;
    }

    segmentTimer += deltaTime;

    if (isTraveling) {
        float t = segmentTimer / travelTime;
        if (t >= 1.0f) {
            t = 1.0f;
            myCamera.setCameraPosition(cameraPathPoints[currentPathIndex + 1]);

            isTraveling = false;
            segmentTimer = 0.0f;
        }
        else {
            glm::vec3 startPos = cameraPathPoints[currentPathIndex];
            glm::vec3 endPos = cameraPathPoints[currentPathIndex + 1];
            glm::vec3 newPos = glm::mix(startPos, endPos, t);
            myCamera.setCameraPosition(newPos);
        }
    }
    else {
        if (segmentTimer >= pauseTime) {
            segmentTimer = 0.0f;
            isTraveling = true;
            currentPathIndex++;

            if (currentPathIndex >= (int)cameraPathPoints.size() - 1) {
                cameraAnimationActive = false;
            }
        }
    }
}

// ----------------------------------------------------------------------
// Helicopter rotation logic
// ----------------------------------------------------------------------
void drawHelicopter(gps::Shader& shader) {
    shader.useShaderProgram();

    glm::mat4 modelHeli = glm::mat4(1.0f);

    modelHeli = glm::translate(modelHeli, glm::vec3(-102.0f, 65.6f, 7.8f));

    if (helicopterRotating) {
        heliAngle += 0.3f * deltaTimeHeli;
    }
    modelHeli = glm::rotate(modelHeli, glm::radians(heliAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    modelHeli = glm::scale(modelHeli, glm::vec3(5.0f, 5.0f, 5.0f));

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelHeli));

    glm::mat3 heliNormalMatrix = glm::mat3(glm::inverseTranspose(view * modelHeli));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(heliNormalMatrix));

    heli.Draw(shader);
}

void updateDeltaTimeHeli(double elapsedSeconds) {
    deltaTimeHeli = static_cast<float>(deltaTimeHeli + elapsedSeconds);
}

// ----------------------------------------------------------------------
// Thunder effect
// ----------------------------------------------------------------------
bool isPlaying = false;

void playThunderSound() {
    if (!isPlaying) {
        isPlaying = true;
        if (PlaySound(TEXT("C:\\Users\\Alex\\source\\repos\\Project\\Project\\thunder.wav"),
            NULL,
            SND_FILENAME | SND_ASYNC)) {
            std::cout << "[INFO] thunder.wav playing asynchronously.\n";
        }
        else {
            std::cerr << "[ERROR] Could not play thunder.wav.\n";
        }
        isPlaying = false;
    }
}

void thunderPollingAction(float deltaTime) {
    static float thunderBrightness = 1.0f;

    thunderTimer += deltaTime;

    if (!isThunderActive && thunderTimer >= nextThunderInterval && rainEnabled) {
        isThunderActive = true;
        thunderStartTime = thunderTimer;

        nextThunderInterval = thunderTimer + 7.0f + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (12.0f - 7.0f)));
    }

    if (isThunderActive && rainEnabled) {
        if (thunderTimer - thunderStartTime <= thunderDuration) {
             thunderBrightness = 10.0f; 
             playThunderSound();
        }
        else {
            isThunderActive = false;
            thunderBrightness = 1.0f;
        }
    }

    myCustomShader.useShaderProgram();

    GLint thunderLoc = glGetUniformLocation(myCustomShader.shaderProgram, "thunderBrightness");
    glUniform1f(thunderLoc, thunderBrightness);
}

// ----------------------------------------------------------------------
// Scene drawing logic here
// ----------------------------------------------------------------------
void drawScene(gps::Shader shader, bool depthPass)
{
    shader.useShaderProgram();

    glm::mat4 rotScene = glm::mat4(1.0f);

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"),
        1, GL_FALSE, glm::value_ptr(rotScene));

    if (!depthPass) {
        glm::mat3 normMat = glm::mat3(glm::inverseTranspose(view * rotScene));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normMat));
    }

    scene.Draw(shader);
}

// ----------------------------------------------------------------------
// Main render function run at each frame
// ----------------------------------------------------------------------
void renderScene(float deltaTime) {
    // 1) Day / Night mode set light brightness
    myCustomShader.useShaderProgram();
    float brightness = nightMode ? 0.5f : 1.0f;
    myCustomShader.useShaderProgram();
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "lightBrightness"), brightness);

    // 2) SHADOW PASS: Render scene from the sun's POV
    depthMapShader.useShaderProgram();

    GLint timeShadowLoc = glGetUniformLocation(depthMapShader.shaderProgram, "time");
    float currentTime = (float)glfwGetTime(); 
    glUniform1f(timeShadowLoc, currentTime);

    GLint windShadowLoc = glGetUniformLocation(depthMapShader.shaderProgram, "enableWind");
    glUniform1i(windShadowLoc, (windEnabled ? 1 : 0));

    glm::mat4 lightSpace = computeLightSpaceTrMatrix();
    glUniformMatrix4fv(glGetUniformLocation(depthMapShader.shaderProgram,
        "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(lightSpace));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    drawScene(depthMapShader, true);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 3) Set up point lights 
    myCustomShader.useShaderProgram();

    GLint numPLoc = glGetUniformLocation(myCustomShader.shaderProgram, "numPointLights");
    glUniform1i(numPLoc, NUM_OF_POINT_LIGHTS);

    glm::vec3 plPosEye[NUM_OF_POINT_LIGHTS];
    for (int i = 0; i < NUM_OF_POINT_LIGHTS; i++) {
        glm::vec4 eyeSpacePos = view * glm::vec4(gPointLightPositions[i], 1.0f);
        plPosEye[i] = glm::vec3(eyeSpacePos);
    }

    GLint plArrayLoc = glGetUniformLocation(myCustomShader.shaderProgram, "pointLightPositions");
    glUniform3fv(plArrayLoc, NUM_OF_POINT_LIGHTS, glm::value_ptr(plPosEye[0]));

    glUniform3fv(glGetUniformLocation(myCustomShader.shaderProgram, "pointLightColor"),
        1, glm::value_ptr(gPointLightColor));

    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "pointLightAmbient"),
        gPointLightAmbient);
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "pointLightDiffuse"),
        gPointLightDiffuse);
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "pointLightSpecular"),
        gPointLightSpecular);
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "constantAtt"),
        gConstantAtt);
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "linearAtt"),
        gLinearAtt);
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "quadraticAtt"),
        gQuadraticAtt);

    glUniform1i(glGetUniformLocation(myCustomShader.shaderProgram, "enablePointLight"),
        (pointLightEnabled ? 1 : 0));

    // 4) If user wants to see the Depth Map, show it and return
    if (showDepthMap) {
        glViewport(0, 0, retina_width, retina_height);
        glClear(GL_COLOR_BUFFER_BIT);
        screenQuadShader.useShaderProgram();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMapTexture);
        glUniform1i(glGetUniformLocation(screenQuadShader.shaderProgram, "depthMap"), 0);

        glDisable(GL_DEPTH_TEST);
        screenQuad.Draw(screenQuadShader);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    // 5) NORMAL PASS: Render the scene from the camera
    glViewport(0, 0, retina_width, retina_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    myCustomShader.useShaderProgram();

    GLint timeMainLoc = glGetUniformLocation(myCustomShader.shaderProgram, "time");
    glUniform1f(timeMainLoc, currentTime);

    GLint windMainLoc = glGetUniformLocation(myCustomShader.shaderProgram, "enableWind");
    glUniform1i(windMainLoc, (windEnabled ? 1 : 0));

    GLint rainEnabled = glGetUniformLocation(myCustomShader.shaderProgram, "rainEnabled");
    glUniform1i(rainEnabled, (rainEnabled ? 1 : 0));

    view = myCamera.getViewMatrix();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    glUniformMatrix4fv(glGetUniformLocation(myCustomShader.shaderProgram,
        "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(lightSpace));

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glUniform1i(glGetUniformLocation(myCustomShader.shaderProgram, "shadowMap"), 3);

    glm::mat4 rotateSun = glm::rotate(glm::mat4(1.0f),
        glm::radians(lightAngle),
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 currentSunPos = glm::vec3(rotateSun * glm::vec4(baseSunPos, 1.0f));

    glm::vec3 newLightDir = glm::normalize(sceneCenter - currentSunPos);
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(newLightDir));

    // 5) Pass the fog uniforms
    glUniform1i(glGetUniformLocation(myCustomShader.shaderProgram, "enableFog"),
        (fogEnabled ? 1 : 0));
    glUniform1f(glGetUniformLocation(myCustomShader.shaderProgram, "fogDensity"),
        gFogDensity);
    glUniform4fv(glGetUniformLocation(myCustomShader.shaderProgram, "fogColor"),
        1, glm::value_ptr(gFogColor));

    // 6) Draw the scene & helicopter obj
    drawScene(myCustomShader, false);
    drawHelicopter(myCustomShader);

    // 7) Draw the rain
    if (rainEnabled) {
        thunderPollingAction(deltaTime);

        gRain.update(deltaTime);

        gRain.uploadToGPU();

        lightShader.useShaderProgram();

        glm::mat4 view = myCamera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(lightShader.shaderProgram, "view"),
            1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(lightShader.shaderProgram, "projection"),
            1, GL_FALSE, glm::value_ptr(projection));

        GLint colorLoc = glGetUniformLocation(lightShader.shaderProgram, "objectColor");
        if (colorLoc >= 0) {
            glUniform3f(colorLoc, 0.65f, 0.65f, 1.0f);  
        }

        gRain.draw(projection, view);
    }

    // 8) Draw the "sun" cube
    lightShader.useShaderProgram();
    glUniformMatrix4fv(glGetUniformLocation(lightShader.shaderProgram, "view"),
        1, GL_FALSE, glm::value_ptr(view));

    model = glm::mat4(1.0f);
    model = glm::translate(model, currentSunPos);
    model = glm::scale(model, glm::vec3(0.25f, 0.25f, 0.25f));
    glUniformMatrix4fv(glGetUniformLocation(lightShader.shaderProgram, "model"),
        1, GL_FALSE, glm::value_ptr(model));

    lightCube.Draw(lightShader);

    // 9) Draw skybox
    mySkyBox.Draw(skyboxShader, view, projection);
}

void cleanup() {
    glDeleteTextures(1, &depthMapTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &shadowMapFBO);

    glfwDestroyWindow(glWindow);
    glfwTerminate();
}

void debugCameraPosition() {
    glm::vec3 camPos = myCamera.getCameraPosition();
    std::cout << "Camera position: ("
        << camPos.x << ", "
        << camPos.y << ", "
        << camPos.z << ")" << std::endl;
}

int main(int argc, const char* argv[])
{
    if (!initOpenGLWindow()) {
        glfwTerminate();
        return 1;
    }

    initOpenGLState();
    initObjects();
    initSkybox();
    initShaders();
    initUniforms();
    initFBO();

    glCheckError();

    lastTimeStamp = glfwGetTime();

    while (!glfwWindowShouldClose(glWindow)) {
        double currentTimeStamp = glfwGetTime();
        float deltaTime = static_cast<float>(currentTimeStamp - lastTimeStamp);
        updateDeltaTimeHeli(currentTimeStamp - lastTimeStamp);
        lastTimeStamp = currentTimeStamp;

        processMovement();
        updateCameraAnimation(deltaTime);
        renderScene(deltaTime);
        //debugCameraPosition();
        
        glfwPollEvents();
        glfwSwapBuffers(glWindow);
    }

    cleanup();
    return 0;
}
