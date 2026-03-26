// SPDX-License-Identifier: MIT

#version 140

in vec2 Texcoord;

uniform vec3 color;
uniform sampler2D tex;

out vec4 outputColor;

void main() {
  vec4 alpha = texture(tex, Texcoord);
  outputColor = vec4(color, alpha.r);
}
