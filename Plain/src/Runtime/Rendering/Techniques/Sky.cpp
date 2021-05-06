#include "pch.h"
#include "Sky.h"
#include "Common/MeshProcessing.h"

const uint32_t skyTransmissionLutResolution = 128;
const uint32_t skyMultiscatterLutResolution = 32;
const uint32_t skyLutWidth = 200;
const uint32_t skyLutHeight = 100;

void Sky::init() {
    // sky transmission lut
    {
        ImageDescription desc;
        desc.width = skyTransmissionLutResolution;
        desc.height = skyTransmissionLutResolution;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyTransmissionLut = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // sky multiscatter lut
    {
        ImageDescription desc;
        desc.width = skyMultiscatterLutResolution;
        desc.height = skyMultiscatterLutResolution;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyMultiscatterLut = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // sky lut
    {
        ImageDescription desc;
        desc.width = skyLutWidth;
        desc.height = skyLutHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyLut = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // sky atmosphere settings buffer
    {
        UniformBufferDescription desc;
        desc.size = sizeof(AtmosphereSettings);
        m_atmosphereSettingsBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    // skybox cube
    {
        MeshData cubeData;
        cubeData.positions = {
            glm::vec3(-1.f, -1.f, -1.f),
            glm::vec3(1.f, -1.f, -1.f),
            glm::vec3(1.f, 1.f, -1.f),
            glm::vec3(-1.f, 1.f, -1.f),
            glm::vec3(-1.f, -1.f, 1.f),
            glm::vec3(1.f, -1.f, 1.f),
            glm::vec3(1.f, 1.f, 1.f),
            glm::vec3(-1.f, 1.f, 1.f)
        };
        cubeData.uvs = {
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2()
        };
        cubeData.normals = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        cubeData.tangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        cubeData.bitangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        cubeData.indices = {
            0, 1, 3, 3, 1, 2,
            1, 5, 2, 2, 5, 6,
            5, 4, 6, 6, 4, 7,
            4, 0, 7, 7, 0, 3,
            3, 2, 7, 7, 2, 6,
            4, 5, 0, 0, 5, 1
        };
        const std::vector<MeshBinary> cubeBinary = meshesToBinary(std::vector<MeshData>{cubeData}, AABBListFromMeshes({ cubeData }));

        m_skyCube = gRenderBackend.createMeshes(cubeBinary).back();
    }
    // quad 
    {
        MeshData quadData;
        quadData.positions = {
            glm::vec3(-1.f, -1.f, -1.f),
            glm::vec3(1.f, 1.f, -1.f),
            glm::vec3(1.f, -1.f, -1.f),
            glm::vec3(-1.f, 1.f, -1.f)
        };
        quadData.uvs = {
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2()
        };
        quadData.normals = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        quadData.tangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        quadData.bitangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        quadData.indices = {
            0, 1, 2,
            0, 1, 3
        };
        const std::vector<MeshBinary> quadBinary = meshesToBinary(std::vector<MeshData>{quadData}, AABBListFromMeshes({ quadData }));

        m_quad = gRenderBackend.createMeshes(quadBinary).back();
    }
    // sky transmission lut creation pass
    {
        ComputePassDescription skyTransmissionLutPassDesc;
        skyTransmissionLutPassDesc.name = "Sky transmission lut";
        skyTransmissionLutPassDesc.shaderDescription.srcPathRelative = "skyTransmissionLut.comp";
        m_skyTransmissionLutPass = gRenderBackend.createComputePass(skyTransmissionLutPassDesc);
    }
    // sky multiscatter lut
    {
        ComputePassDescription skyMultiscatterPassDesc;
        skyMultiscatterPassDesc.name = "Sky multiscatter lut";
        skyMultiscatterPassDesc.shaderDescription.srcPathRelative = "skyMultiscatterLut.comp";
        m_skyMultiscatterLutPass = gRenderBackend.createComputePass(skyMultiscatterPassDesc);
    }
    // sky lut creation pass
    {
        ComputePassDescription skyLutPassDesc;
        skyLutPassDesc.name = "Sky lut";
        skyLutPassDesc.shaderDescription.srcPathRelative = "skyLut.comp";
        m_skyLutPass = gRenderBackend.createComputePass(skyLutPassDesc);
    }
    // sky pass
    {
        const Attachment colorAttachment(ImageFormat::R11G11B10_uFloat, AttachmentLoadOp::Load);
        const Attachment depthAttachment(ImageFormat::Depth32, AttachmentLoadOp::Load);

        GraphicPassDescription skyPassConfig;
        skyPassConfig.name = "Skybox render";
        skyPassConfig.attachments = { colorAttachment, depthAttachment };
        skyPassConfig.shaderDescriptions.vertex.srcPathRelative = "sky.vert";
        skyPassConfig.shaderDescriptions.fragment.srcPathRelative = "sky.frag";
        skyPassConfig.depthTest.function = DepthFunction::GreaterEqual;
        skyPassConfig.depthTest.write = false;
        skyPassConfig.rasterization.cullMode = CullMode::None;
        skyPassConfig.rasterization.mode = RasterizationeMode::Fill;
        skyPassConfig.blending = BlendState::None;
        skyPassConfig.vertexFormat = VertexFormat::Full;

        m_skyPass = gRenderBackend.createGraphicPass(skyPassConfig);
    }
    // sun sprite
    {
        const auto colorAttachment = Attachment(ImageFormat::R11G11B10_uFloat, AttachmentLoadOp::Load);
        const auto depthAttachment = Attachment(ImageFormat::Depth32, AttachmentLoadOp::Load);

        GraphicPassDescription desc;
        desc.name = "Sun sprite";
        desc.attachments = { colorAttachment, depthAttachment };
        desc.shaderDescriptions.vertex.srcPathRelative = "sunSprite.vert";
        desc.shaderDescriptions.fragment.srcPathRelative = "sunSprite.frag";
        desc.depthTest.function = DepthFunction::GreaterEqual;
        desc.depthTest.write = false;
        desc.rasterization.cullMode = CullMode::None;
        desc.rasterization.mode = RasterizationeMode::Fill;
        desc.blending = BlendState::Additive;
        desc.vertexFormat = VertexFormat::Full;

        m_sunSpritePass = gRenderBackend.createGraphicPass(desc);
    }
}

ImageHandle Sky::getTransmissionLut() const {
    return m_skyTransmissionLut;
}

ImageHandle Sky::getSkyLut() const {
    return m_skyLut;
}

void Sky::issueSkyDrawcalls(const glm::vec2 sunDirection, const glm::mat4& viewProjectionMatrix) {

    gRenderBackend.drawMeshes(std::vector<MeshHandle> {m_skyCube}, nullptr, m_skyPass, 0);

    const float lattitudeOffsetAngle = 90;
    const float longitudeOffsetAngle = -90;
    const float sunAngularDiameter = 0.535f; //from "Physically Based Sky, Atmosphereand Cloud Rendering in Frostbite", page 25
    const float spriteScale = glm::tan(glm::radians(sunAngularDiameter * 0.5f));
    const glm::mat4 spriteScaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(spriteScale, spriteScale, 1.f));
    const glm::mat4 spriteLattitudeRotation = glm::rotate(glm::mat4(1.f), glm::radians(sunDirection.y + lattitudeOffsetAngle), glm::vec3(-1, 0, 0));
    const glm::mat4 spriteLongitudeRotation = glm::rotate(glm::mat4(1.f), glm::radians(sunDirection.x + longitudeOffsetAngle), glm::vec3(0, -1, 0));

    struct SunSpriteMatrices {
        glm::mat4 model;
        glm::mat4 mvp;
    };
    SunSpriteMatrices sunSpriteMatrices;
    sunSpriteMatrices.model = spriteLongitudeRotation * spriteLattitudeRotation * spriteScaleMatrix;
    sunSpriteMatrices.mvp = viewProjectionMatrix * sunSpriteMatrices.model;

    gRenderBackend.drawMeshes(std::vector<MeshHandle> { m_quad }, (char*)&sunSpriteMatrices, m_sunSpritePass, 0);
}

void Sky::updateTransmissionLut() {
    ImageResource lutResource(m_skyTransmissionLut, 0, 0);
    UniformBufferResource atmosphereBufferResource(m_atmosphereSettingsBuffer, 1);

    ComputePassExecution skyTransmissionLutExecution;
    skyTransmissionLutExecution.genericInfo.handle = m_skyTransmissionLutPass;
    skyTransmissionLutExecution.genericInfo.resources.storageImages = { lutResource };
    skyTransmissionLutExecution.genericInfo.resources.uniformBuffers = { atmosphereBufferResource };
    skyTransmissionLutExecution.dispatchCount[0] = skyTransmissionLutResolution / 8;
    skyTransmissionLutExecution.dispatchCount[1] = skyTransmissionLutResolution / 8;
    skyTransmissionLutExecution.dispatchCount[2] = 1;
    gRenderBackend.setComputePassExecution(skyTransmissionLutExecution);
}

void Sky::updateSkyLut(const StorageBufferHandle lightBuffer, const AtmosphereSettings& atmosphereSettings) const {

    gRenderBackend.setUniformBufferData(
        m_atmosphereSettingsBuffer,
        &atmosphereSettings,
        sizeof(atmosphereSettings));

    // compute multiscatter lut
    {
        ImageResource multiscatterLutResource(m_skyMultiscatterLut, 0, 0);
        ImageResource transmissionLutResource(m_skyTransmissionLut, 0, 1);
        UniformBufferResource atmosphereBufferResource(m_atmosphereSettingsBuffer, 3);

        ComputePassExecution skyMultiscatterLutExecution;
        skyMultiscatterLutExecution.genericInfo.handle = m_skyMultiscatterLutPass;
        skyMultiscatterLutExecution.genericInfo.resources.storageImages = { multiscatterLutResource };
        skyMultiscatterLutExecution.genericInfo.resources.sampledImages = { transmissionLutResource };
        skyMultiscatterLutExecution.genericInfo.resources.uniformBuffers = { atmosphereBufferResource };
        skyMultiscatterLutExecution.dispatchCount[0] = skyMultiscatterLutResolution / 8;
        skyMultiscatterLutExecution.dispatchCount[1] = skyMultiscatterLutResolution / 8;
        skyMultiscatterLutExecution.dispatchCount[2] = 1;
        gRenderBackend.setComputePassExecution(skyMultiscatterLutExecution);
    }
    // compute sky lut
    {
        ImageResource lutResource(m_skyLut, 0, 0);
        ImageResource lutTransmissionResource(m_skyTransmissionLut, 0, 1);
        ImageResource lutMultiscatterResource(m_skyMultiscatterLut, 0, 2);
        UniformBufferResource atmosphereBufferResource(m_atmosphereSettingsBuffer, 4);
        StorageBufferResource lightBufferResource(lightBuffer, true, 5);

        ComputePassExecution skyLutExecution;
        skyLutExecution.genericInfo.handle = m_skyLutPass;
        skyLutExecution.genericInfo.resources.storageImages = { lutResource };
        skyLutExecution.genericInfo.resources.sampledImages = { lutTransmissionResource, lutMultiscatterResource };
        skyLutExecution.genericInfo.resources.uniformBuffers = { atmosphereBufferResource };
        skyLutExecution.genericInfo.resources.storageBuffers = { lightBufferResource };
        skyLutExecution.dispatchCount[0] = skyLutWidth / 8;
        skyLutExecution.dispatchCount[1] = skyLutHeight / 8;
        skyLutExecution.dispatchCount[2] = 1;
        gRenderBackend.setComputePassExecution(skyLutExecution);
    }
}

void Sky::renderSky(const ImageHandle colorTarget, const ImageHandle depthTarget,
    const SkyRenderingDependencies dependencies) const {

    // render skybox
    {
        GraphicPassExecution skyPassExecution;
        skyPassExecution.genericInfo.handle = m_skyPass;
        skyPassExecution.targets = {
            RenderTarget { colorTarget, 0 },
            RenderTarget { depthTarget, 0 }
        };
        skyPassExecution.genericInfo.resources.sampledImages = {
            ImageResource(m_skyLut, 0, 0),
            ImageResource(dependencies.volumetricIntegrationVolume, 0, 1)
        };
        skyPassExecution.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(dependencies.volumetricLightingSettingsUniforms, 2)
        };
        gRenderBackend.setGraphicPassExecution(skyPassExecution);
    }
    // sun sprite
    {
        const StorageBufferResource lightBufferResource(dependencies.lightBuffer, true, 0);
        const ImageResource transmissionLutResource(m_skyTransmissionLut, 0, 1);

        GraphicPassExecution sunSpritePassExecution;
        sunSpritePassExecution.genericInfo.handle = m_sunSpritePass;
        sunSpritePassExecution.targets = {
            RenderTarget { colorTarget, 0 },
            RenderTarget { depthTarget, 0 }
        };
        sunSpritePassExecution.genericInfo.resources.storageBuffers = { lightBufferResource };
        sunSpritePassExecution.genericInfo.resources.sampledImages = { transmissionLutResource };
        gRenderBackend.setGraphicPassExecution(sunSpritePassExecution);
    }
}