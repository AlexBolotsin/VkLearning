#version 450 // gsls 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

layout(binding = 0) uniform MVP
{
	mat4 proj;
	mat4 view;
	mat4 model;
} mvp;

layout(location = 0) out vec3 flagCol;

void main() {
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(pos, 1.0);
	flagCol = col;
}