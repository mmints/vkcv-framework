#pragma once

#include <vector>

#include <vkcv/camera/Camera.hpp>

#include "Mesh.hpp"

namespace vkcv::scene {
	
	class Scene;
	
	class Node {
		friend class Scene;
		
	private:
		Scene* m_scene;
		
		std::vector<Mesh> m_meshes;
		std::vector<Node> m_nodes;
		
		explicit Node(Scene* scene);
		
		void loadMesh(const asset::Scene& asset_scene, const asset::Mesh& asset_mesh);
		
		void recordDrawcalls(std::vector<glm::mat4>& matrices, std::vector<DrawcallInfo>& drawcalls);
	
	public:
		~Node();
		
		Node(const Node& other);
		Node(Node&& other) noexcept;
		
		Node& operator=(const Node& other);
		Node& operator=(Node&& other) noexcept;
		
		Node& addNode();
		
	};
	
}
