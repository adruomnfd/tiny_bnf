#ifndef PINE_CORE_SAMPLING_H
#define PINE_CORE_SAMPLING_H

#include <core/vecmath.h>

namespace pine {

struct SpatialPdf {
    operator float() const {
        return pos * dir;
    }
    float pos = 0.0f;
    float dir = 0.0f;
};

inline vec2 SampleDiskPolar(vec2 u) {
    float r = pstd::sqrt(u[0]);
    float theta = 2 * Pi * u[1];
    return {r * pstd::cos(theta), r * pstd::sin(theta)};
}

inline vec2 SampleDiskConcentric(vec2 u) {
    u = vec2(u.x * 2 - 1.0f, u.y * 2 - 1.0f);
    float theta, r;
    if (pstd::abs(u.x) > pstd::abs(u.y)) {
        r = u.x;
        theta = Pi / 4.0f * u.y / u.x;
    } else {
        r = u.y;
        theta = Pi / 2.0f - Pi / 4.0f * (u.x / u.y);
    }
    return r * vec2(pstd::cos(theta), pstd::sin(theta));
}

inline vec3 CosineWeightedSampling(vec2 u) {
    vec2 d = SampleDiskConcentric(u);
    float z = pstd::sqrt(pstd::max(1.0f - d.x * d.x - d.y * d.y, 0.0f));
    return vec3(d.x, d.y, z);
}

inline vec3 UniformSphereSampling(vec2 u) {
    return SphericalToCartesian(u.x * Pi * 2, pstd::acos(1.0f - 2 * u.y));
}
inline vec2 InverseUniformSphereMampling(vec3 d) {
    auto [phi, theta] = CartesianToSpherical(d);
    return {phi / Pi2, (1.0f - pstd::cos(theta)) / 2.0f};
}

inline vec3 UniformHemisphereMampling(vec2 u) {
    return SphericalToCartesian(u.x * Pi * 2, pstd::acos(u.y));
}
inline vec2 InverseUniformHemisphereMampling(vec3 d) {
    auto [phi, theta] = CartesianToSpherical(d);
    return {phi / Pi2, pstd::cos(theta)};
}

inline float BalanceHeuristic(int nF, float pF, int nG, float pG) {
    return nF * pF / (nF * pF + nG * pG);
}
inline float PowerHeuristic(int nF, float pF, int nG, float pG) {
    float f = nF * pF;
    float g = nG * pG;
    return pstd::sqr(f) / (pstd::sqr(f) + pstd::sqr(g));
}

}  // namespace pine

#endif  // PINE_CORE_SAMPLING_H
