#include "vkcv/asset/asset_loader.hpp"
#include <iostream>
#include <string.h>	// memcpy(3)
#include <stdlib.h>	// calloc(3)
#include <fx/gltf.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>
#include <vkcv/Logger.hpp>

namespace vkcv::asset {

/**
* convert the accessor type from the fx-gltf library to an unsigned int
* @param type
* @return unsigned integer representation
*/
// TODO Return proper error code (we need to define those as macros or enums,
// will discuss during the next core meeting if that should happen on the scope
// of the vkcv framework or just this module)
uint8_t convertTypeToInt(const fx::gltf::Accessor::Type type) {
	switch (type) {
	case fx::gltf::Accessor::Type::None :
		return 0;
	case fx::gltf::Accessor::Type::Scalar :
		return 1;
	case fx::gltf::Accessor::Type::Vec2 :
		return 2;
	case fx::gltf::Accessor::Type::Vec3 :
		return 3;
	case fx::gltf::Accessor::Type::Vec4 :
		return 4;
	default: return 10; // TODO add cases for matrices (or maybe change the type in the struct itself)
	}
}

/**
 * This function unrolls nested exceptions via recursion and prints them
 * @param e error code
 * @param path path to file that is responsible for error
 */
void print_what (const std::exception& e, const std::string &path) {
	vkcv_log(LogLevel::ERROR, "Loading file %s: %s",
			 path.c_str(), e.what());
	
	try {
		std::rethrow_if_nested(e);
	} catch (const std::exception& nested) {
		print_what(nested, path);
	}
}

/* Translate the component type used in the index accessor of fx-gltf to our
 * enum for index type. The reason we have defined an incompatible enum that
 * needs translation is that only a subset of component types is valid for
 * indices and we want to catch these incompatibilities here. */
enum IndexType getIndexType(const enum fx::gltf::Accessor::ComponentType &t)
{
	switch (t) {
	case fx::gltf::Accessor::ComponentType::UnsignedByte:
		return IndexType::UINT8;
	case fx::gltf::Accessor::ComponentType::UnsignedShort:
		return IndexType::UINT16;
	case fx::gltf::Accessor::ComponentType::UnsignedInt:
		return IndexType::UINT32;
	default:
		std::cerr << "ERROR: Index type not supported: " <<
			static_cast<uint16_t>(t) << std::endl;
		return IndexType::UNDEFINED;
	}
}

std::array<float, 16> computeModelMatrix(std::array<float, 3> translation, std::array<float, 3> scale, std::array<float, 4> rotation, std::array<float, 16> matrix){
    std::array<float, 16> modelMatrix = {1,0,0,0,
                                         0,1,0,0,
                                         0,0,1,0,
                                         0,0,0,1};
    if (matrix != modelMatrix){
        return matrix;
    } else {
        // translation
        modelMatrix[3] = translation[0];
        modelMatrix[7] = translation[1];
        modelMatrix[11] = translation[2];
        // rotation and scale
        auto a = rotation[0];
        auto q1 = rotation[1];
        auto q2 = rotation[2];
        auto q3 = rotation[3];

        modelMatrix[0] = (2 * (a * a + q1 * q1) - 1) * scale[0];
        modelMatrix[1] = (2 * (q1 * q2 - a * q3)) * scale[1];
        modelMatrix[2] = (2 * (q1 * q3 + a * q2)) * scale[2];

        modelMatrix[4] = (2 * (q1 * q2 + a * q3)) * scale[0];
        modelMatrix[5] = (2 * (a * a + q2 * q2) - 1) * scale[1];
        modelMatrix[6] = (2 * (q2 * q3 - a * q1)) * scale[2];

        modelMatrix[8] = (2 * (q1 * q3 - a * q2)) * scale[0];
        modelMatrix[9] = (2 * (q2 * q3 + a * q1)) * scale[1];
        modelMatrix[10] = (2 * (a * a + q3 * q3) - 1) * scale[2];
        return modelMatrix;
    }

}

int loadMesh(const std::string &path, Mesh &mesh) {
	fx::gltf::Document object;

	try {
		if (path.rfind(".glb", (path.length()-4)) != std::string::npos) {
			object = fx::gltf::LoadFromBinary(path);
		} else {
			object = fx::gltf::LoadFromText(path);
		}
	} catch (const std::system_error &err) {
		print_what(err, path);
		return 0;
	} catch (const std::exception &e) {
		print_what(e, path);
		return 0;
	}

	// TODO Temporary restriction: Only one mesh per glTF file allowed
	// currently. Later, we want to support whole scenes with more than
	// just meshes.
	if (object.meshes.size() != 1) return 0;

	fx::gltf::Mesh const &objectMesh = object.meshes[0];
	// TODO We want to support more than one vertex group per mesh
	// eventually... right now this is hard-coded to use only the first one
	// because we only care about the example triangle and cube
	fx::gltf::Primitive const &objectPrimitive = objectMesh.primitives[0];
	fx::gltf::Accessor posAccessor;
	
	std::vector<VertexAttribute> vertexAttributes;
	vertexAttributes.reserve(objectPrimitive.attributes.size());
	
	for (auto const & attrib : objectPrimitive.attributes) {
		fx::gltf::Accessor accessor =  object.accessors[attrib.second];
		VertexAttribute attribute;

		if (attrib.first == "POSITION") {
			attribute.type = PrimitiveType::POSITION;
			posAccessor = accessor;
		} else if (attrib.first == "NORMAL") {
			attribute.type = PrimitiveType::NORMAL;
		} else if (attrib.first == "TEXCOORD_0") {
			attribute.type = PrimitiveType::TEXCOORD_0;
		} else {
			return 0;
		}
		
		attribute.offset = object.bufferViews[accessor.bufferView].byteOffset;
		attribute.length = object.bufferViews[accessor.bufferView].byteLength;
		attribute.stride = object.bufferViews[accessor.bufferView].byteStride;
		attribute.componentType = static_cast<ComponentType>(accessor.componentType);
		
		if (convertTypeToInt(accessor.type) != 10) {
			attribute.componentCount = convertTypeToInt(accessor.type);
		} else {
			return 0;
		}
		
		vertexAttributes.push_back(attribute);
	}

	// TODO consider the case where there is no index buffer (not all
	// meshes have to use indexed rendering)
	const fx::gltf::Accessor &indexAccessor = object.accessors[objectPrimitive.indices];
	const fx::gltf::BufferView &indexBufferView = object.bufferViews[indexAccessor.bufferView];
	const fx::gltf::Buffer &indexBuffer = object.buffers[indexBufferView.buffer];
	
	std::vector<uint8_t> indexBufferData;
	indexBufferData.resize(indexBufferView.byteLength);
	{
		const size_t off = indexBufferView.byteOffset;
		const void *const ptr = ((char*)indexBuffer.data.data()) + off;
		if (!memcpy(indexBufferData.data(), ptr, indexBufferView.byteLength)) {
			vkcv_log(LogLevel::ERROR, "Copying index buffer data");
			return 0;
		}
	}

	const fx::gltf::BufferView&	vertexBufferView	= object.bufferViews[posAccessor.bufferView];
	const fx::gltf::Buffer&		vertexBuffer		= object.buffers[vertexBufferView.buffer];
	
	// FIXME: This only works when all vertex attributes are in one buffer
	std::vector<uint8_t> vertexBufferData;
	vertexBufferData.resize(vertexBuffer.byteLength);
	{
		const size_t off = 0;
		const void *const ptr = ((char*)vertexBuffer.data.data()) + off;
		if (!memcpy(vertexBufferData.data(), ptr, vertexBuffer.byteLength)) {
			vkcv_log(LogLevel::ERROR, "Copying vertex buffer data");
			return 0;
		}
	}

	IndexType indexType;
	switch(indexAccessor.componentType) {
	case fx::gltf::Accessor::ComponentType::UnsignedByte:
		indexType = IndexType::UINT8; break;
	case fx::gltf::Accessor::ComponentType::UnsignedShort:
		indexType = IndexType::UINT16; break;
	case fx::gltf::Accessor::ComponentType::UnsignedInt:
		indexType = IndexType::UINT32; break;
	default:
		vkcv_log(LogLevel::ERROR, "Index type (%u) not supported",
				 static_cast<uint16_t>(indexAccessor.componentType));
		return 0;
	}

	const size_t numVertexGroups = objectMesh.primitives.size();
	
	std::vector<VertexGroup> vertexGroups;
	vertexGroups.reserve(numVertexGroups);
	
	vertexGroups.push_back({
		static_cast<PrimitiveMode>(objectPrimitive.mode),
		object.accessors[objectPrimitive.indices].count,
		posAccessor.count,
		{indexType, indexBufferData},
		{vertexBufferData, vertexAttributes},
		{posAccessor.min[0], posAccessor.min[1], posAccessor.min[2]},
		{posAccessor.max[0], posAccessor.max[1], posAccessor.max[2]},
		static_cast<uint8_t>(objectPrimitive.material)
	});
	
	std::vector<Material> materials;
	std::vector<Texture> textures;
	std::vector<Sampler> samplers;

	std::vector<int> vertexGroupsIndex;

    //glm::mat4 modelMatrix = object.nodes[0].matrix;
	for(int i = 0; i < numVertexGroups; i++){
        vertexGroupsIndex.push_back(i);
	}


	mesh = {
		object.meshes[0].name,
        object.nodes[0].matrix,
		vertexGroupsIndex,
	};

	// FIXME HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK
	// fail quietly if there is no texture
	textures.reserve(1);
	if (object.textures.size()) {
		const std::string mime_type("image/jpeg");
		const fx::gltf::Texture &tex = object.textures[0];
		const fx::gltf::Image &img = object.images[tex.source];
#ifndef NDEBUG
		printf("texture name=%s sampler=%u source=%u\n",
				tex.name.c_str(), tex.sampler, tex.source);
		printf("image   name=%s uri=%s mime=%s\n", img.name.c_str(),
				img.uri.c_str(), img.mimeType.c_str());
#endif
		
		size_t pos = path.find_last_of("/");
		auto dir = path.substr(0, pos);
		
		std::string img_uri = dir + "/" + img.uri;
		int w, h, c;
		uint8_t *data = stbi_load(img_uri.c_str(), &w, &h, &c, 4);
		c = 4;	// FIXME hardcoded to always have RGBA channel layout
		if (!data) {
			std::cerr << "ERROR loading texture image data.\n";
			return 0;
		}
		const size_t byteLen = w * h * c;

		std::vector<uint8_t> imgdata;
		imgdata.resize(byteLen);
		if (!memcpy(imgdata.data(), data, byteLen)) {
			std::cerr << "ERROR copying texture image data.\n";
			free(data);
			return 0;
		}
		free(data);

		textures.push_back({
			0, static_cast<uint8_t>(c),
			static_cast<uint16_t>(w), static_cast<uint16_t>(h),
			imgdata
		});
	}
	// FIXME HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK HACK
	return 1;
}


bool materialHasTexture(const Material *const m, const PBRTextureTarget t)
{
	return m->textureMask & bitflag(t);
}


int loadScene(const std::string &path, Scene &scene){
    fx::gltf::Document sceneObjects;

    try {
        if (path.rfind(".glb", (path.length()-4)) != std::string::npos) {
            sceneObjects = fx::gltf::LoadFromBinary(path);
        } else {
            sceneObjects = fx::gltf::LoadFromText(path);
        }
    } catch (const std::system_error &err) {
        print_what(err, path);
        return 0;
    } catch (const std::exception &e) {
        print_what(e, path);
        return 0;
    }
    size_t pos = path.find_last_of("/");
    auto dir = path.substr(0, pos);

    // file has to contain at least one mesh
    if (sceneObjects.meshes.size() == 0) return 0;


    fx::gltf::Accessor posAccessor;
    std::vector<VertexAttribute> vertexAttributes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    std::vector<Mesh> meshes;
    std::vector<VertexGroup> vertexGroups;
    std::vector<int> vertexGroupsIndex;
    int groupCount = 0;

    Mesh mesh = {};


    for(int i = 0; i < sceneObjects.meshes.size(); i++){
        fx::gltf::Mesh const &objectMesh = sceneObjects.meshes[i];

        for(int j = 0; j < objectMesh.primitives.size(); j++){
            fx::gltf::Primitive const &objectPrimitive = objectMesh.primitives[j];
            vertexAttributes.clear();
            vertexAttributes.reserve(objectPrimitive.attributes.size());

            for (auto const & attrib : objectPrimitive.attributes) {

                fx::gltf::Accessor accessor =  sceneObjects.accessors[attrib.second];
                VertexAttribute attribute;

                if (attrib.first == "POSITION") {
                    attribute.type = PrimitiveType::POSITION;
                    posAccessor = accessor;
                } else if (attrib.first == "NORMAL") {
                    attribute.type = PrimitiveType::NORMAL;
                } else if (attrib.first == "TEXCOORD_0") {
                    attribute.type = PrimitiveType::TEXCOORD_0;
                } else if (attrib.first == "TEXCOORD_1") {
                    attribute.type = PrimitiveType::TEXCOORD_1;
                } else {
                    return 0;
                }

                attribute.offset = sceneObjects.bufferViews[accessor.bufferView].byteOffset;
                attribute.length = sceneObjects.bufferViews[accessor.bufferView].byteLength;
                attribute.stride = sceneObjects.bufferViews[accessor.bufferView].byteStride;
		attribute.componentType = static_cast<ComponentType>(accessor.componentType);

                if (convertTypeToInt(accessor.type) != 10) {
                    attribute.componentCount = convertTypeToInt(accessor.type);
                } else {
                    return 0;
                }

                vertexAttributes.push_back(attribute);
            }

            IndexType indexType;
            std::vector<uint8_t> indexBufferData = {};
            if (objectPrimitive.indices >= 0){ // if there is no index buffer, -1 is returned from fx-gltf
                const fx::gltf::Accessor &indexAccessor = sceneObjects.accessors[objectPrimitive.indices];
                const fx::gltf::BufferView &indexBufferView = sceneObjects.bufferViews[indexAccessor.bufferView];
                const fx::gltf::Buffer &indexBuffer = sceneObjects.buffers[indexBufferView.buffer];

                indexBufferData.resize(indexBufferView.byteLength);
                {
                    const size_t off = indexBufferView.byteOffset;
                    const void *const ptr = ((char*)indexBuffer.data.data()) + off;
                    if (!memcpy(indexBufferData.data(), ptr, indexBufferView.byteLength)) {
                        std::cerr << "ERROR copying index buffer data.\n";
                        return 0;
                    }
                }

		indexType = getIndexType(indexAccessor.componentType);
		if (indexType == IndexType::UNDEFINED) return 0; // TODO return vkcv::error;
            }

            const fx::gltf::BufferView&	vertexBufferView	= sceneObjects.bufferViews[posAccessor.bufferView];
            const fx::gltf::Buffer&		vertexBuffer		= sceneObjects.buffers[vertexBufferView.buffer];

            // FIXME: This only works when all vertex attributes are in one buffer
            std::vector<uint8_t> vertexBufferData;
            vertexBufferData.resize(vertexBuffer.byteLength);
            {
                const size_t off = 0;
                const void *const ptr = ((char*)vertexBuffer.data.data()) + off;
                if (!memcpy(vertexBufferData.data(), ptr, vertexBuffer.byteLength)) {
                    std::cerr << "ERROR copying vertex buffer data.\n";
                    return 0;
                }
            }

            const size_t numVertexGroups = objectMesh.primitives.size();
            vertexGroups.reserve(numVertexGroups);

            vertexGroups.push_back({
                static_cast<PrimitiveMode>(objectPrimitive.mode),
                sceneObjects.accessors[objectPrimitive.indices].count,
                posAccessor.count,
                {indexType, indexBufferData},
                {vertexBufferData, vertexAttributes},
                {posAccessor.min[0], posAccessor.min[1], posAccessor.min[2]},
                {posAccessor.max[0], posAccessor.max[1], posAccessor.max[2]},
                static_cast<uint8_t>(objectPrimitive.material)
            });

            groupCount++;
            vertexGroupsIndex.push_back(groupCount);
        }

        mesh.name = sceneObjects.meshes[i].name;
        mesh.vertexGroups = vertexGroupsIndex;

        meshes.push_back(mesh);
    }

    for(int m = 0; m < sceneObjects.nodes.size(); m++) {
        meshes[sceneObjects.nodes[m].mesh].modelMatrix = computeModelMatrix(sceneObjects.nodes[m].translation,
                                                                            sceneObjects.nodes[m].scale,
                                                                            sceneObjects.nodes[m].rotation,
                                                                            sceneObjects.nodes[m].matrix);
    }

    if (sceneObjects.textures.size() > 0){
        textures.reserve(sceneObjects.textures.size());

        for(int k = 0; k < sceneObjects.textures.size(); k++){
            const fx::gltf::Texture &tex = sceneObjects.textures[k];
            const fx::gltf::Image &img = sceneObjects.images[tex.source];
#ifndef NDEBUG
            printf("texture name=%s sampler=%u source=%u\n",
                   tex.name.c_str(), tex.sampler, tex.source);
            printf("image   name=%s uri=%s mime=%s\n", img.name.c_str(),
                   img.uri.c_str(), img.mimeType.c_str());
#endif
            std::string img_uri = dir + "/" + img.uri;
            int w, h, c;
            uint8_t *data = stbi_load(img_uri.c_str(), &w, &h, &c, 4);
            c = 4;	// FIXME hardcoded to always have RGBA channel layout
            if (!data) {
                std::cerr << "ERROR loading texture image data.\n";
                return 0;
            }
            const size_t byteLen = w * h * c;

            std::vector<uint8_t> imgdata;
            imgdata.resize(byteLen);
            if (!memcpy(imgdata.data(), data, byteLen)) {
                std::cerr << "ERROR copying texture image data.\n";
                free(data);
                return 0;
            }
            free(data);

            textures.push_back({
                0,
                static_cast<uint8_t>(c),
                static_cast<uint16_t>(w),
                static_cast<uint16_t>(h),
                imgdata
            });

        }
    }

    if (sceneObjects.materials.size() > 0){
        materials.reserve(sceneObjects.materials.size());

        for (int l = 0; l < sceneObjects.materials.size(); l++){
            fx::gltf::Material material = sceneObjects.materials[l];
	    // TODO I think we shouldn't set the index for a texture target if
	    // it isn't defined. So we need to test first if there is a normal
	    // texture before assigning material.normalTexture.index.
	    // About the bitmask: If a normal texture is there, modify the
	    // materials textureMask like this:
	    // 		mat.textureMask |= bitflag(asset::normal);
            materials.push_back({
               0,
               material.pbrMetallicRoughness.baseColorTexture.index,
               material.pbrMetallicRoughness.metallicRoughnessTexture.index,
               material.normalTexture.index,
               material.occlusionTexture.index,
               material.emissiveTexture.index,
               {
                   material.pbrMetallicRoughness.baseColorFactor[0],
                   material.pbrMetallicRoughness.baseColorFactor[1],
                   material.pbrMetallicRoughness.baseColorFactor[2],
                   material.pbrMetallicRoughness.baseColorFactor[3]
               },
               material.pbrMetallicRoughness.metallicFactor,
               material.pbrMetallicRoughness.roughnessFactor,
               material.normalTexture.scale,
               material.occlusionTexture.strength,
               {
                   material.emissiveFactor[0],
                   material.emissiveFactor[1],
                   material.emissiveFactor[2]
               }

            });
            printf("baseColor index=%d normal=%d metallic factor=%f\n",
                   materials[l].baseColor, materials[l].normal, materials[l].metallicFactor);
        }
    }

    scene = {
            meshes,
            vertexGroups,
            materials,
            textures,
            samplers
    };

    return 1;
}

}
