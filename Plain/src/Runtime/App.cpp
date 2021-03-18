#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Timer.h"
#include "Common/ModelLoadSaveBinary.h"
#include "Utilities/MathUtils.h"
#include "Common/sdfUtilities.h"
#include "ImageIO.h"
#include "RuntimeScene.h"
#include "AABB.h"

std::vector<RenderObject> extractRenderObjectFromScene(const std::vector<SceneObject>& scene,
	const std::vector<AxisAlignedBoundingBox>& bbs);

App::App() {
    
}

std::vector<RenderObject> extractRenderObjectFromScene(const std::vector<SceneObject>& scene, 
	const std::vector<AxisAlignedBoundingBox>& bbs) {
	std::vector<RenderObject> renderObjects;
	renderObjects.reserve(scene.size());

	for (const SceneObject& sceneObj : scene) {
		RenderObject renderObj;
		renderObj.mesh = sceneObj.mesh;
		renderObj.modelMatrix = sceneObj.modelMatrix;
		renderObj.previousModelMatrix = sceneObj.modelMatrix;	//FIXME: use previous matrix after transforms are properly supported
		renderObj.bbWorld = axisAlignedBoundingBoxTransformed(bbs[sceneObj.bbIndex], sceneObj.modelMatrix);
		renderObjects.push_back(renderObj);
	}
	return renderObjects;
}

void App::setup(const std::string& sceneFilePath) {

	AxisAlignedBoundingBox sceneBB;

    //load static scene
    {
        SceneBinary scene;
        std::cout << "Loading scene file: " << sceneFilePath << "\n";
        loadBinaryScene(sceneFilePath, &scene);
        const std::vector<MeshHandleFrontend> meshHandles = gRenderFrontend.registerMeshes(scene.meshes);

		m_scene.reserve(scene.objects.size());
		for (const ObjectBinary& objectBinary : scene.objects) {
			SceneObject object;
			object.mesh = meshHandles[objectBinary.meshIndex];
			object.bbIndex = objectBinary.meshIndex;	//FIXME: breaks when loading multiple scenes
			object.modelMatrix = objectBinary.modelMatrix;
			m_scene.push_back(object);
		}

		m_bbs.reserve(scene.meshes.size());
		for (const MeshBinary& mesh : scene.meshes) {
			m_bbs.push_back(mesh.boundingBox);
		}
    }
}

void App::runUpdate() {
    m_cameraController.update();
    gRenderFrontend.setCameraExtrinsic(m_cameraController.getExtrinsic());    
    gRenderFrontend.prepareForDrawcalls();

	const std::vector<RenderObject> renderScene = extractRenderObjectFromScene(m_scene, m_bbs);
    gRenderFrontend.renderScene(renderScene);
}