#include "pch.h"
#include "RenderFrontend.h"
#include "ImageLoader.h"
#include <imgui/imgui.h>
#include <Utilities/MathUtils.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

/*
=========
resizeCallback
=========
*/
void resizeCallback(GLFWwindow* window, int width, int height) {
    RenderFrontend* frontEnd = reinterpret_cast<RenderFrontend*>(glfwGetWindowUserPointer(window));
    frontEnd->setResolution(width, height);
}

/*
=========
setup
=========
*/
void RenderFrontend::setup(GLFWwindow* window) {
    m_window = window;

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    m_camera.intrinsic.aspectRatio = (float)width / (float)height;

    m_backend.setup(window);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, resizeCallback);

    const auto cubeSamplerDesc = SamplerDescription(
        SamplerInterpolation::Linear, 
        SamplerWrapping::Clamp, 
        false, 
        1, 
        SamplerBorderColor::Black, 
        0);
    m_cubeSampler = m_backend.createSampler(cubeSamplerDesc);

    const auto skySamplerDesc = SamplerDescription(
        SamplerInterpolation::Linear,
        SamplerWrapping::Clamp,
        false,
        0,
        SamplerBorderColor::Black,
        m_skyTextureMipCount);
    m_skySamplerWithMips = m_backend.createSampler(skySamplerDesc);

    createShadowPass();
    createMainPass(width, height);
    createSkyPass();
    createSkyCubeMesh();
    createSkyTexturePreparationPasses();
    createSpecularConvolutionPass();
    createDiffuseConvolutionPass();
    createBRDFLutPreparationPass();
}

/*
=========
teardown
=========
*/
void RenderFrontend::teardown() {
    m_backend.teardown();
}

/*
=========
newFrame
=========
*/
void RenderFrontend::newFrame() {
    if (m_didResolutionChange) {
        m_backend.recreateSwapchain(m_screenWidth, m_screenHeight, m_window);
        m_backend.resizeImages( std::vector<ImageHandle>{ m_colorBuffer, m_depthBuffer }, m_screenWidth, m_screenHeight);
        m_didResolutionChange = false;
    }
    if (m_minimized) {
        return;
    }
    m_backend.newFrame();
}

/*
=========
setResolution
=========
*/
void RenderFrontend::setResolution(const uint32_t width, const uint32_t height) {
    m_screenWidth = width;
    m_screenHeight = height;
    m_camera.intrinsic.aspectRatio = (float)width / (float)height;
    if (width == 0 || height == 0) {
        m_minimized = true;
        return;
    }
    else {
        m_minimized = false;
    }
    m_didResolutionChange = true;
}

/*
=========
setCameraMatrix
=========
*/
void RenderFrontend::setCameraExtrinsic(const CameraExtrinsic& extrinsic) {
    m_camera.extrinsic = extrinsic;
    const glm::mat4 viewMatrix = viewMatrixFromCameraExtrinsic(extrinsic);
    const glm::mat4 projectionMatrix = projectionMatrixFromCameraIntrinsic(m_camera.intrinsic);
    const glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    m_backend.setViewProjectionMatrix(viewProjectionMatrix, m_mainPass);
    m_backend.setViewProjectionMatrix(viewProjectionMatrix, m_skyPass);
}

/*
=========
createMesh
=========
*/
MeshHandle RenderFrontend::createMesh(const MeshData& meshData) {
    return m_backend.createMesh(meshData, std::vector<RenderPassHandle> { m_mainPass, m_shadowPass });
}

/*
=========
issueMeshDraw
=========
*/
void RenderFrontend::issueMeshDraw(const MeshHandle mesh, const glm::mat4& modelMatrix) {
    m_backend.drawMesh(mesh, std::vector<RenderPassHandle> { m_mainPass, m_shadowPass }, modelMatrix);
}

/*
=========
renderFrame
=========
*/
void RenderFrontend::renderFrame() {

    if (m_minimized) {
        return;
    }
    
    /*
    additional passes that have to be executed before the main pass
    */
    std::vector<RenderPassHandle> preparationPasses;

    if (m_firstFrame) {

        /*
        write to sky texture
        */
        const auto skyTextureResource = ImageResource(m_skyTexture, 0, 0);
        const auto hdrCaptureResource = ImageResource(m_environmentMapSrc, 0, 1);
        const auto hdrSamplerResource = SamplerResource(m_cubeSampler, 2);

        RenderPassExecution cubeWriteExecution;
        cubeWriteExecution.handle = m_toCubemapPass;
        cubeWriteExecution.resources.storageImages = { skyTextureResource };
        cubeWriteExecution.resources.sampledImages = { hdrCaptureResource };
        cubeWriteExecution.resources.samplers = { hdrSamplerResource };
        cubeWriteExecution.dispatchCount[0] = m_skyTextureRes / 8;
        cubeWriteExecution.dispatchCount[1] = m_skyTextureRes / 8;
        cubeWriteExecution.dispatchCount[2] = 6;
        m_backend.setRenderPassExecution(cubeWriteExecution);

        /*
        create sky texture mips
        */
        for (uint32_t i = 1; i < m_skyTextureMipCount; i++) {
            const uint32_t srcMip = i - 1;
            const auto skyMipSrcResource = ImageResource(m_skyTexture, srcMip, 0);
            const auto skyMipDstResource = ImageResource(m_skyTexture, i, 1);

            RenderPassExecution skyMipExecution;
            skyMipExecution.handle = m_cubemapMipPasses[srcMip];
            skyMipExecution.resources.storageImages = { skyMipSrcResource, skyMipDstResource };
            if (srcMip == 0) {
                skyMipExecution.parents = { m_toCubemapPass };
            }
            else {
                skyMipExecution.parents = { m_cubemapMipPasses[srcMip - 1] };
            }
            skyMipExecution.dispatchCount[0] = m_skyTextureRes / 8 / glm::pow(2, i);
            skyMipExecution.dispatchCount[1] = m_skyTextureRes / 8 / glm::pow(2, i);
            skyMipExecution.dispatchCount[2] = 6;
            m_backend.setRenderPassExecution(skyMipExecution);
        }

        /*
        diffuse convolution
        */
        const auto diffuseProbeResource             = ImageResource(m_diffuseProbe, 0, 0);
        const auto diffuseConvolutionSrcResource    = ImageResource(m_skyTexture, 0, 1);
        const auto cubeSamplerResource              = SamplerResource(m_cubeSampler, 2);

        RenderPassExecution diffuseConvolutionExecution;
        diffuseConvolutionExecution.handle = m_diffuseConvolutionPass;
        diffuseConvolutionExecution.parents = { m_toCubemapPass };
        diffuseConvolutionExecution.resources.storageImages = { diffuseProbeResource };
        diffuseConvolutionExecution.resources.sampledImages = { diffuseConvolutionSrcResource };
        diffuseConvolutionExecution.resources.samplers = { cubeSamplerResource };
        diffuseConvolutionExecution.dispatchCount[0] = m_diffuseProbeRes / 8;
        diffuseConvolutionExecution.dispatchCount[1] = m_diffuseProbeRes / 8;
        diffuseConvolutionExecution.dispatchCount[2] = 6;
        m_backend.setRenderPassExecution(diffuseConvolutionExecution);
        preparationPasses.push_back(m_diffuseConvolutionPass);

        /*
        create brdf lut for split sum approximation
        */
        const auto brdfLutStorageResource = ImageResource(m_brdfLut, 0, 0);

        RenderPassExecution brdfLutExecution;
        brdfLutExecution.handle = m_brdfLutPass;
        brdfLutExecution.resources.storageImages = { brdfLutStorageResource };
        brdfLutExecution.dispatchCount[0] = m_brdfLutRes / 8;
        brdfLutExecution.dispatchCount[1] = m_brdfLutRes / 8;
        brdfLutExecution.dispatchCount[2] = 1;
        m_backend.setRenderPassExecution(brdfLutExecution);
        preparationPasses.push_back(m_brdfLutPass);

        /*
        specular probe convolution
        */
        for (uint32_t i = 0; i < m_specularProbeMipCount; i++) {

            int mipLevel = i;
            BufferDescription mipLevelBufferDesc;
            mipLevelBufferDesc.initialData = &mipLevel;
            mipLevelBufferDesc.size = sizeof(mipLevel);
            mipLevelBufferDesc.type = BufferType::Uniform;
            const auto mipLevelBuffer = m_backend.createUniformBuffer(mipLevelBufferDesc);

            const auto specularProbeResource = ImageResource(m_specularProbe, i, 0);
            const auto specularConvolutionSrcResource = ImageResource(m_skyTexture, 0, 1);
            const auto specCubeSamplerResource = SamplerResource(m_skySamplerWithMips, 2);
            const auto mipBufferResource = UniformBufferResource(mipLevelBuffer, true, 3);

            RenderPassExecution specularConvolutionExecution;
            specularConvolutionExecution.handle = m_specularConvolutionPerMipPasses[i];
            specularConvolutionExecution.parents = { m_toCubemapPass, m_cubemapMipPasses.back() };
            specularConvolutionExecution.resources.storageImages = { specularProbeResource };
            specularConvolutionExecution.resources.sampledImages = { specularConvolutionSrcResource };
            specularConvolutionExecution.resources.samplers = { specCubeSamplerResource };
            specularConvolutionExecution.resources.uniformBuffers = { mipBufferResource };
            specularConvolutionExecution.dispatchCount[0] = m_specularProbeRes / 8;
            specularConvolutionExecution.dispatchCount[1] = m_specularProbeRes / 8;
            specularConvolutionExecution.dispatchCount[2] = 6;
            m_backend.setRenderPassExecution(specularConvolutionExecution);
            preparationPasses.push_back(m_specularConvolutionPerMipPasses[i]);
        }

        m_firstFrame = false;
    }
    /*
    render sun shadow
    */
    RenderPassExecution shadowPassExecution;
    shadowPassExecution.handle = m_shadowPass;
    m_backend.setRenderPassExecution(shadowPassExecution);

    /*
    render scene geometry
    */
    const auto shadowMapResource        = ImageResource(m_shadowMap, 0, 0);
    const auto shadowSamplerResource    = SamplerResource(m_shadowSampler, 1);
    const auto diffuseProbeResource     = ImageResource(m_diffuseProbe, 0, 2);
    const auto cubeSamplerResource      = SamplerResource(m_cubeSampler, 3);
    const auto brdfLutResource          = ImageResource(m_brdfLut, 0, 4);
    const auto specularProbeResource    = ImageResource(m_specularProbe, 0, 5);
    const auto cubeSamplerMipsResource  = SamplerResource(m_skySamplerWithMips, 6);
    const auto lustSamplerResource      = SamplerResource(m_lutSampler, 7);

    RenderPassExecution mainPassExecution;
    mainPassExecution.handle = m_mainPass;
    mainPassExecution.resources.sampledImages = { shadowMapResource, diffuseProbeResource, brdfLutResource, specularProbeResource };
    mainPassExecution.resources.samplers = { shadowSamplerResource, cubeSamplerResource, cubeSamplerMipsResource, lustSamplerResource };
    mainPassExecution.parents = { m_shadowPass };
    mainPassExecution.parents.insert(mainPassExecution.parents.begin(), preparationPasses.begin(), preparationPasses.end());
    m_backend.setRenderPassExecution(mainPassExecution);

    const auto skyTextureResource = ImageResource(m_skyTexture, 0, 0);
    const auto skySamplerResource = SamplerResource(m_cubeSampler, 1);

    /*
    render sky
    */
    RenderPassExecution skyPassExecution;
    skyPassExecution.handle = m_skyPass;
    skyPassExecution.resources.sampledImages = { skyTextureResource };
    skyPassExecution.resources.samplers = { skySamplerResource };
    skyPassExecution.parents = { m_mainPass };
    if (m_firstFrame) {
        skyPassExecution.parents = { m_mainPass, m_toCubemapPass };
    }
    m_backend.setRenderPassExecution(skyPassExecution);

    /*
    update and final commands
    */
    drawUi();
    updateSun();
    updateGlobalShaderInfo();
    m_backend.drawMesh(m_skyCube, std::vector<RenderPassHandle> { m_skyPass }, glm::mat4(1.f));
    m_backend.renderFrame();
}

/*
=========
updateGlobalShaderInfo
=========
*/
void RenderFrontend::updateGlobalShaderInfo() {
    m_globalShaderInfo.cameraPos = glm::vec4(m_camera.extrinsic.position, 1.f);
    m_backend.setGlobalShaderInfo(m_globalShaderInfo);
}

/*
=========
createMainPass
=========
*/
void RenderFrontend::createMainPass(const uint32_t width, const uint32_t height) {
    
    const auto colorBufferDescription = ImageDescription(
        std::vector<char>{},
        width,
        height,
        1,
        ImageType::Type2D,
        ImageFormat::R11G11B10_uFloat,
        (ImageUsageFlags)(ImageUsageFlags::IMAGE_USAGE_ATTACHMENT | ImageUsageFlags::IMAGE_USAGE_STORAGE),
        MipCount::One,
        0,
        false);
    m_colorBuffer = m_backend.createImage(colorBufferDescription);

    const auto depthBufferDescription = ImageDescription(
        std::vector<char>{},
        width,
        height,
        1,
        ImageType::Type2D,
        ImageFormat::Depth32,
        IMAGE_USAGE_ATTACHMENT,
        MipCount::One,
        0,
        false
    );
    m_depthBuffer = m_backend.createImage(depthBufferDescription);

    const auto shadowSamplerDesc = SamplerDescription(
        SamplerInterpolation::Nearest,
        SamplerWrapping::Color,
        false,
        0,
        SamplerBorderColor::White,
        0);
    m_shadowSampler = m_backend.createSampler(shadowSamplerDesc);

    const auto colorAttachment = Attachment(
        m_colorBuffer,
        0,
        0,
        AttachmentLoadOp::Clear);

    const auto depthAttachment = Attachment(
        m_depthBuffer,
        0,
        0,
        AttachmentLoadOp::Clear);

    GraphicPassDescription mainPassConfig;
    mainPassConfig.attachments = { colorAttachment, depthAttachment };
    mainPassConfig.shaderPaths.vertex   = "Shaders\\triangle.vert";
    mainPassConfig.shaderPaths.fragment = "Shaders\\triangle.frag";
    mainPassConfig.depthTest.function = DepthFunction::LessEqual;
    mainPassConfig.depthTest.write = true;
    mainPassConfig.rasterization.cullMode = CullMode::Back;
    mainPassConfig.rasterization.mode = RasterizationeMode::Fill;
    mainPassConfig.blending = BlendState::None;

    m_mainPass = m_backend.createGraphicPass(mainPassConfig);
    m_backend.setSwapchainInputImage(m_colorBuffer);

    const auto cubeSamplerDesc = SamplerDescription(
        SamplerInterpolation::Linear,
        SamplerWrapping::Clamp,
        false,
        1,
        SamplerBorderColor::White,
        0);
    m_cubeSampler = m_backend.createSampler(cubeSamplerDesc);

    const auto lutSamplerDesc = SamplerDescription(
        SamplerInterpolation::Linear,
        SamplerWrapping::Clamp,
        false,
        1,
        SamplerBorderColor::White,
        0);
    m_lutSampler = m_backend.createSampler(lutSamplerDesc);
}

/*
=========
createShadowPass
=========
*/
void RenderFrontend::createShadowPass() {

    const auto shadowMapDesc = ImageDescription(
        std::vector<char>{},
        m_shadowMapRes,
        m_shadowMapRes,
        1,
        ImageType::Type2D,
        ImageFormat::Depth16,
        (ImageUsageFlags)(ImageUsageFlags::IMAGE_USAGE_ATTACHMENT | ImageUsageFlags::IMAGE_USAGE_SAMPLED),
        MipCount::One,
        1,
        false);
    m_shadowMap = m_backend.createImage(shadowMapDesc);

    const auto shadowMapAttachment = Attachment(
        m_shadowMap,
        0,
        0,
        AttachmentLoadOp::Clear);

    GraphicPassDescription shadowPassConfig;
    shadowPassConfig.attachments = { shadowMapAttachment };
    shadowPassConfig.shaderPaths.vertex   = "Shaders\\shadow.vert";
    shadowPassConfig.shaderPaths.fragment = "Shaders\\shadow.frag";
    shadowPassConfig.depthTest.function = DepthFunction::LessEqual;
    shadowPassConfig.depthTest.write = true;
    shadowPassConfig.rasterization.cullMode = CullMode::Front;
    shadowPassConfig.rasterization.mode = RasterizationeMode::Fill;
    shadowPassConfig.blending = BlendState::None;
    m_shadowPass = m_backend.createGraphicPass(shadowPassConfig);
}

/*
=========
createSkyTexturePreparationPasses
=========
*/
void RenderFrontend::createSkyTexturePreparationPasses() {

    ComputePassDescription cubeWriteDesc;
    cubeWriteDesc.shaderPath = "Shaders\\copyToCube.comp";
    m_toCubemapPass = m_backend.createComputePass(cubeWriteDesc);

    ComputePassDescription cubemapMipPassDesc;
    cubemapMipPassDesc.shaderPath = "Shaders\\cubemapMip.comp";
    /*
    first map is written to by different shader
    */
    for (uint32_t i = 0; i < m_skyTextureMipCount - 1; i++) {
        m_cubemapMipPasses.push_back(m_backend.createComputePass(cubemapMipPassDesc));
    }

    ImageDescription hdrCapture = loadImage("textures\\sunset_in_the_chalk_quarry_2k.hdr", false);
    m_environmentMapSrc = m_backend.createImage(hdrCapture);

     const auto hdriSamplerDesc = SamplerDescription(
        SamplerInterpolation::Linear,
        SamplerWrapping::Clamp,
        false,
        0,
        SamplerBorderColor::Black,
        0);
     m_hdriSampler = m_backend.createSampler(hdriSamplerDesc);


}

/*
=========
createDiffuseConvolutionPass
=========
*/
void RenderFrontend::createSpecularConvolutionPass() {

    for (uint32_t i = 0; i < m_specularProbeMipCount; i++) {
        ComputePassDescription specularConvolutionDesc;
        specularConvolutionDesc.shaderPath = "Shaders\\specularCubeConvolution.comp";
        m_specularConvolutionPerMipPasses.push_back(m_backend.createComputePass(specularConvolutionDesc));
    }

    const auto specularProbeDesc = ImageDescription(
        std::vector<char>{},
        m_specularProbeRes,
        m_specularProbeRes,
        1,
        ImageType::TypeCube,
        ImageFormat::R11G11B10_uFloat,
        ImageUsageFlags(IMAGE_USAGE_SAMPLED | IMAGE_USAGE_STORAGE),
        MipCount::Manual,
        m_specularProbeMipCount,
        false);
    m_specularProbe = m_backend.createImage(specularProbeDesc);
}

/*
=========
createDiffuseConvolutionPass
=========
*/
void RenderFrontend::createDiffuseConvolutionPass() {
    ComputePassDescription diffuseConvolutionDesc;
    diffuseConvolutionDesc.shaderPath = "Shaders\\diffuseCubeConvolution.comp";
    m_diffuseConvolutionPass = m_backend.createComputePass(diffuseConvolutionDesc);

    const auto diffuseProbeDesc = ImageDescription(
        std::vector<char>{},
        m_diffuseProbeRes,
        m_diffuseProbeRes,
        1,
        ImageType::TypeCube,
        ImageFormat::R11G11B10_uFloat,
        ImageUsageFlags(IMAGE_USAGE_SAMPLED | IMAGE_USAGE_STORAGE),
        MipCount::One,
        0,
        false);
    m_diffuseProbe = m_backend.createImage(diffuseProbeDesc);
}

/*
=========
createSkyPass
=========
*/
void RenderFrontend::createSkyPass() {

    const auto skyCubeDesc = ImageDescription(
        std::vector<char>{},
        m_skyTextureRes,
        m_skyTextureRes,
        1,
        ImageType::TypeCube,
        ImageFormat::R11G11B10_uFloat,
        (ImageUsageFlags)(IMAGE_USAGE_SAMPLED | IMAGE_USAGE_STORAGE),
        MipCount::Manual,
        8,
        false);
    m_skyTexture = m_backend.createImage(skyCubeDesc);
    const auto colorAttachment = Attachment(m_colorBuffer, 0, 0, AttachmentLoadOp::Load);
    const auto depthAttachment = Attachment(m_depthBuffer, 0, 0, AttachmentLoadOp::Load);

    GraphicPassDescription skyPassConfig;
    skyPassConfig.attachments = { colorAttachment, depthAttachment };
    skyPassConfig.shaderPaths.vertex   = "Shaders\\sky.vert";
    skyPassConfig.shaderPaths.fragment = "Shaders\\sky.frag";
    skyPassConfig.depthTest.function = DepthFunction::LessEqual;
    skyPassConfig.depthTest.write = false;
    skyPassConfig.rasterization.cullMode = CullMode::None;
    skyPassConfig.rasterization.mode = RasterizationeMode::Fill;
    skyPassConfig.blending = BlendState::None;
    
    m_skyPass = m_backend.createGraphicPass(skyPassConfig);
}

/*
=========
createBRDFLutPreparationPass
=========
*/
void RenderFrontend::createBRDFLutPreparationPass() {

    ComputePassDescription brdfLutPassDesc;
    brdfLutPassDesc.shaderPath = "Shaders\\brdfLut.comp";
    m_brdfLutPass = m_backend.createComputePass(brdfLutPassDesc);

    const auto brdfLustDesc = ImageDescription(
        std::vector<char>{},
        m_brdfLutRes,
        m_brdfLutRes,
        1,
        ImageType::Type2D,
        ImageFormat::RG16_sFloat,
        (ImageUsageFlags)(IMAGE_USAGE_SAMPLED | IMAGE_USAGE_STORAGE),
        MipCount::One,
        1,
        false);
    m_brdfLut = m_backend.createImage(brdfLustDesc);
}

/*
=========
createSkyCubeMesh
=========
*/
void RenderFrontend::createSkyCubeMesh() {

    MeshData cubedata;
    cubedata.useMaterial = false;
    cubedata.positions = {
        glm::vec3(-1.f, -1.f, -1.f),
        glm::vec3(1.f, -1.f, -1.f),
        glm::vec3(1.f, 1.f, -1.f),
        glm::vec3(-1.f, 1.f, -1.f),
        glm::vec3(-1.f, -1.f, 1.f),
        glm::vec3(1.f, -1.f, 1.f),
        glm::vec3(1.f, 1.f, 1.f),
        glm::vec3(-1.f, 1.f, 1.f)
    };
    cubedata.indices = {
        0, 1, 3, 3, 1, 2,
        1, 5, 2, 2, 5, 6,
        5, 4, 6, 6, 4, 7,
        4, 0, 7, 7, 0, 3,
        3, 2, 7, 7, 2, 6,
        4, 5, 0, 0, 5, 1
    };
    m_skyCube = m_backend.createMesh(cubedata, std::vector<RenderPassHandle>{ m_skyPass });
}

/*
=========
updateSun
=========
*/
void RenderFrontend::updateSun() {

    m_globalShaderInfo.sunDirection = glm::vec4(directionToVector(m_sunDirection), 0.f);

    /*
    set shadow pass matrix
    */
    const glm::mat4 coordinateSystemCorrection = glm::mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);

    const glm::mat4 P = glm::ortho(-10.f, 10.f, -10.f, 10.f, 1.f, 10.f);
    const glm::mat4 V = glm::lookAt(5.f * glm::vec3(m_globalShaderInfo.sunDirection), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    const glm::mat4 L = coordinateSystemCorrection * P * V;
    m_backend.setViewProjectionMatrix(L, m_shadowPass);

    m_globalShaderInfo.lightMatrix = L;
}

/*
=========
drawUi
=========
*/
void RenderFrontend::drawUi() {
    ImGui::Begin("Rendering");
    if (ImGui::Button("Reload Shaders")) {
        m_backend.reloadShaders();
    }
    ImGui::DragFloat2("Sun direction", &m_sunDirection.x);
    ImGui::ColorEdit4("Sun color", &m_globalShaderInfo.sunColor.x);
    ImGui::End();
}