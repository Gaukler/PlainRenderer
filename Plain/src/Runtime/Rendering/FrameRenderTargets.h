#pragma once
#include "pch.h"
#include "Runtime/Rendering/RenderHandles.h"

// simple wrapper to keep all images and framebuffers used in a frame in one place
// simplifies keeping resources of multiples frames around for temporal techniques
struct FrameRenderTargets {
    ImageHandle colorBuffer;
    ImageHandle motionBuffer;
    ImageHandle depthBuffer;
};