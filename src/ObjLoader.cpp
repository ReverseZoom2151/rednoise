#include "ObjLoader.h"

#include <Colour.h>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

// Parse a Wavefront .mtl file into a name -> diffuse-colour map.
static std::map<std::string, Colour> loadMTL(const std::string &filename) {
	std::map<std::string, Colour> materials;
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cout << "Failed to open material file: " << filename << std::endl;
		return materials;
	}
	std::string line;
	std::string currentName;
	while (std::getline(file, line)) {
		std::stringstream ss(line);
		std::string type;
		ss >> type;
		if (type == "newmtl") {
			ss >> currentName;
		} else if (type == "Kd" && !currentName.empty()) {
			float r, g, b;
			ss >> r >> g >> b;
			materials[currentName] =
			    Colour(currentName, static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255));
		}
	}
	return materials;
}

std::vector<ModelTriangle> loadOBJ(const std::string &filename, float scale) {
	std::vector<ModelTriangle> triangles;
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cout << "Failed to open file: " << filename << std::endl;
		return triangles;
	}
	// Directory containing the .obj, so a relative mtllib path resolves correctly.
	std::string directory;
	size_t slash = filename.find_last_of("/\\");
	if (slash != std::string::npos)
		directory = filename.substr(0, slash + 1);

	std::map<std::string, Colour> materials;
	std::vector<glm::vec3> vertices;
	Colour currentColour;
	std::string line;
	while (std::getline(file, line)) {
		std::stringstream ss(line);
		std::string type;
		ss >> type;
		if (type == "mtllib") {
			std::string mtlName;
			ss >> mtlName;
			materials = loadMTL(directory + mtlName);
		} else if (type == "usemtl") {
			std::string mtlName;
			ss >> mtlName;
			auto it = materials.find(mtlName);
			if (it != materials.end())
				currentColour = it->second;
		} else if (type == "v") {
			float x, y, z;
			ss >> x >> y >> z;
			vertices.push_back(glm::vec3(x * scale, y * scale, z * scale));
		} else if (type == "f") {
			std::string v1, v2, v3;
			ss >> v1 >> v2 >> v3;
			// Face tokens look like "2/", "2/3/", or "2/3/4"; stoi stops at the slash.
			int index1 = std::stoi(v1) - 1;
			int index2 = std::stoi(v2) - 1;
			int index3 = std::stoi(v3) - 1;
			triangles.push_back(ModelTriangle(vertices[index1], vertices[index2], vertices[index3], currentColour));
		}
	}
	return triangles;
}
