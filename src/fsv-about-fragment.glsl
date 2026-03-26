// SPDX-License-Identifier: MIT

#version 140

in vec3 vertColor;
in vec3 fragPos;
in vec3 fragNormal;
in vec4 lightPos;

out vec4 outputColor;

uniform float ambient;
uniform float diffuse;
uniform float specular;

uniform vec3 fog_color;
uniform float fog_start;
uniform float fog_end;

void main() {
  vec3 light_color = vec3(1.0, 1.0, 1.0);
  // Ambient light
  vec3 ambient_light = ambient * light_color;

  // Diffuse light
  vec3 lightDir;
  if (lightPos.w == 0.0)  // Light at infinity
    lightDir = normalize(lightPos.xyz);
  else
    lightDir = normalize(lightPos.xyz - fragPos);
  vec3 fragNN = normalize(fragNormal);
  float diffuse_refl = max(dot(fragNN, lightDir), 0.0);
  vec3 diffuse_light = diffuse_refl * diffuse * light_color;

  // Specular light
  vec3 viewPos = vec3(0.0, 0.0, 0.0);
  vec3 viewDir = normalize(viewPos - fragPos);
  vec3 reflectDir = reflect(-lightDir, fragNN);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 2);
  vec3 spec_light = specular * spec * light_color;

  // Final color from lightning calculation
  vec3 color = (ambient_light + diffuse_light + spec_light) * vertColor;

  // Fog
  float zdist = abs(fragPos.z);
  float fog = (fog_end - zdist) / (fog_end - fog_start);

  outputColor = vec4(mix(fog_color, color, fog), 1.0);

  // For debugging, uncomment this. Also set fragNormal to flat in both vertex
  // and fragment shader out/in
  //outputColor = 0.9999 * vec4(abs(fragNN), 1.0) + 0.0001 * outputColor;
}
