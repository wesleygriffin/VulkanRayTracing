#version 460 core
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 hitValue;

void main() {
  hitValue = vec3(0.0, 0.1, 0.3);
}