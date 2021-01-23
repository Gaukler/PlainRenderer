#include "pch.h"
#include "ModelImport.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "Utilities/DirectoryUtils.h"
#include "Common/MeshProcessing.h"

bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData) {

    const std::filesystem::path fullPath = DirectoryUtils::getResourceDirectory() / filename;

    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warning;
    std::string error;

    //parent path removes the filename, so the .mtl file is searched in the directory of the .obj file
    bool success = tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, fullPath.string().c_str(), fullPath.parent_path().string().c_str());

    if (!warning.empty()) {
        std::cout << warning << std::endl;
    }

    if (!error.empty()) {
        std::cerr << error << std::endl;
    }

    if (!success) {
        return false;
    }

    std::vector<uint32_t> materialIndices;

    //iterate over models
    for (uint32_t shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {

        std::vector<MeshData> meshPerMaterial;
        meshPerMaterial.resize(materials.size());
        const auto shape = shapes[shapeIndex];

        //iterate over face vertices
        for (size_t i = 0; i < shape.mesh.indices.size(); i++) {

            int materialIndex = shape.mesh.material_ids[i / 3];

            const auto& indices = shape.mesh.indices[i];
            size_t vertexIndex = indices.vertex_index;
            size_t normalIndex = indices.normal_index;
            size_t uvIndex = indices.texcoord_index;

            meshPerMaterial[materialIndex].positions.push_back(glm::vec3(
                attributes.vertices[vertexIndex * 3],
                attributes.vertices[vertexIndex * 3 + 1],
                -attributes.vertices[vertexIndex * 3 + 2]));

            meshPerMaterial[materialIndex].normals.push_back(glm::vec3(
                attributes.normals[normalIndex * 3],
                attributes.normals[normalIndex * 3 + 1],
                -attributes.normals[normalIndex * 3 + 2]));

            meshPerMaterial[materialIndex].uvs.push_back(glm::vec2(
                attributes.texcoords[uvIndex * 2],
                1.f - attributes.texcoords[uvIndex * 2 + 1]));

            //correct for vulkan coordinate system
            meshPerMaterial[materialIndex].positions.back().y *= -1.f;
            meshPerMaterial[materialIndex].normals.back().y *= -1.f;
        }

        for (int i = 0; i < meshPerMaterial.size(); i++) {
            const auto& mesh = meshPerMaterial[i];
            if (mesh.positions.size() > 0) {
                outData->push_back(mesh);
                materialIndices.push_back(i);
            }
        }
    }

    const auto modelDirectory = fullPath.parent_path();
    for (size_t i = 0; i < outData->size(); i++) {
        computeTangentBitangent(&(*outData)[i]);
        (*outData)[i] = buildIndexedData((*outData)[i]);

        const auto cleanPath = [](std::string path) {

            auto backslashPos = path.find("\\\\");
            while (backslashPos != std::string::npos) {
                path.replace(backslashPos, 2, "\\");
                backslashPos = path.find("\\\\");
            }

            if (path.substr(0, 3) == "..\\") {
                return path.substr(3);
            }
            else {
                return path;
            }
        };

        const auto materialIndex = materialIndices[i];
        const auto material = materials[materialIndex];

        if (material.diffuse_texname.length() != 0) {
            std::filesystem::path diffuseTexture = cleanPath(material.diffuse_texname);
            (*outData)[i].texturePaths.albedoTexturePath = modelDirectory / diffuseTexture;
        }
        else {
            (*outData)[i].texturePaths.albedoTexturePath = "";
        }

        if (material.bump_texname.length() != 0) {
            std::string normalTexture = cleanPath(material.bump_texname);
            (*outData)[i].texturePaths.normalTexturePath = modelDirectory / normalTexture;
        }
        else {
            (*outData)[i].texturePaths.normalTexturePath = "";
        }

        if (material.specular_texname.length() != 0) {
            std::string specularTexture = cleanPath(material.specular_texname);
            (*outData)[i].texturePaths.specularTexturePath = modelDirectory / specularTexture;
        }
        else {
            (*outData)[i].texturePaths.specularTexturePath = "";
        }
    }

    return true;
}

bool retrieveGltfAttribute(const std::map<std::string, int> attributeMap, const std::string attribute, int* outIndex) {
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

void coordinateSystemCorrection(std::vector<glm::vec3>* outData) {
	for (glm::vec3& v : *outData) {
		v.y *= -1;
		v.z *= -1;
	}
}

bool loadModelGLTF(const std::filesystem::path& filename, std::vector<MeshData>* outData) {
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

	for (const tinygltf::Mesh& mesh : model.meshes) {
		for (const tinygltf::Primitive primitive : mesh.primitives) {

			int positionIndex;
			int normalIndex;
			int tangentIndex;
			int uvIndex;

			bool hasAllAttributes =	retrieveGltfAttribute(primitive.attributes, "POSITION", &positionIndex);
			hasAllAttributes &=		retrieveGltfAttribute(primitive.attributes, "NORMAL", &normalIndex);
			hasAllAttributes &=		retrieveGltfAttribute(primitive.attributes, "TANGENT", &tangentIndex);
			hasAllAttributes &=		retrieveGltfAttribute(primitive.attributes, "TEXCOORD_0", &uvIndex);

			if (!hasAllAttributes) {
				continue;
			}

			const std::vector<char> positionBytes	= loadGltfAttribute(model, positionIndex, sizeof(glm::vec3), TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
			const std::vector<char> normalBytes		= loadGltfAttribute(model, normalIndex, sizeof(glm::vec3), TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
			const std::vector<char> tangentBytes	= loadGltfAttribute(model, tangentIndex, sizeof(glm::vec4), TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT);
			const std::vector<char> uvBytes			= loadGltfAttribute(model, uvIndex, sizeof(glm::vec2), TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT);

			MeshData data;

			//position
			data.positions.resize(positionBytes.size() / sizeof(glm::vec3));
			memcpy(data.positions.data(), positionBytes.data(), positionBytes.size());
			coordinateSystemCorrection(&data.positions);

			//normal
			data.normals.resize(normalBytes.size() / sizeof(glm::vec3));
			memcpy(data.normals.data(), normalBytes.data(), normalBytes.size());
			coordinateSystemCorrection(&data.normals);

			//tangents
			data.tangents.reserve(tangentBytes.size() / sizeof(glm::vec4));
			for (int i = 0; i < tangentBytes.size(); i += sizeof(glm::vec4)) {
				data.tangents.push_back(*reinterpret_cast<const glm::vec3*>(tangentBytes.data() + i));
			}
			coordinateSystemCorrection(&data.tangents);

			//bitangent
			assert(data.tangents.size() == data.normals.size());
			data.bitangents.reserve(data.tangents.size());
			for (int i = 0; i < data.tangents.size(); i++) {
				data.bitangents.push_back(glm::normalize(glm::cross(data.tangents[i], data.normals[i])));
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

			const std::filesystem::path modelDirectory = fullPath.parent_path();
			const tinygltf::Material material = model.materials[primitive.material];

			data.texturePaths.albedoTexturePath		= modelDirectory / model.images[model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source].uri;
			data.texturePaths.specularTexturePath	= modelDirectory / model.images[model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source].uri;
			data.texturePaths.normalTexturePath		= modelDirectory / model.images[model.textures[material.normalTexture.index].source].uri;

			outData->push_back(data);
		}
	}
	return true;
}