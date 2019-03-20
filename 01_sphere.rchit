#version 460 core
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform accelerationStructureNV scene;
layout(set = 0, binding = 1, rgba8) uniform image2D image;

layout(set = 0, binding = 2) uniform Camera {
  vec4 Eye;
  vec4 U;
  vec4 V;
  vec4 W;
} camera;

struct Sphere {
  float aabbMinX;
  float aabbMinY;
  float aabbMinZ;
  float aabbMaxX;
  float aabbMaxY;
  float aabbMaxZ;
};

layout(std430, binding = 3) readonly buffer SphereBuffer {
  Sphere spheres[];
};

layout(location = 0) rayPayloadInNV vec3 hitValue;
hitAttributeNV vec3 normalVector;

void main() {
  const vec3 P = hitValue;
  const vec3 N = normalize(normalVector.xyz);
  hitValue = vec3(.5f) * (N + vec3(1.f));
}
