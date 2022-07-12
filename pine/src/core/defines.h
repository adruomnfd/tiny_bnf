#ifndef PINE_H
#define PINE_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define PINE_DELETE_COPY_MOVE(ClassName)             \
    ClassName(const ClassName&) = delete;            \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName(ClassName&&) = delete;                 \
    ClassName& operator=(ClassName&&) = delete;

#define PINE_DELETE_COPY(ClassName)       \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete;

#define SWITCH(x)                    \
    const auto& _switchVariable = x; \
    if (false)

#define CASE(x) \
    }           \
    else if (_switchVariable == x) {
#define DEFAULT \
    }           \
    else {
#define CASE_F(f) \
    }             \
    else if (f(_switchVariable)) {
#define DEFAULT \
    }           \
    else {
#define CASE_BEGINWITH(s) \
    }                     \
    else if (_switchVariable.rfind(s, 0) == 0) {
#define DEFAULT \
    }           \
    else {
#if defined(__GNUC__) || defined(__clang__)
#define PINE_LIKELY(x) __builtin_expect(x, true)
#define PINE_UNLIKELY(x) __builtin_expect(x, false)
#define PINE_RESTRICT __restrict
#define PINE_ALWAYS_INLINE __attribute__((always_inline))
#define UNREACHABLE __builtin_unreachable()
#else
#define PINE_LIKELY(x) x
#define PINE_UNLIKELY(x) x
#define PINE_RESTRICT
#define PINE_ALWAYS_INLINE
#define UNREACHABLE
#endif

namespace pine {

struct PhaseFunction;
struct TriangleMesh;
struct Interaction;
struct Parameters;
struct Material;
struct Camera;
struct Medium;
struct Scene;
struct Shape;
struct AABB;
struct Ray;
struct RNG;

class Integrator;
class Accel;

}  // namespace pine

#endif  // PINE_H