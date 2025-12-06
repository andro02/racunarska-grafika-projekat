#version 330 core
layout(location = 0) in vec2 aPos;

uniform vec2 uOffset;
uniform float uScale;
uniform float uAngle;
uniform float uAspect;

void main() {
    // Prvo primeni rotaciju
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec2 pos = vec2(
        aPos.x * c - aPos.y * s,
        aPos.x * s + aPos.y * c
    );

    // Primeni aspect ratio tako da vizuelno ostane proporcionalno
    pos.x /= uAspect;

    // Skaliranje i pomeranje
    pos = pos * uScale + uOffset;
    gl_Position = vec4(pos, 0.0, 1.0);
}
