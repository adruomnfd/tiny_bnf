#ifndef PINE_CORE_LIGHT_H
#define PINE_CORE_LIGHT_H

#include <core/spectrum.h>
#include <core/sampling.h>
#include <core/ray.h>
#include <util/taggedvariant.h>
#include <util/profiler.h>

#include <pstd/vector.h>

namespace pine {

struct LightSample {
    Spectrum Le;
    vec3 wo;
    float distance = 1.0f;
    float pdf = 1.0f;
    bool isDelta = false;
};

struct LightLeSample {
    Spectrum Le;
    Ray ray;
    SpatialPdf pdf;
};

struct PointLight {
    static PointLight Create(const Parameters& params);
    PointLight(vec3 position, vec3 color)
        : position(position), color(Spectrum(color, SpectrumType::Illuminant)){};

    LightSample Sample(vec3 p, vec2 u2) const;
    LightLeSample SampleLe(vec2, vec2 ud) const;
    SpatialPdf PdfLe(const Ray&) const;
    Spectrum Power() const {
        return color * Pi4;
    }
    bool IsDelta() const {
        return true;
    }

    vec3 position;
    Spectrum color;
};

struct DirectionalLight {
    static DirectionalLight Create(const Parameters& params);
    DirectionalLight(vec3 direction, vec3 color)
        : direction(Normalize(direction)), color(Spectrum(color, SpectrumType::Illuminant)){};

    LightSample Sample(vec3 p, vec2 u2) const;
    LightLeSample SampleLe(vec2, vec2) const {
        return {};
    }
    SpatialPdf PdfLe(const Ray&) const {
        return {};
    }
    Spectrum Power() const {
        return {};
    }
    bool IsDelta() const {
        return true;
    }

    vec3 direction;
    Spectrum color;
};

struct AreaLight {
    LightSample Sample(vec3 p, vec2 u2) const;
    LightLeSample SampleLe(vec2, vec2) const;
    SpatialPdf PdfLe(const Ray&) const;
    Spectrum Power() const;
    bool IsDelta() const {
        return false;
    }

    const Shape* shape = nullptr;
};

struct Sky {
    static Sky Create(const Parameters& params);
    Sky(vec3 sunDirection, Spectrum sunColor)
        : sunDirection(Normalize(sunDirection)), sunColor(sunColor) {
    }

    LightSample Sample(vec3 p, vec2 u2) const;
    Spectrum Color(vec3 wo) const;
    float Pdf(vec3 wo) const;
    LightLeSample SampleLe(vec2, vec2) const {
        return {};
    }
    SpatialPdf PdfLe(const Ray&) const {
        return {};
    }
    Spectrum Power() const {
        return {};
    }
    bool IsDelta() const {
        return false;
    }

    vec3 sunDirection;
    Spectrum sunColor;
};

struct Atmosphere {
    static Atmosphere Create(const Parameters& params);
    Atmosphere(vec3 sunDirection, Spectrum sunColor, vec2i size, bool interpolate);

    LightSample Sample(vec3 p, vec2 u2) const;
    Spectrum Color(vec3 wo) const;
    float Pdf(vec3 wo) const;
    LightLeSample SampleLe(vec2, vec2) const {
        return {};
    }
    SpatialPdf PdfLe(const Ray&) const {
        return {};
    }
    Spectrum Power() const {
        return {};
    }
    bool IsDelta() const {
        return false;
    }

    vec3 sunDirection;
    Spectrum sunColor;
    Spectrum sunSampledColor;
    vec2i size;
    pstd::vector<vec3> colors;
    bool interpolate = true;
};

struct EnvironmentLight : TaggedVariant<Atmosphere, Sky> {
    using TaggedVariant::TaggedVariant;
    static EnvironmentLight Create(const Parameters& params);

    LightSample Sample(vec3 p, vec2 u2) const {
        SampledProfiler _(ProfilePhase::SampleEnvLight);
        return Dispatch([&](auto&& x) { return x.Sample(p, u2); });
    }
    Spectrum Color(vec3 wo) const {
        SampledProfiler _(ProfilePhase::SampleEnvLight);
        return Dispatch([&](auto&& x) { return x.Color(wo); });
    }
    float Pdf(vec3 wo) const {
        SampledProfiler _(ProfilePhase::SampleEnvLight);
        return Dispatch([&](auto&& x) { return x.Pdf(wo); });
    }
    LightLeSample SampleLe(vec2 up, vec2 ud) const {
        SampledProfiler _(ProfilePhase::SampleEnvLight);
        return Dispatch([&](auto&& x) { return x.SampleLe(up, ud); });
    }
    SpatialPdf PdfLe(const Ray& ray) const {
        SampledProfiler _(ProfilePhase::SampleEnvLight);
        return Dispatch([&](auto&& x) { return x.PdfLe(ray); });
    }
    Spectrum Power() const {
        SampledProfiler _(ProfilePhase::SampleEnvLight);
        return Dispatch([&](auto&& x) { return x.Power(); });
    }
    bool IsDelta() const {
        return false;
    }
};

struct Light : TaggedVariant<PointLight, DirectionalLight, AreaLight, EnvironmentLight> {
    using TaggedVariant::TaggedVariant;

    LightSample Sample(vec3 p, vec2 u2) const {
        return Dispatch([&](auto&& x) { return x.Sample(p, u2); });
    }
    LightLeSample SampleLe(vec2 up, vec2 ud) const {
        return Dispatch([&](auto&& x) { return x.SampleLe(up, ud); });
    }
    SpatialPdf PdfLe(const Ray& ray) const {
        return Dispatch([&](auto&& x) { return x.PdfLe(ray); });
    }
    Spectrum Power() const {
        return Dispatch([&](auto&& x) { return x.Power(); });
    }
    bool IsDelta() const {
        return Dispatch([&](auto&& x) { return x.IsDelta(); });
    }
};

Light CreateLight(const Parameters& params);

}  // namespace pine

#endif