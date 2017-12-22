#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec4 pos;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec2 outTexCoord;

void main() {
  gl_Position = pos;
  outTexCoord = texCoord;
}
