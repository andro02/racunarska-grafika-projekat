#version 330 core

layout(location = 0) in vec2 aPos;
uniform float uScaleX;
uniform vec2 uOffset;
uniform float uSize; 

void main() {
    gl_Position = vec4(aPos.x * uScaleX + uOffset.x, aPos.y * uSize + uOffset.y, 0.0, 1.0);
}
