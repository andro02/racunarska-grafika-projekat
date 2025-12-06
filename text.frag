#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D text;
uniform vec3 textColor;
uniform float textAlpha;

void main()
{
    float alpha = texture(text, TexCoords).a * textAlpha;
    FragColor = vec4(textColor, alpha);
}