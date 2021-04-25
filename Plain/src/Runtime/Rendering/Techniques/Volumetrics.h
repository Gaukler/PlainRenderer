#pragma once
#include "pch.h"
#include "Runtime/Rendering/Backend/RenderBackend.h"

struct VolumetricsSettings {
    glm::vec3 scatteringCoefficients = glm::vec3(1.f);
    float maxDistance = 30.f;
    float absorptionCoefficient = 1.f;
    float baseDensity = 0.003f;
    float densityNoiseRange = 0.008f;   // how strong the noise influences density
    float densityNoiseScale = 0.5f;     // world space scale of the noise pattern
    float phaseFunctionG = 0.2f;        // g of henyey greenstein phase function
};

struct WindSettings {
    glm::vec3 vector = glm::vec3(0.f);
    float speed = 0.15f;
};

class Volumetrics {
public:
    void init(const int screenWidth, const int screenHeight);
    void resizeTextures(const int width, const int height);

    struct Dependencies {
        ImageHandle shadowMap;
        StorageBufferHandle sunShadowInfoBuffer;
        StorageBufferHandle lightBuffer;
        std::vector<RenderPassHandle> parents;  // must be passes that write shadow map and light buffer
    };

    RenderPassHandle computeVolumetricLighting(const VolumetricsSettings& settings, const WindSettings& wind, 
        const Dependencies& dependencies);

    ImageHandle getIntegrationVolume() const;
    UniformBufferHandle getVolumetricsInfoBuffer() const;

private:
    RenderPassHandle m_froxelVolumeMaterialPass;
    RenderPassHandle m_froxelScatteringTransmittancePass;
    RenderPassHandle m_volumetricLightingIntegration;
    RenderPassHandle m_volumetricLightingReprojection;

    ImageHandle m_volumeMaterialVolume;
    ImageHandle m_scatteringTransmittanceVolume;
    ImageHandle m_volumetricLightingHistory[2];
    ImageHandle m_volumetricIntegrationVolume;
    ImageHandle m_perlinNoise3D;

    UniformBufferHandle m_volumetricsSettingsUniforms;

    struct VolumetricsState {
        glm::vec3 windSampleOffset = glm::vec3(0.f);    // offset to apply to density noise, caused by wind
        float sampleOffset = 0.f;
    };

    struct VolumetricsBufferContents {
        VolumetricsState state;
        VolumetricsSettings settings;
    };
    
    VolumetricsState m_state;
};