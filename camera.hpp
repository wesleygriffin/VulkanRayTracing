#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/vec3.hpp"

class Camera {
public:
  Camera(float vfovDeg, float aspectRatio, glm::vec3 lookFrom, glm::vec3 lookAt,
         glm::vec3 viewUp) noexcept
    : vfov_{vfovDeg * glm::pi<float>() / 180.f}
    , aspectRatio_{aspectRatio}
    , eye_(std::move(lookFrom))
    , lookAt_(std::move(lookAt))
    , viewUp_(std::move(viewUp)) {
    calculateFrame();
  }

  glm::vec3 eye() const noexcept { return eye_; }
  glm::vec3 u() const noexcept { return u_; }
  glm::vec3 v() const noexcept { return v_; }
  glm::vec3 w() const noexcept { return w_; }

  glm::vec3 lookAt() const noexcept { return lookAt_; }

  void aspectRatio(float aspectRatio) noexcept {
    aspectRatio_ = aspectRatio;
    calculateFrame();
  }

  Camera() = default;

private:
  float vfov_{};
  float aspectRatio_{};
  glm::vec3 eye_{}, lookAt_{}, viewUp_{};
  glm::vec3 u_{}, v_{}, w_{};

  void calculateFrame() noexcept {
    w_ = lookAt_ - eye_;
    u_ = glm::normalize(glm::cross(w_, viewUp_));
    v_ = glm::cross(u_, w_);

    float const len = glm::length(w_) * std::tan(.5f * vfov_);
    v_ *= len;
    u_ *= len * aspectRatio_;
  }
}; // class camera

#endif // CAMERA_HPP_
