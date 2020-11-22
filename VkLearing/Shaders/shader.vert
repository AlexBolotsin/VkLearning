#version 450 // gsls 4.5

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;

layout(binding = 0) uniform UboViewProjection {
	mat4 proj;
	mat4 view;
} uboViewProjection;

layout(binding = 1) uniform UboModel {
	mat4 model;
} uboModel;

layout(location = 0) out vec3 flagCol;

void main() {
	gl_Position = uboViewProjection.proj * uboViewProjection.view * uboModel.model * vec4(pos, 1.0);
	flagCol = col;
}