#version 410 core

in vec3 fPosition;           
in vec3 fNormal;             
in vec2 fTexCoords;          
in vec4 fPosEye;             
in vec4 fragPosLightSpace;   

out vec4 fColor;

// ----------[ Directional Light Uniforms ]----------
uniform vec3 lightDir;
uniform vec3 lightColor;

// ----------[ Multiple Point Lights Uniforms ]----------
#define MAX_POINT_LIGHTS  10
uniform int  numPointLights;

uniform vec3 pointLightColor;
uniform float pointLightAmbient;
uniform float pointLightDiffuse;
uniform float pointLightSpecular;
uniform float constantAtt;
uniform float linearAtt;
uniform float quadraticAtt;

uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform int  enablePointLight;
uniform float lightBrightness;

// ----------[ Shadow Sampler & Constants ]----------
uniform sampler2D shadowMap;
float shadowIntensity = 0.5; 

// ----------[ Diffuse & Specular Textures ]----------
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;

// ----------[ Fog Uniforms ]----------
uniform int   enableFog;      
uniform float fogDensity;    
uniform vec4  fogColor;      

// ----------[ Thunder Uniforms ]----------
uniform int   rainEnabled;         
uniform float thunderBrightness; 


// ----------[ Common Phong Lighting Params ]----------
float bias      = 0.005;
float shininess = 32.0f;


// ---------------------------------------------------------
//    1) Directional Light Computation (the "sun")
// ---------------------------------------------------------
vec3 CalcDirectionalLight(in vec3 normalEye, in vec3 viewDirEye) {
    // basic ambient
    float ambientStrength = 0.01f;
    vec3  ambient = ambientStrength * lightColor;

    // diffuse
    float diff = max(dot(normalEye, normalize(lightDir)), 0.0f);
    vec3 diffuse = diff * lightColor;

    // specular
    vec3 reflection = reflect(-normalize(lightDir), normalEye);
    float specCoeff = pow(max(dot(viewDirEye, reflection), 0.0f), shininess);
    float specularStrength = 1.0f;
    vec3 specular = specularStrength * specCoeff * lightColor;

    return (ambient + diffuse + specular);
}

// ---------------------------------------------------------
//    2) Point Light Computation
// ---------------------------------------------------------
vec3 CalcPointLight(
    in vec3 normalEye,
    in vec3 fragPosEye,
    in vec3 viewDirEye,
    in vec3 lightPosEye) {

    vec3 lightVec = lightPosEye - fragPosEye;
    float distance = length(lightVec);
    vec3  L = normalize(lightVec);

    vec3 ambient = pointLightAmbient * pointLightColor;

    float diff = max(dot(normalEye, L), 0.0);
    vec3 diffuse = pointLightDiffuse * diff * pointLightColor;

    vec3 reflection = reflect(-L, normalEye);
    float specCoeff = pow(max(dot(viewDirEye, reflection), 0.0), shininess);
    vec3 specular  = pointLightSpecular * specCoeff * pointLightColor;

    float att = 1.0 / (constantAtt + linearAtt * distance
                                 + quadraticAtt * distance * distance);

    return (ambient + diffuse + specular) * att;
}

// ---------------------------------------------------------
//    3) Shadow Computation
// ---------------------------------------------------------
float computeShadow() {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float shadow = currentDepth - bias > closestDepth ? shadowIntensity : 0.0;
    return shadow;
}


// ---------------------------------------------------------
//    4) Fog Computation
// ---------------------------------------------------------
float computeFogFactor() {
    float dist = length(fPosEye.xyz);
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    return clamp(fogFactor, 0.0, 1.0);
}

void main() {
    vec3 normalEye  = normalize(fNormal);
    vec3 viewDirEye = normalize(-fPosEye.xyz);

    vec3 baseColor = texture(diffuseTexture, fTexCoords).rgb;
    vec3 specMap   = texture(specularTexture, fTexCoords).rgb;

    float shadow = computeShadow();
    vec3 dirLight = CalcDirectionalLight(normalEye, viewDirEye);

    vec3 directLightContrib = (0.3 * dirLight) + (1.0 - shadow) * (dirLight - 0.3 * dirLight);
    directLightContrib *= lightBrightness;

    vec3 directionalResult = directLightContrib * baseColor
                           + (1.0 - shadow) * specMap;

    vec3 pointLightsAccum = vec3(0.0);
    for (int i = 0; i < numPointLights; i++)
    {
        vec3 lightContribution = CalcPointLight(normalEye, fPosEye.xyz, viewDirEye, pointLightPositions[i]);
        pointLightsAccum += lightContribution;
    }
    pointLightsAccum = pointLightsAccum * baseColor + pointLightsAccum * specMap;

    vec3 finalColor = directionalResult + float(enablePointLight) * pointLightsAccum;

    if (enableFog == 1) {
        float fogFactor = computeFogFactor();
        vec3 foggedColor = mix(fogColor.rgb, finalColor, fogFactor);
        finalColor = foggedColor;
    }

    if (rainEnabled == 1) {
        finalColor *= thunderBrightness;
    }

    fColor = vec4(finalColor, 1.0);
}
