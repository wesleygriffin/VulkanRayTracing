#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "glm/common.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/mat4x4.hpp"
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
    update();
  }

  void aspectRatio(float aspectRatio) noexcept {
    aspectRatio_ = aspectRatio;
    update();
  }

  void translate(float scale) noexcept {
    eye_ = eye_ + (lookAt_ - eye_) * scale;
    update();
  }

  void rotate(glm::mat4 rotation) noexcept {
    rotation_ = std::move(rotation);
    update();
  }

  glm::vec3 eye() const noexcept { return eye_; }
  glm::vec3 lookAt() const noexcept { return lookAt_; }

  glm::vec3 u() const noexcept { return u_; }
  glm::vec3 v() const noexcept { return v_; }
  glm::vec3 w() const noexcept { return w_; }

  Camera() = default;

private:
  glm::mat4 rotation_{1.f};
  float vfov_{};
  float aspectRatio_{};
  glm::vec3 eye_{}, lookAt_{}, viewUp_{};
  glm::vec3 u_{}, v_{}, w_{};

  void update() noexcept {
    calculateFrame();

    glm::vec3 const nu = glm::normalize(u_);
    glm::vec3 const nv = glm::normalize(v_);
    glm::vec3 const nw = glm::normalize(-w_);

    // clang-format off
    glm::mat4 const frame = glm::mat4(
        nu.x, nv.x, nw.x, lookAt_.x,
        nu.y, nv.y, nw.y, lookAt_.y,
        nu.z, nv.z, nw.z, lookAt_.z,
         0.f,  0.f,  0.f,       1.f
    );
    // clang-format on

    glm::mat4 const frameInv = glm::inverse(frame);
    glm::mat4 const trans = frame * rotation_ * rotation_ * frameInv;

    eye_ = glm::vec3(trans * glm::vec4(eye_, 1.f));
    lookAt_ = glm::vec3(trans * glm::vec4(lookAt_, 1.f));
    viewUp_ = glm::vec3(trans * glm::vec4(viewUp_, 0.f));

    calculateFrame();
    rotation_ = glm::mat4(1.f);
  } // void update

  void calculateFrame() noexcept {
    w_ = lookAt_ - eye_;
    u_ = glm::normalize(glm::cross(w_, viewUp_));
    v_ = glm::normalize(glm::cross(u_, w_));

    float const len = glm::length(w_) * std::tan(.5f * vfov_);
    v_ *= len;
    u_ *= len * aspectRatio_;
  } // void calculateFrame
}; // class camera

#endif // CAMERA_HPP_
