#include "pch.h"
#include "ModelImport.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
//#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "Utilities/DirectoryUtils.h"
#include "Common/MeshProcessing.h"

//---- private function declarations ----
bool getGltfAttributeIndex(const std::map<std::string, int> attributeMap, const std::string attribute, int* outIndex);
std::vector<char> loadGltfAttribute(const tinygltf::Model& model, const int attributeIndex,
	const size_t typeSize, const int tinyGltfExpectedType, const int tinyGltfExpectedComponentType);
glm::mat4 computeNodeMatrix(const tinygltf::Node& node);

//---- implementation ----

bool getGltfAttributeIndex(const std::map<std::string, int> attributeMap, const std::string attribute, int* outIndex) {
	if (attributeMap.find(attribute) == attributeMap.end()) {
		std::cout << "Primitive missing attribute: " << attribute << "\n";
		return false;
	}
	else {
		*outIndex = attributeMap.at(attribute);
		return true;
	}
}

std::vector<char> loadGltfAttribute(const tinygltf::Model& model, const int attributeIndex,
	const size_t typeSize, const int tinyGltfExpectedType, const int tinyGltfExpectedComponentType) {	

	const tinygltf::Accessor accessor = model.accessors[attributeIndex];
	const tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
	assert(accessor.type == tinyGltfExpectedType);
	assert(accessor.componentType == tinyGltfExpectedComponentType);
	assert(bufferView.byteStride == 0);

	const size_t byteCount = accessor.count * typeSize;
	assert(bufferView.byteLength == byteCount);

	std::vector<char> byteData(byteCount);
	memcpy(byteData.data(), buffer.data.data() + bufferView.byteOffset, byteCount);
	return byteData;
}

glm::mat4 computeNodeMatrix(const tinygltf::Node& node) {
	glm::mat4 rotation(1.f);
	glm::mat4 translation(1.f);
	glm::mat4 scale(1.f);
	if (node.rotation.size() == 4) {
		//gltf stores quaternion  (x, y, z, w)
		//glm constructor expects (w, x, y, z)
		glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
		rotation = glm::toMat4(q);
	}
	if (node.translation.size() == 3) {
		translation = glm::translate(glm::mat4(1.f), glm::vec3(node.translation[0], node.translation[1], node.translation[2]));

	}
	if (node.scale.size() == 3) {
		scale = glm::scale(glm::mat4(1.f), glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
	}
	return translation * rotation * scale;
}

glm::vec3 computeMeanAlbedo(const tinygltf::Image& image) {
	if (image.component != 4) {
		std::cout << "computeMeanAlbedo: expecting four component image\n";
		return glm::vec3(1.f, 0.f, 0.f);
	}
	if (image.bits != 8) {
		std::cout << "computeMeanAlbedo: expecting 8 bit image\n";
		return glm::vec3(1.f, 0.f, 0.f);
	}

	size_t redTotal		= 0;
	size_t greenTotal	= 0;
	size_t blueTotal	= 0;

	const float maxValue = 255.f; //8 bit chars

	//advance one pixel (four bytes) at a time
	for (size_t i = 0; i < image.image.size(); i += 4) {
		const size_t red   = image.image[i];
		const size_t green  = image.image[i+1];
		const size_t blue = image.image[i+2];
		const float alpha = image.image[i+3] / maxValue;

		redTotal	+= red	 * alpha;
		greenTotal	+= green * alpha;
		blueTotal	+= blue	 * alpha;
	}

	const size_t pixelCount = image.image.size() / 4;

	return glm::vec3(
		redTotal   / maxValue / pixelCount, 
		greenTotal / maxValue / pixelCount, 
		blueTotal  / maxValue / pixelCount);
}

bool loadModelGLTF(const std::filesystem::path& filename, Scene* outScene) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string error;
	std::string warning;

	const std::filesystem::path fullPath = DirectoryUtils::getResourceDirectory() / filename;

	const bool success = loader.LoadASCIIFromFile(&model, &error, &warning, fullPath.string().c_str());

	if (!warning.empty()) {
		std::cout << "tinygltf warning:\n" << warning << "\n";
	}

	if (!error.empty()) {
		std::cout << "tinygltf error:\n" << error << "\n";
	}

	if (!success) {
		std::cout << "Failed to parse glTF\n";
		return false;
	}

	std::vector<std::vector<size_t>> perMeshPrimitives;	//indices into outScene->meshes

	//load meshes
	for (const tinygltf::Mesh& mesh : model.meshes) {
		std::vector<size_t> primitiveList;
		for (const tinygltf::Primitive primitive : mesh.primitives) {

			int positionIndex;
			int normalIndex;
			int tangentIndex;
			int uvIndex;

			bool hasAllAttributes =	getGltfAttributeIndex(primitive.attributes, "POSITION", &positionIndex);
			hasAllAttributes &=		getGltfAttributeIndex(primitive.attributes, "NORMAL", &normalIndex);
			hasAllAttributes &=		getGltfAttributeIndex(primitive.attributes, "TANGENT", &tangentIndex);
			hasAllAttributes &=		getGltfAttributeIndex(primitive.attributes, "TEXCOORD_0", &uvIndex);

			if (!hasAllAttributes) {
				std::cout << "File contains meshes with missing attributes: " << filename << "\n";
				return false;
			}

			const std::vector<char> positionBytes	= loadGltfAttribute(model, positionIndex, sizeof(glm::vec3), TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
			const std::vector<char> normalBytes		= loadGltfAttribute(model, normalIndex, sizeof(glm::vec3), TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
			const std::vector<char> tangentBytes	= loadGltfAttribute(model, tangentIndex, sizeof(glm::vec4), TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT);
			const std::vector<char> uvBytes			= loadGltfAttribute(model, uvIndex, sizeof(glm::vec2), TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT);

			MeshData data;

			//position
			data.positions.resize(positionBytes.size() / sizeof(glm::vec3));
			memcpy(data.positions.data(), positionBytes.data(), positionBytes.size());

			//normal
			data.normals.resize(normalBytes.size() / sizeof(glm::vec3));
			memcpy(data.normals.data(), normalBytes.data(), normalBytes.size());

			//tangents
			data.tangents.reserve(tangentBytes.size() / sizeof(glm::vec4));
			for (int i = 0; i < tangentBytes.size(); i += sizeof(glm::vec4)) {
				data.tangents.push_back(*reinterpret_cast<const glm::vec3*>(tangentBytes.data() + i));
			}

			//bitangent
			assert(data.tangents.size() == data.normals.size());
			data.bitangents.reserve(data.tangents.size());
			for (int i = 0; i < data.tangents.size(); i++) {
				data.bitangents.push_back(glm::normalize(glm::cross(data.tangents[i], data.normals[i])));
			}

			//correct coordinate system
			for (glm::vec3& p : data.positions) {
				p.y *= -1;
				p.z *= -1;
			}
			for (glm::vec3& n : data.normals) {
				n.y *= -1;
				n.z *= -1;
			}

			//uvs
			data.uvs.resize(uvBytes.size() / sizeof(glm::vec2));
			memcpy(data.uvs.data(), uvBytes.data(), uvBytes.size());

			//indices
			const std::vector<char> indexBytes = loadGltfAttribute(model, primitive.indices, sizeof(uint16_t), TINYGLTF_TYPE_SCALAR, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
			data.indices.reserve(indexBytes.size() / sizeof(uint16_t));
			for (int i = 0; i < indexBytes.size(); i += sizeof(uint16_t)) {
				data.indices.push_back(*reinterpret_cast<const glm::uint16_t*>(&indexBytes[i]));
			}

			//material textures
			const std::filesystem::path modelDirectory = fullPath.parent_path();
			const tinygltf::Material material = model.materials[primitive.material];

			const tinygltf::Image albedoImage = model.images[model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source];
			
			data.meanAlbedo = computeMeanAlbedo(albedoImage);

			data.texturePaths.albedoTexturePath		= modelDirectory / albedoImage.uri;
			data.texturePaths.specularTexturePath	= modelDirectory / model.images[model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source].uri;
			data.texturePaths.normalTexturePath		= modelDirectory / model.images[model.textures[material.normalTexture.index].source].uri;

			//sdf texture path
			const size_t primitiveIndex = primitiveList.size();
			std::string primitiveName = mesh.name;
			primitiveName += primitiveIndex > 0 ? "_" + std::to_string(primitiveIndex) : "";
			data.texturePaths.sdfTexturePath = modelDirectory / "sdfTextures" / primitiveName;
			data.texturePaths.sdfTexturePath += ".dds";

			primitiveList.push_back(outScene->meshes.size());
			outScene->meshes.push_back(data);
		}
		perMeshPrimitives.push_back(primitiveList);
	}

	//load objects
	for (const tinygltf::Scene& scene : model.scenes) {
		//nodes in scene.nodes must be root nodes
		std::vector<int> nodesToProcess = scene.nodes;
		glm::mat4 initialMatrix = glm::mat4(1.f);
		std::vector<glm::mat4> parentMatrices(nodesToProcess.size(), initialMatrix);

		while (nodesToProcess.size() != 0) {

			//process and remove last node
			const int currentNodeIndex = nodesToProcess[nodesToProcess.size() - 1];
			nodesToProcess.pop_back();

			const glm::mat4 parentMatrix = parentMatrices[parentMatrices.size() - 1];
			parentMatrices.pop_back();

			tinygltf::Node node = model.nodes[currentNodeIndex];
			const glm::mat4 modelMatrix = parentMatrix * computeNodeMatrix(node);

			//add children to process stack
			for (const int childIndex : node.children) {
				nodesToProcess.push_back(childIndex);
				parentMatrices.push_back(modelMatrix);
			}

			//skip nodes without mesh
			if (node.mesh != -1) {
				ObjectBinary obj;

				if (glm::determinant(modelMatrix) < 0) {
					std::cout << "Negative model matrix determinant will cause winding order problems: " << node.name << "\n";
				}

				glm::mat4 correctionMatrix(1.f);
				correctionMatrix[1][1] = -1;
				correctionMatrix[2][2] = -1;

				//to change the coordinate system the correction matrix c has to be applied at the end: 
				//world = c * m1 * m2 *... * vertex
				//where does the second c in the code come from:
				//the vertices are also corrected, so the meshes are correctly displayed when using an identity matrix I
				//corrected vertex: 
				//vertex_c = c * vertex
				//c = c_inverse, so c * c = I
				//because of this:
				//c * vertex_c = c * c * vertex = I * vertex = vertex
				//so final transform is:
				//world = c * m * c * vertex_c
				obj.modelMatrix = correctionMatrix * modelMatrix * correctionMatrix;

				for (const size_t primitiveIndex : perMeshPrimitives[node.mesh]) {
					obj.meshIndex = primitiveIndex;
					outScene->objects.push_back(obj);
				}
			}
		}
	}
	return true;
}