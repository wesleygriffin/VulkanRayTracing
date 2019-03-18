#version 460 core
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 hitValue;

void main() {
  const vec3 direction = normalize(gl_WorldRayDirectionNV);
  const float t = .5f * (direction.y + 1.f);
  hitValue = mix(vec3(1.f, 1.f, 1.f), vec3(.5f, .7f, 1.f), t);
}