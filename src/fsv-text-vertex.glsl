// SPDX-License-Identifier: MIT

#version 140

in vec3 position;
in vec2 texcoord;
out vec2 Texcoord;

uniform mat4 mvp;

void main() {
  gl_Position = mvp * vec4(position, 1.0);
  Texcoord = texcoord;
}
