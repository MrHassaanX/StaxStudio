#version 440

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 vertexTexCoord;
layout(binding = 0) uniform sampler2D sourceTexture;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(sourceTexture, vertexTexCoord) * vertexColor;
}
