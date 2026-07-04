#include "ObjLoader.h"

#include <Colour.h>
#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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

// Map a material name to a surface response. Materials literally named "Mirror"
// or "Glass" become reflective / refractive; everything else is diffuse.
static Material materialFor(const std::string &name) {
	if (name == "Mirror")
		return Material::Mirror;
	if (name == "Glass")
		return Material::Glass;
	if (name == "Marble")
		return Material::Procedural;
	if (name == "Bumpy")
		return Material::Bump;
	return Material::Diffuse;
}

namespace {
struct Face {
	int a, b, c;
	Colour colour;
	Material material;
};
} // namespace

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
	std::vector<Face> faces;
	Colour currentColour;
	Material currentMaterial = Material::Diffuse;
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
			currentMaterial = materialFor(mtlName);
		} else if (type == "v") {
			float x, y, z;
			ss >> x >> y >> z;
			vertices.push_back(glm::vec3(x * scale, y * scale, z * scale));
		} else if (type == "f") {
			std::string v1, v2, v3;
			ss >> v1 >> v2 >> v3;
			// Face tokens look like "2/", "2/3/", or "2/3/4"; stoi stops at the slash.
			int i1 = std::stoi(v1) - 1;
			int i2 = std::stoi(v2) - 1;
			int i3 = std::stoi(v3) - 1;
			faces.push_back({i1, i2, i3, currentColour, currentMaterial});
		}
	}

	// Face normals (guard degenerate triangles so we never divide by zero).
	std::vector<glm::vec3> faceNormals(faces.size());
	for (size_t i = 0; i < faces.size(); i++) {
		const Face &f = faces[i];
		glm::vec3 cross = glm::cross(vertices[f.b] - vertices[f.a], vertices[f.c] - vertices[f.a]);
		float len = glm::length(cross);
		faceNormals[i] = (len > 1e-8f) ? cross / len : glm::vec3(0.0f, 1.0f, 0.0f);
	}

	// Which faces touch each vertex, so we can average their normals.
	std::vector<std::vector<int>> vertexFaces(vertices.size());
	for (size_t i = 0; i < faces.size(); i++) {
		vertexFaces[faces[i].a].push_back(static_cast<int>(i));
		vertexFaces[faces[i].b].push_back(static_cast<int>(i));
		vertexFaces[faces[i].c].push_back(static_cast<int>(i));
	}

	// Smooth a vertex normal by averaging only the adjacent face normals that lie
	// within ~60 degrees of this face, so hard edges (e.g. the box corners) stay
	// crisp while genuinely curved surfaces are smoothed.
	const float threshold = 0.5f;
	auto smoothNormal = [&](int vertexIndex, size_t faceIndex) {
		glm::vec3 sum(0.0f);
		for (int g : vertexFaces[vertexIndex]) {
			if (glm::dot(faceNormals[g], faceNormals[faceIndex]) > threshold)
				sum += faceNormals[g];
		}
		return (glm::length(sum) > 0.0f) ? glm::normalize(sum) : faceNormals[faceIndex];
	};

	triangles.reserve(faces.size());
	for (size_t i = 0; i < faces.size(); i++) {
		const Face &f = faces[i];
		ModelTriangle triangle(vertices[f.a], vertices[f.b], vertices[f.c], f.colour);
		triangle.material = f.material;
		triangle.normal = faceNormals[i];
		triangle.vertexNormals = {smoothNormal(f.a, i), smoothNormal(f.b, i), smoothNormal(f.c, i)};
		triangles.push_back(triangle);
	}
	return triangles;
}
