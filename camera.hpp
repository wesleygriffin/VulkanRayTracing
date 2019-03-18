#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/vec3.hpp"

class Camera {
public:
  glm::vec3 origin() const noexcept { return origin_; }
  glm::vec3 horizontal() const noexcept { return horizontal_; }
  glm::vec3 vertical() const noexcept { return vertical_; }
  glm::vec3 lowerLeft() const noexcept { return lowerLeft_; }

  Camera(float vfov_deg, float aspect, glm::vec3 look_from = {0.f, 0.f, 0.f},
         glm::vec3 look_at = {0.f, 0.f, -2.f},
         glm::vec3 view_up = {0.f, 1.f, 0.f})
    : origin_(look_from) {

    w_ = glm::normalize(look_from - look_at);
    u_ = glm::normalize(glm::cross(view_up, w_));
    v_ = glm::cross(w_, u_);

    float const vfov_rad = vfov_deg * glm::pi<float>() / 180.f;
    float const half_height = std::tan(vfov_rad / 2.f);
    float const half_width = aspect * half_height;

    horizontal_ = 2.f * half_width * u_;
    vertical_ = 2.f * half_height * v_;
    lowerLeft_ = origin_ - .5f * horizontal_ - .5f * vertical_ - w_;
  }

  Camera(glm::vec3 origin, glm::vec3 horizontal, glm::vec3 vertical,
         glm::vec3 lowerLeft)
    : origin_(std::move(origin))
    , u_(0.f)
    , v_(0.f)
    , w_(0.f)
    , horizontal_(std::move(horizontal))
    , vertical_(std::move(vertical))
    , lowerLeft_(std::move(lowerLeft)) {}

private:
  glm::vec3 origin_;
  glm::vec3 u_, v_, w_;
  glm::vec3 horizontal_;
  glm::vec3 vertical_;
  glm::vec3 lowerLeft_;
}; // class camera

#endif // CAMERA_HPP_
