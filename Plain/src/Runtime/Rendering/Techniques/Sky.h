#pragma once
#include "Sky.h"
#include "Runtime/Rendering/Backend/RenderBackend.h"

// everything in km
struct AtmosphereSettings {
    glm::vec3 scatteringRayleighGround = glm::vec3(0.0058f, 0.0135f, 0.0331f);
    float earthRadius = 6371;
    glm::vec3 extinctionRayleighGround = scatteringRayleighGround;
    float atmosphereHeight = 100;
    glm::vec3 ozoneExtinction = glm::vec3(0.000650f, 0.001881f, 0.000085f);
    float scatteringMieGround = 0.006f;
    float extinctionMieGround = 1.11f * scatteringMieGround;
    float mieScatteringExponent = 0.76f;
};

class Sky {
public:
    void init();
    ImageHandle getTransmissionLut() const;
    ImageHandle getSkyLut() const;

    //FIXME: currently drawcalls can only be issued after renderpass executions are set
    // this makes for a pretty bad API for the sky rendering
    void issueSkyDrawcalls(const glm::vec2 sunDirection, const glm::mat4& viewProjectionMatrix);

    // returns pass that must be waited on before using the transmission lut
    // separate from the sky lut because the exposure pass depends on the transmission lut 
    // and the other sky luts depend on the exposure
    RenderPassHandle updateTransmissionLut();

    // returns pass that must be waited on before using the luts
    // lightBuffer required as it contains the pre-exposed sun brightness
    // parents are the pass that writes to lightBuffer and the transmission lut pass
    RenderPassHandle updateSkyLut(const StorageBufferHandle lightBuffer, const std::vector<RenderPassHandle> parents,
        const AtmosphereSettings& atmosphereSettings) const;

    struct SkyRenderingDependencies {
        // for applying local volume effects to sky
        ImageHandle         volumetricIntegrationVolume;
        UniformBufferHandle volumetricLightingSettingsUniforms;

        StorageBufferHandle lightBuffer; // contains pre-exposed sun brightness

        // must contain passes that write light buffer and volume integration
        // and sky LUT pass if updated this frame
        std::vector<RenderPassHandle> parents;
    };

    // returns pass that must be waited on before sky is rendered
    RenderPassHandle renderSky(const FramebufferHandle framebuffer, const SkyRenderingDependencies dependencies) const;

private:
    RenderPassHandle m_skyTransmissionLutPass;
    RenderPassHandle m_skyMultiscatterLutPass;
    RenderPassHandle m_skyLutPass;
    RenderPassHandle m_skyPass;
    RenderPassHandle m_sunSpritePass;

    ImageHandle m_skyTransmissionLut;
    ImageHandle m_skyMultiscatterLut;
    ImageHandle m_skyLut;

    UniformBufferHandle m_atmosphereSettingsBuffer;

    MeshHandle m_skyCube;
    MeshHandle m_quad;      //used to render sun sprite
};