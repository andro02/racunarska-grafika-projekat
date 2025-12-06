#version 330 core

layout(location = 0) in vec2 aPos;
uniform float uScaleX;
uniform float uScaleY;
uniform vec2 uOffset;

void main() {
    gl_Position = vec4(aPos.x * uScaleX + uOffset.x, aPos.y * uScaleY + uOffset.y, 0.0, 1.0);
}
