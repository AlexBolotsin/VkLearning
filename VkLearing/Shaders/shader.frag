#version 450

layout(location = 0) in vec3 fragCol;
layout(location = 0) out vec4 color;

void main() {
	color = vec4(fragCol, 1.0);
}