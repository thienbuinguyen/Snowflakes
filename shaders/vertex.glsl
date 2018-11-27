#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 texCoord;

out vec2 fragTexCoord;

uniform mat4 mvp;

void main() {
    fragTexCoord = texCoord;
    gl_Position = mvp * vec4(pos, 1.0f);
}
