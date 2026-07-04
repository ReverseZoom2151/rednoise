#include "Transform.h"

std::vector<ModelTriangle> transformModel(const std::vector<ModelTriangle> &model, const glm::mat4 &m) {
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(m)));
	std::vector<ModelTriangle> out;
	out.reserve(model.size());
	for (ModelTriangle t : model) {
		for (int i = 0; i < 3; i++) {
			glm::vec4 v = m * glm::vec4(t.vertices[i], 1.0f);
			t.vertices[i] = glm::vec3(v);
			t.vertexNormals[i] = glm::normalize(normalMatrix * t.vertexNormals[i]);
		}
		t.normal = glm::normalize(normalMatrix * t.normal);
		out.push_back(t);
	}
	return out;
}

void appendInstance(std::vector<ModelTriangle> &scene, const std::vector<ModelTriangle> &mesh, const glm::mat4 &m) {
	std::vector<ModelTriangle> transformed = transformModel(mesh, m);
	scene.insert(scene.end(), transformed.begin(), transformed.end());
}

const std::vector<ModelTriangle> &selectLOD(float distance, float switchDistance,
                                            const std::vector<ModelTriangle> &highMesh,
                                            const std::vector<ModelTriangle> &lowMesh) {
	return (distance <= switchDistance) ? highMesh : lowMesh;
}
