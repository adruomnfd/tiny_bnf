#include <core/medium.h>
#include <core/scene.h>
#include <core/sampling.h>
#include <util/parameters.h>
#include <util/fileio.h>

namespace pine {

static float PhaseHG(float cosTheta, float g) {
    float denom = 1.0f + g * g + 2 * g * cosTheta;
    return (1 - g * g) / (denom * pstd::sqrt(denom) * Pi * 4);
}

float PhaseFunction::P(vec3 wi, vec3 wo) const {
    return PhaseHG(AbsDot(wi, wo), g);
}
float PhaseFunction::Sample(vec3 wi, vec3& wo, vec2 u2) const {
    float cosTheta;
    if (pstd::abs(g) < 1e-3f) {
        cosTheta = 1 - 2 * u2[0];
    } else {
        float sqrTerm = (1.0f - g * g) / (1 + g - 2 * g * u2[0]);
        cosTheta = -(1.0f + g * g - sqrTerm * sqrTerm) / (2.0f * g);
    }

    float sinTheta = pstd::sqrt(1.0f - cosTheta * cosTheta);
    float phi = 2 * Pi * u2[1];
    mat3 m = CoordinateSystem(wi);
    wo = m * vec3(sinTheta * pstd::cos(phi), sinTheta * pstd::sin(phi), cosTheta);
    return PhaseHG(cosTheta, g);
}

Spectrum HomogeneousMedium::Tr(const Ray& ray, Sampler&) const {
    return Exp(-ray.tmax * sigma_t);
}
Spectrum HomogeneousMedium::Sample(const Ray& ray, Interaction& mi, Sampler& sampler) const {
    int i = sampler.Get1D() * Spectrum::nSamples;
    float dist = -pstd::log(1.0f - sampler.Get1D()) / sigma_t[i];
    bool sampledMedium = dist < ray.tmax;
    float t = pstd::min(dist, ray.tmax);
    Spectrum tr = Exp(-sigma_t * t);

    if (sampledMedium) {
        mi.p = ray(t);
        mi.mediumInterface = MediumInterface<const Medium*>((const Medium*)this);
        mi.phaseFunction = &phaseFunction;
        float pdf = (sigma_t * tr).Average();
        return sigma_s * tr / pdf;
    } else {
        float pdf = tr.Average();
        return tr / pdf;
    }
}

Spectrum GridMedium::Tr(const Ray& r, Sampler& sampler) const {
    Ray ray = TransformRayOD(w2m, r);
    float tmin, tmax;
    if (!AABB(vec3(0), vec3(1)).Hit(ray, tmin, tmax))
        return Spectrum(1.0f);

    if (method == SamplingMethod::DeltaTracking) {
        float tr = 1.0f;
        float t = tmin;
        while (true) {
            t -= pstd::log(1.0f - sampler.Get1D()) * invMaxDensity / sigma_t;
            if (t >= tmax)
                break;
            float density = Density(ray(t));
            tr *= 1.0f - pstd::max(0.0f, density * invMaxDensity);

            const float rrThreshold = 0.1f;
            if (tr < rrThreshold) {
                float q = pstd::max(0.05f, 1.0f - tr);
                if (sampler.Get1D() < q)
                    return 0.0f;
                else
                    tr /= 1.0f - q;
            }
        }
        return Spectrum(tr);

    } else if (method == SamplingMethod::RayMarching) {
        float tr = 0.0f;
        float stepSize = rayMarchingStepSize;
        float jitter = stepSize * (sampler.Get1D() - 0.5f);
        float t = tmin;
        while (t < tmax) {
            tr += Density(ray(t + jitter)) * sigma_t * stepSize;
            t += stepSize;
        }

        return pstd::exp(-tr);
    }

    return Spectrum(1.0f);
}

Spectrum GridMedium::Sample(const Ray& r, Interaction& mi, Sampler& sampler) const {
    Ray ray = TransformRayOD(w2m, r);
    float tmin, tmax;
    if (!AABB(vec3(0), vec3(1)).Hit(ray, tmin, tmax))
        return Spectrum(1.0f);

    if (method == SamplingMethod::DeltaTracking) {
        float t = tmin;
        while (true) {
            t -= pstd::log(1.0f - sampler.Get1D()) * invMaxDensity / sigma_t;
            if (t >= tmax)
                return Spectrum(1.0f);
            if (Density(ray(t)) * invMaxDensity > sampler.Get1D()) {
                mi.p = m2w * vec4(ray(t), 1.0f);
                mi.mediumInterface = MediumInterface<const Medium*>((const Medium*)this);
                mi.phaseFunction = &phaseFunction;
                return sigma_s / sigma_t;
            }
        }

    } else if (method == SamplingMethod::RayMarching) {
        float lnXi = -pstd::log(1.0f - sampler.Get1D());
        float stepSize = rayMarchingStepSize;
        float jitter = stepSize * (sampler.Get1D() - 0.5f);
        float t = tmin;
        float sum = 0.0f;
        while (t < tmax) {
            sum += Density(ray(t)) * sigma_t * stepSize;
            if (sum >= lnXi) {
                mi.p = m2w * vec4(ray(t + jitter), 1.0f);
                mi.mediumInterface = MediumInterface<const Medium*>((const Medium*)this);
                mi.phaseFunction = &phaseFunction;
                return sigma_s / sigma_t;
            }
            t += stepSize;
        }

        return Spectrum(1.0f);
    }

    return Spectrum(0.0f);
}

float GridMedium::Density(vec3 p) const {
    AABB bound(vec3(0), vec3(1));
    p = bound.Offset(p) * size;
    if (!interpolate)
        return D(p);

    vec3i pi = p;
    vec3i ps[] = {pi + vec3i(0, 0, 0), pi + vec3i(0, 0, 1), pi + vec3i(0, 1, 0),
                  pi + vec3i(0, 1, 1), pi + vec3i(1, 0, 0), pi + vec3i(1, 0, 1),
                  pi + vec3i(1, 1, 0), pi + vec3i(1, 1, 1)};
    float ds[2][2][2] = {{{D(ps[0]), D(ps[1])}, {D(ps[2]), D(ps[3])}},
                         {{D(ps[4]), D(ps[5])}, {D(ps[6]), D(ps[7])}}};

    return TrilinearInterp(ds, p - pi);
}
float GridMedium::D(vec3i p) const {
    if (!Inside(p, vec3i(0), size))
        return 0.0f;
    return density[p.x + p.y * size.x + p.z * size.y * size.x];
}

GridMedium::GridMedium(Spectrum sigma_a, Spectrum sigma_s, PhaseFunction phaseFunction, vec3i size,
                       vec3 position, float scale, pstd::vector<float> density, bool interpolate,
                       SamplingMethod method, float rayMarchingStepSize)
    : sigma_a(sigma_a),
      sigma_s(sigma_s),
      sigma_t((sigma_a + sigma_s)[0]),
      phaseFunction(phaseFunction),
      size(size),
      density(pstd::move(density)),
      interpolate(interpolate),
      method(method),
      rayMarchingStepSize(rayMarchingStepSize / pstd::max(size.x, size.y, size.z)) {
    float maxDensity = 0.0f;
    for (int x = 0; x < size.x; x++)
        for (int y = 0; y < size.y; y++)
            for (int z = 0; z < size.z; z++)
                maxDensity = pstd::max(D(vec3i(x, y, z)), maxDensity);
    invMaxDensity = 1.0f / maxDensity;
    vec3 normalizedSize = size / float(pstd::max(size.x, pstd::max(size.y, size.z)));
    w2m = Translate(vec3(0.5f) - position) * Scale(1.0f / normalizedSize / scale);
    m2w = Inverse(w2m);
}

HomogeneousMedium HomogeneousMedium::Create(const Parameters& params) {
    return HomogeneousMedium(params.GetVec3("sigma_a"), params.GetVec3("sigma_s"),
                             PhaseFunction(params.GetFloat("g", 0.0f)));
}

GridMedium GridMedium::Create(const Parameters& params) {
    auto [density, size] = LoadVolume(params.GetString("file"));
    LOG("[GridMedium]Grid size: &", size);
    LOG("[GridMedium]Memory usage: & MB", sizeof(density[0]) * density.size() / 1000000.0);
    SamplingMethod method = SamplingMethod::DeltaTracking;
    pstd::string samplingMethod = params.GetString("samplingMethod", "deltaTracking");
    SWITCH(samplingMethod) {
        CASE("rayMarching") method = SamplingMethod::RayMarching;
        CASE("deltaTracking") method = SamplingMethod::DeltaTracking;
        DEFAULT LOG_WARNING("[GridMedium]Unknown sampling method \"&\"", samplingMethod);
    }

    float rayMarchingStepSize = params.GetFloat("rayMarchingStepSize", 2.0f);
    if (rayMarchingStepSize < 1.0f) {
        LOG_WARNING("[GridMedium][Create]\"rayMarchingStepSize\" will be clamped to [1 inf]");
        rayMarchingStepSize = 1.0f;
    }

    return GridMedium(params.GetVec3("sigma_a"), params.GetVec3("sigma_s"),
                      PhaseFunction(params.GetFloat("g", 0.0f)), size, params.GetVec3("position"),
                      params.GetFloat("scale", 1.0f), pstd::move(density),
                      params.GetBool("interpolate", true), method, rayMarchingStepSize);
}

Medium CreateMedium(const Parameters& params) {
    pstd::string type = params.GetString("type");
    SWITCH(params.GetString("type")) {
        CASE("Homogeneous") return HomogeneousMedium(HomogeneousMedium::Create(params));
        CASE("Grid") return GridMedium(GridMedium::Create(params));
        DEFAULT {
            LOG_WARNING("[Medium][Create]Unknown type \"&\"", type);
            return HomogeneousMedium(HomogeneousMedium::Create(params));
        }
    }
}

}  // namespace pine