#pragma once
#include "pch.h"
#include "Rendering/RenderFrontend.h"
#include "CameraController.h"

class App {
public:
	App();
    void setup(const std::string& sceneFilePath);
	void runUpdate();
private:
    CameraController m_cameraController;
	std::vector<SceneObject> m_scene;
	std::vector<AxisAlignedBoundingBox> m_bbs;
};