// SPDX-License-Identifier: MIT

#version 140

in vec3 position;
in vec3 normal;

out vec3 fragPos;
out vec3 fragNormal;
out vec4 lightPos;

uniform mat4 mvp;
uniform mat4 modelview;
uniform mat3 normal_matrix;
uniform vec4 light_pos;
uniform bool lightning_enabled;


void main() {
  vec4 pos = vec4(position, 1.0);
  gl_Position = mvp * pos;

  if (lightning_enabled) {
    lightPos = modelview * light_pos;

    // Position of vertex in camera coordinates interpolated to frag coords
    vec4 fragTmp = modelview * pos;
    fragPos = fragTmp.xyz / fragTmp.w;

    fragNormal = normal_matrix * normal;
  }
}
