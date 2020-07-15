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
    m_screenWidth = width;
    m_screenHeight = height;
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
    createHistogramPasses();
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

    if (m_isMainPassShaderDescriptionStale) {
        m_backend.updateGraphicPassShaderDescription(m_mainPass, m_mainPassShaderConfig);
        m_isMainPassShaderDescriptionStale = false;
    }

    if (m_isBRDFLutShaderDescriptionStale) {
        m_brdfLutPassShaderConfig.specialisationConstants.values[m_brdfLutSpecilisationConstantDiffuseBRDFIndex] 
            = m_mainPassShaderConfig.fragment.specialisationConstants.values[m_mainPassSpecilisationConstantDiffuseBRDFIndex];
        m_backend.updateComputePassShaderDescription(m_brdfLutPass, m_brdfLutPassShaderConfig);
        //don't reset m_isMainPassShaderDescriptionStale, this is done when rendering as it's used to trigger lut recreation
    }

    m_backend.updateShaderCode();

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

        firstFramePreparation();

        preparationPasses.push_back(m_diffuseConvolutionPass);
        preparationPasses.push_back(m_brdfLutPass);
        for (uint32_t i = 0; i < m_specularConvolutionPerMipPasses.size(); i++) {
            preparationPasses.push_back(m_specularConvolutionPerMipPasses[i]);
        }

        m_firstFrame = false;
    }

    if (m_isBRDFLutShaderDescriptionStale) {
        computeBRDFLut();
        preparationPasses.push_back(m_brdfLutPass);
        m_isBRDFLutShaderDescriptionStale = false;
    }

    /*
    render sun shadow
    */
    {
        RenderPassExecution shadowPassExecution;
        shadowPassExecution.handle = m_shadowPass;
        m_backend.setRenderPassExecution(shadowPassExecution);
    }
    
    /*
    histogram and exposure computation
    */
    StorageBufferResource histogramResource(m_histogramBuffer, false, 0);

    {
        //histogram reset
        RenderPassExecution histogramResetExecution;
        histogramResetExecution.handle = m_histogramResetPass;
        histogramResetExecution.resources.storageBuffers = { histogramResource };
        histogramResetExecution.dispatchCount[0] = uint32_t(std::ceilf(m_nHistogramBins / 64.f));
        histogramResetExecution.dispatchCount[1] = 1;
        histogramResetExecution.dispatchCount[2] = 1;

        m_backend.setRenderPassExecution(histogramResetExecution);
    }
    

    //histogram create
    {
        ImageResource colorBufferImageResource(m_colorBuffer, 0, 1);
        StorageBufferResource lightBufferResource(m_lightBuffer, false, 2);

        RenderPassExecution histogramCreateExecution;
        histogramCreateExecution.handle = m_histogramCreationPass;
        histogramCreateExecution.resources.storageBuffers = { histogramResource, lightBufferResource };
        histogramCreateExecution.resources.storageImages = { colorBufferImageResource };
        histogramCreateExecution.parents = { m_histogramResetPass };
        histogramCreateExecution.dispatchCount[0] = uint32_t(std::ceilf((float)m_screenWidth / 8.f));
        histogramCreateExecution.dispatchCount[1] = uint32_t(std::ceilf((float)m_screenHeight / 8.f));
        histogramCreateExecution.dispatchCount[2] = 1;

        m_backend.setRenderPassExecution(histogramCreateExecution);
    }

    //pre expose
    {
        StorageBufferResource lightBufferResource(m_lightBuffer, false, 1);

        RenderPassExecution preExposeLightsExecution;
        preExposeLightsExecution.handle = m_preExposeLightsPass;
        preExposeLightsExecution.resources.storageBuffers = { histogramResource, lightBufferResource };
        preExposeLightsExecution.parents = { m_histogramCreationPass };
        preExposeLightsExecution.dispatchCount[0] = 1;
        preExposeLightsExecution.dispatchCount[1] = 1;
        preExposeLightsExecution.dispatchCount[2] = 1;

        m_backend.setRenderPassExecution(preExposeLightsExecution);
    }
    

    /*
    render scene geometry
    */
    {
        const auto shadowMapResource = ImageResource(m_shadowMap, 0, 0);
        const auto shadowSamplerResource = SamplerResource(m_shadowSampler, 1);
        const auto diffuseProbeResource = ImageResource(m_diffuseProbe, 0, 2);
        const auto cubeSamplerResource = SamplerResource(m_cubeSampler, 3);
        const auto brdfLutResource = ImageResource(m_brdfLut, 0, 4);
        const auto specularProbeResource = ImageResource(m_specularProbe, 0, 5);
        const auto cubeSamplerMipsResource = SamplerResource(m_skySamplerWithMips, 6);
        const auto lustSamplerResource = SamplerResource(m_lutSampler, 7);
        const auto lightBufferResource = StorageBufferResource(m_lightBuffer, true, 8);

        RenderPassExecution mainPassExecution;
        mainPassExecution.handle = m_mainPass;
        mainPassExecution.resources.storageBuffers = { lightBufferResource };
        mainPassExecution.resources.sampledImages = { shadowMapResource, diffuseProbeResource, brdfLutResource, specularProbeResource };
        mainPassExecution.resources.samplers = { shadowSamplerResource, cubeSamplerResource, cubeSamplerMipsResource, lustSamplerResource };
        mainPassExecution.parents = { m_shadowPass, m_preExposeLightsPass };
        mainPassExecution.parents.insert(mainPassExecution.parents.begin(), preparationPasses.begin(), preparationPasses.end());
        m_backend.setRenderPassExecution(mainPassExecution);

    }
    
    

    /*
    render sky
    */
    {
        const auto skyTextureResource = ImageResource(m_skyTexture, 0, 0);
        const auto skySamplerResource = SamplerResource(m_cubeSampler, 1);
        const auto lightBufferResource = StorageBufferResource(m_lightBuffer, true, 2);

        RenderPassExecution skyPassExecution;
        skyPassExecution.handle = m_skyPass;
        skyPassExecution.resources.storageBuffers = { lightBufferResource };
        skyPassExecution.resources.sampledImages = { skyTextureResource };
        skyPassExecution.resources.samplers = { skySamplerResource };
        skyPassExecution.parents = { m_mainPass };
        if (m_firstFrame) {
            skyPassExecution.parents = { m_mainPass, m_toCubemapPass };
        }
        m_backend.setRenderPassExecution(skyPassExecution);
    }

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
firstFramePreparation
=========
*/
void RenderFrontend::firstFramePreparation() {
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
    const auto diffuseProbeResource = ImageResource(m_diffuseProbe, 0, 0);
    const auto diffuseConvolutionSrcResource = ImageResource(m_skyTexture, 0, 1);
    const auto cubeSamplerResource = SamplerResource(m_skySamplerWithMips, 2);

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

    computeBRDFLut();

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
    }
}

/*
=========
computeBRDFLut
=========
*/
void RenderFrontend::computeBRDFLut() {
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

    
    m_mainPassShaderConfig.vertex.srcPathRelative   = "triangle.vert";
    m_mainPassShaderConfig.fragment.srcPathRelative = "triangle.frag";

    const int diffuseBRDFConstantID = 0;
    m_mainPassShaderConfig.fragment.specialisationConstants.locationIDs.push_back(diffuseBRDFConstantID);
    m_mainPassShaderConfig.fragment.specialisationConstants.values.push_back(m_diffuseBRDFDefaultSelection);

    const int directMultiscatterBRDFID = 1;
    const int directMultiscatterBRDFDefaultSelection = 0;
    m_mainPassShaderConfig.fragment.specialisationConstants.locationIDs.push_back(directMultiscatterBRDFID);
    m_mainPassShaderConfig.fragment.specialisationConstants.values.push_back(directMultiscatterBRDFDefaultSelection);

    const int indirectMultiscatterBRDFID = 2;
    const int indirectMultiscatterBRDFDefaultSelection = 1;
    m_mainPassShaderConfig.fragment.specialisationConstants.locationIDs.push_back(indirectMultiscatterBRDFID);
    m_mainPassShaderConfig.fragment.specialisationConstants.values.push_back(indirectMultiscatterBRDFDefaultSelection);

    GraphicPassDescription mainPassDesc;
    mainPassDesc.shaderDescriptions = m_mainPassShaderConfig;
    mainPassDesc.attachments = { colorAttachment, depthAttachment };
    mainPassDesc.depthTest.function = DepthFunction::LessEqual;
    mainPassDesc.depthTest.write = true;
    mainPassDesc.rasterization.cullMode = CullMode::Back;
    mainPassDesc.rasterization.mode = RasterizationeMode::Fill;
    mainPassDesc.blending = BlendState::None;

    m_mainPass = m_backend.createGraphicPass(mainPassDesc);
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
    shadowPassConfig.shaderDescriptions.vertex.srcPathRelative   = "shadow.vert";
    shadowPassConfig.shaderDescriptions.fragment.srcPathRelative = "shadow.frag";
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
    cubeWriteDesc.shaderDescription.srcPathRelative = "copyToCube.comp";
    m_toCubemapPass = m_backend.createComputePass(cubeWriteDesc);

    ComputePassDescription cubemapMipPassDesc;
    cubemapMipPassDesc.shaderDescription.srcPathRelative = "cubemapMip.comp";
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
        specularConvolutionDesc.shaderDescription.srcPathRelative = "specularCubeConvolution.comp";
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
    diffuseConvolutionDesc.shaderDescription.srcPathRelative = "diffuseCubeConvolution.comp";
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
    skyPassConfig.shaderDescriptions.vertex.srcPathRelative   = "sky.vert";
    skyPassConfig.shaderDescriptions.fragment.srcPathRelative = "sky.frag";
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

    m_brdfLutPassShaderConfig.srcPathRelative = "brdfLut.comp";
    const int diffuseBRDFConstantID = 0;
    m_brdfLutPassShaderConfig.specialisationConstants.locationIDs.push_back(diffuseBRDFConstantID);
    m_brdfLutPassShaderConfig.specialisationConstants.values.push_back(m_diffuseBRDFDefaultSelection);

    ComputePassDescription brdfLutPassDesc;
    brdfLutPassDesc.shaderDescription = m_brdfLutPassShaderConfig;
    m_brdfLutPass = m_backend.createComputePass(brdfLutPassDesc);

    const auto brdfLutDesc = ImageDescription(
        std::vector<char>{},
        m_brdfLutRes,
        m_brdfLutRes,
        1,
        ImageType::Type2D,
        ImageFormat::RGBA16_sFloat,
        (ImageUsageFlags)(IMAGE_USAGE_SAMPLED | IMAGE_USAGE_STORAGE),
        MipCount::One,
        1,
        false);
    m_brdfLut = m_backend.createImage(brdfLutDesc);
}

/*
=========
createHistogramPasses
=========
*/
void RenderFrontend::createHistogramPasses() {
    BufferDescription histogramBufferDesc;
    histogramBufferDesc.size = m_nHistogramBins * sizeof(uint32_t);
    histogramBufferDesc.type = BufferType::Storage;
    m_histogramBuffer = m_backend.createStorageBuffer(histogramBufferDesc);

    float initialLightBufferData[3] = { 1.f, 1.f, 1.f };
    BufferDescription lightBufferDesc;
    lightBufferDesc.size = 3 * sizeof(uint32_t);
    lightBufferDesc.type = BufferType::Storage;
    lightBufferDesc.initialData = initialLightBufferData;
    m_lightBuffer = m_backend.createStorageBuffer(lightBufferDesc);

    ComputePassDescription histogramCreationDesc;
    histogramCreationDesc.shaderDescription.srcPathRelative = "histogramCreation.comp";

    const uint32_t nBinsSpecialisationConstantID = 0;
    const uint32_t minLumininanceSpecialisationConstantID = 1;
    const uint32_t maxLumininanceSpecialisationConstantID = 2;
    const uint32_t lumininanceFactorSpecialisationConstantID = 3;
    
    //range is remapped to avoid values < 0, due to problems with log()
    const uint32_t minHistogramValue = 1;
    const uint32_t maxHistogramValue = uint32_t(m_histogramMax / m_histogramMin);
    const uint32_t histogramFactor = uint32_t(1.f / m_histogramMin);

    histogramCreationDesc.shaderDescription.specialisationConstants.locationIDs.push_back(nBinsSpecialisationConstantID);
    histogramCreationDesc.shaderDescription.specialisationConstants.locationIDs.push_back(minLumininanceSpecialisationConstantID);
    histogramCreationDesc.shaderDescription.specialisationConstants.locationIDs.push_back(maxLumininanceSpecialisationConstantID);
    histogramCreationDesc.shaderDescription.specialisationConstants.locationIDs.push_back(lumininanceFactorSpecialisationConstantID);

    histogramCreationDesc.shaderDescription.specialisationConstants.values.push_back(m_nHistogramBins);
    histogramCreationDesc.shaderDescription.specialisationConstants.values.push_back(minHistogramValue);
    histogramCreationDesc.shaderDescription.specialisationConstants.values.push_back(maxHistogramValue);
    histogramCreationDesc.shaderDescription.specialisationConstants.values.push_back(histogramFactor);

    m_histogramCreationPass = m_backend.createComputePass(histogramCreationDesc);

    ComputePassDescription histogramResetDesc;
    histogramResetDesc.shaderDescription.srcPathRelative = "histogramReset.comp";
    histogramResetDesc.shaderDescription.specialisationConstants.locationIDs.push_back(nBinsSpecialisationConstantID);
    histogramResetDesc.shaderDescription.specialisationConstants.values.push_back(m_nHistogramBins);
    m_histogramResetPass = m_backend.createComputePass(histogramResetDesc);

    ComputePassDescription preExposeLightsDesc;
    preExposeLightsDesc.shaderDescription.srcPathRelative = "preExposeLights.comp";

    preExposeLightsDesc.shaderDescription.specialisationConstants.locationIDs.push_back(nBinsSpecialisationConstantID);
    preExposeLightsDesc.shaderDescription.specialisationConstants.locationIDs.push_back(minLumininanceSpecialisationConstantID);
    preExposeLightsDesc.shaderDescription.specialisationConstants.locationIDs.push_back(maxLumininanceSpecialisationConstantID);
    preExposeLightsDesc.shaderDescription.specialisationConstants.locationIDs.push_back(lumininanceFactorSpecialisationConstantID);

    preExposeLightsDesc.shaderDescription.specialisationConstants.values.push_back(m_nHistogramBins);
    preExposeLightsDesc.shaderDescription.specialisationConstants.values.push_back(minHistogramValue);
    preExposeLightsDesc.shaderDescription.specialisationConstants.values.push_back(maxHistogramValue);
    preExposeLightsDesc.shaderDescription.specialisationConstants.values.push_back(histogramFactor);

    m_preExposeLightsPass = m_backend.createComputePass(preExposeLightsDesc);
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
    ImGui::DragFloat2("Sun direction", &m_sunDirection.x);
    ImGui::ColorEdit4("Sun color", &m_globalShaderInfo.sunColor.x);
    ImGui::DragFloat("Exposure offset EV", &m_globalShaderInfo.exposureOffset, 0.1f);
    ImGui::InputFloat("Sun Illuminance Lux", &m_globalShaderInfo.sunIlluminanceLux);
    ImGui::InputFloat("Sky Illuminance Lux", &m_globalShaderInfo.skyIlluminanceLux);

    static bool indirectMultiscatterSelection = 
        m_mainPassShaderConfig.fragment.specialisationConstants.values[m_mainPassSpecilisationConstantUseIndirectMultiscatterBRDFIndex] == 1;
    if (ImGui::Checkbox("Indirect Multiscatter BRDF", &indirectMultiscatterSelection)) {
        m_isMainPassShaderDescriptionStale = true;
        m_mainPassShaderConfig.fragment.specialisationConstants.values[m_mainPassSpecilisationConstantUseIndirectMultiscatterBRDFIndex] 
            = indirectMultiscatterSelection ? 1 : 0;
    }

    const char* diffuseBRDFOptions[] = { "Lambert", "Disney", "CoD WWII", "Titanfall 2" };
    const bool diffuseBRDFChanged = ImGui::Combo("Diffuse BRDF", 
        &m_mainPassShaderConfig.fragment.specialisationConstants.values[m_mainPassSpecilisationConstantDiffuseBRDFIndex], 
        diffuseBRDFOptions, 4);
    m_isMainPassShaderDescriptionStale |= diffuseBRDFChanged;
    m_isBRDFLutShaderDescriptionStale = diffuseBRDFChanged;

    const char* directMultiscatterBRDFOptions[] = { "McAuley", "Simplified", "Scaled GGX lobe", "None" };
    m_isMainPassShaderDescriptionStale |= ImGui::Combo("Direct Multiscatter BRDF",
        &m_mainPassShaderConfig.fragment.specialisationConstants.values[m_mainPassSpecilisationConstantDirectMultiscatterBRDFIndex], 
        directMultiscatterBRDFOptions, 4);

    ImGui::End();
}