#version 410 core

layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoords;

out vec3 fNormal;
out vec4 fPosEye;
out vec2 fTexCoords;
out vec4 fragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform mat4 lightSpaceTrMatrix;

uniform float time;

uniform int enableWind;

void main()
{
    //---------------------------------------------
    // 1) Wind displacement
    //---------------------------------------------
    float baseStrength  = 0.1;
    float extraStrength = 0.05 * sin(time * 0.2);
    float windStrength  = (enableWind == 1) 
                            ? (baseStrength + extraStrength)
                            : 0.0;   

    float frequency     = 0.5;
    float wave = 0.0;

    if (enableWind == 1)
    {
        wave = sin(vPosition.x * frequency + time)
             * cos(vPosition.z * frequency + time);
    }

    vec3 displacedPosition = vPosition + vNormal * wave * windStrength;

    //---------------------------------------------
    // 2) Usual transformations
    //---------------------------------------------
    fPosEye  = view * model * vec4(displacedPosition, 1.0f);
    fNormal  = normalize(normalMatrix * vNormal);
    fTexCoords = vTexCoords;
    fragPosLightSpace = lightSpaceTrMatrix * model * vec4(displacedPosition, 1.0f);

    gl_Position = projection * fPosEye;
}
