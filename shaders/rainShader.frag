#version 410 core

uniform vec3 rainColor;

out vec4 fragColor;

void main() {
    fragColor = vec4(rainColor, 1.0);
}
