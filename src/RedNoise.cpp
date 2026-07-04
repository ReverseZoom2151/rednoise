#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <iostream>
#include <iomanip>
#include <CanvasPoint.h>
#include <Colour.h>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <random>
#include <TextureMap.h>
#include <ModelTriangle.h>
#include <sstream>
#include <map>
#include <string>

#define WIDTH 320
#define HEIGHT 240

// ---------------------------------------------------------------------------
// Small shared helpers
// ---------------------------------------------------------------------------

// Pack 8-bit RGB into the window's 0xAARRGGBB pixel format (alpha forced opaque).
uint32_t packColour(int red, int green, int blue) {
	return (255u << 24) | ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
}

// A single, well-seeded random source for the interactive demos.
static int randInt(int upperExclusive) {
	static std::mt19937 generator{std::random_device{}()};
	std::uniform_int_distribution<int> distribution(0, upperExclusive - 1);
	return distribution(generator);
}

// ---------------------------------------------------------------------------
// Interpolation
// ---------------------------------------------------------------------------

std::vector<float> interpolateSingleFloats(float from, float to, size_t numberOfValues) {
	std::vector<float> result;
	if (numberOfValues == 0) return result;
	if (numberOfValues == 1) {
		result.push_back(from);
		return result;
	}
	float step = (to - from) / (numberOfValues - 1);
	for (size_t i = 0; i < numberOfValues; i++) {
		result.push_back(from + (i * step));
	}
	return result;
}

void interpolateSingleFloatsExample() {
	std::vector<float> result = interpolateSingleFloats(2.2, 8.5, 7);
	for (size_t i = 0; i < result.size(); i++) {
		std::cout << result[i] << " ";
	}
	std::cout << std::endl;
}

std::vector<glm::vec3> interpolateThreeElementValues(const glm::vec3 &from, const glm::vec3 &to, int numberOfValues) {
	std::vector<glm::vec3> result;
	if (numberOfValues <= 0) return result;
	if (numberOfValues == 1) {
		result.push_back(from);
		return result;
	}
	float stepSize = 1.0f / (numberOfValues - 1);
	for (int i = 0; i < numberOfValues; i++) {
		float t = stepSize * i;
		result.push_back(from + (t * (to - from)));
	}
	return result;
}

void interpolateThreeElementValuesExample() {
	glm::vec3 from(1.0, 4.0, 9.2);
	glm::vec3 to(4.0, 1.0, 9.8);
	std::vector<glm::vec3> result = interpolateThreeElementValues(from, to, 4);
	for (size_t i = 0; i < result.size(); i++) {
		std::cout << "(" << result[i].x << ", " << result[i].y << ", " << result[i].z << ") ";
	}
	std::cout << std::endl;
}

// Scalar linear map of x from [x1,x2] onto [y1,y2]. Degenerate source range -> y1.
float interpolation(float x, float x1, float x2, float y1, float y2) {
	if (x2 == x1) return y1;
	return y1 + (x - x1) * ((y2 - y1) / (x2 - x1));
}

// ---------------------------------------------------------------------------
// Simple full-screen demos
// ---------------------------------------------------------------------------

void redNoise(DrawingWindow &window) {
	for (size_t y = 0; y < window.height; y++) {
		for (size_t x = 0; x < window.width; x++) {
			window.setPixelColour(x, y, packColour(randInt(256), 0, 0));
		}
	}
}

void singleDimensionGreyscaleInterpolation(DrawingWindow &window) {
	for (size_t y = 0; y < window.height; y++) {
		for (size_t x = 0; x < window.width; x++) {
			float gradient = 1.0f - static_cast<float>(x) / window.width;
			int grey = static_cast<int>(gradient * 255);
			window.setPixelColour(x, y, packColour(grey, grey, grey));
		}
	}
}

void twoDimensionalColourInterpolation(DrawingWindow &window) {
	glm::vec3 topLeft(255, 0, 0);
	glm::vec3 topRight(0, 0, 255);
	glm::vec3 bottomRight(0, 255, 0);
	glm::vec3 bottomLeft(255, 255, 0);
	std::vector<glm::vec3> leftColumnColours = interpolateThreeElementValues(topLeft, bottomLeft, window.height);
	std::vector<glm::vec3> rightColumnColours = interpolateThreeElementValues(topRight, bottomRight, window.height);
	for (size_t y = 0; y < window.height; y++) {
		std::vector<glm::vec3> rowColours =
		        interpolateThreeElementValues(leftColumnColours[y], rightColumnColours[y], window.width);
		for (size_t x = 0; x < window.width; x++) {
			window.setPixelColour(x, y,
			                      packColour(int(rowColours[x].x), int(rowColours[x].y), int(rowColours[x].z)));
		}
	}
}

// ---------------------------------------------------------------------------
// Lines and triangles
// ---------------------------------------------------------------------------

// Draw a line, clipping any pixel that falls outside the window so we never
// spam "not on visible screen area" or wrap negative coordinates.
void drawLine(const CanvasPoint &from, const CanvasPoint &to, const Colour &colour, DrawingWindow &window) {
	float xDiff = to.x - from.x;
	float yDiff = to.y - from.y;
	float numberOfSteps = std::max(std::abs(xDiff), std::abs(yDiff));
	uint32_t packed = colour.toUint32();
	if (numberOfSteps == 0.0f) {
		int x = static_cast<int>(std::round(from.x));
		int y = static_cast<int>(std::round(from.y));
		if (x >= 0 && x < int(window.width) && y >= 0 && y < int(window.height)) window.setPixelColour(x, y, packed);
		return;
	}
	float xStepSize = xDiff / numberOfSteps;
	float yStepSize = yDiff / numberOfSteps;
	for (float i = 0.0f; i <= numberOfSteps; i++) {
		int x = static_cast<int>(std::round(from.x + (xStepSize * i)));
		int y = static_cast<int>(std::round(from.y + (yStepSize * i)));
		if (x >= 0 && x < int(window.width) && y >= 0 && y < int(window.height)) window.setPixelColour(x, y, packed);
	}
}

void drawStraightLines(DrawingWindow &window) {
	CanvasPoint topLeft(0, 0);
	CanvasPoint center(window.width / 2, window.height / 2);
	CanvasPoint middle(window.width / 2, 0);
	Colour red(255, 0, 0);
	Colour green(0, 255, 0);
	Colour blue(0, 0, 255);
	drawLine(topLeft, center, red, window);
	drawLine(middle, CanvasPoint(middle.x, window.height), green, window);
	drawLine(CanvasPoint(window.width / 3, window.height / 2), CanvasPoint((2 * window.width) / 3, window.height / 2),
	         blue, window);
}

void drawCanvasStrokedTriangle(CanvasTriangle triangle, const Colour &colour, DrawingWindow &window) {
	drawLine(triangle[0], triangle[1], colour, window);
	drawLine(triangle[1], triangle[2], colour, window);
	drawLine(triangle[2], triangle[0], colour, window);
}

void drawRandomTriangle(DrawingWindow &window) {
	CanvasPoint v1(randInt(window.width), randInt(window.height));
	CanvasPoint v2(randInt(window.width), randInt(window.height));
	CanvasPoint v3(randInt(window.width), randInt(window.height));
	Colour colour(randInt(256), randInt(256), randInt(256));
	drawCanvasStrokedTriangle(CanvasTriangle(v1, v2, v3), colour, window);
}

// Flat fill via the classic top/bottom split. Guards degenerate (zero-height)
// spans so we never divide by zero, and no longer overdraws a debug outline.
void drawFilledTriangle(CanvasTriangle triangle, const Colour &colour, DrawingWindow &window) {
	std::vector<CanvasPoint> sortedPoints = {triangle.v0(), triangle.v1(), triangle.v2()};
	std::sort(sortedPoints.begin(), sortedPoints.end(),
	          [](const CanvasPoint &a, const CanvasPoint &b) { return a.y < b.y; });
	CanvasPoint top = sortedPoints[0];
	CanvasPoint middle = sortedPoints[1];
	CanvasPoint bottom = sortedPoints[2];

	// Slope of the long edge (top -> bottom), used for the right-hand x on both halves.
	float longStep = (bottom.y != top.y) ? (bottom.x - top.x) / (bottom.y - top.y) : 0.0f;

	// Top half: top -> middle.
	if (middle.y > top.y) {
		float leftStep = (middle.x - top.x) / (middle.y - top.y);
		for (int y = static_cast<int>(std::ceil(top.y)); y <= static_cast<int>(std::floor(middle.y)); y++) {
			int startX = static_cast<int>(std::round(top.x + leftStep * (y - top.y)));
			int endX = static_cast<int>(std::round(top.x + longStep * (y - top.y)));
			drawLine(CanvasPoint(startX, y), CanvasPoint(endX, y), colour, window);
		}
	}

	// Bottom half: middle -> bottom.
	if (bottom.y > middle.y) {
		float leftStep = (bottom.x - middle.x) / (bottom.y - middle.y);
		for (int y = static_cast<int>(std::ceil(middle.y)); y <= static_cast<int>(std::floor(bottom.y)); y++) {
			int startX = static_cast<int>(std::round(middle.x + leftStep * (y - middle.y)));
			int endX = static_cast<int>(std::round(top.x + longStep * (y - top.y)));
			drawLine(CanvasPoint(startX, y), CanvasPoint(endX, y), colour, window);
		}
	}
}

void drawRandomFilledTriangle(DrawingWindow &window) {
	Colour colour(randInt(256), randInt(256), randInt(256));
	CanvasPoint v1(randInt(window.width), randInt(window.height));
	CanvasPoint v2(randInt(window.width), randInt(window.height));
	CanvasPoint v3(randInt(window.width), randInt(window.height));
	CanvasTriangle triangle(v1, v2, v3);
	drawFilledTriangle(triangle, colour, window);
	// A white outline makes it easy to confirm the fill reaches the true edges.
	drawCanvasStrokedTriangle(triangle, Colour(255, 255, 255), window);
}

// ---------------------------------------------------------------------------
// Texture mapping
// ---------------------------------------------------------------------------

void drawTexturedLine(DrawingWindow &window, CanvasPoint from, CanvasPoint to, TextureMap texture) {
	float xDiff = to.x - from.x;
	float yDiff = to.y - from.y;
	float textureXDiff = to.texturePoint.x - from.texturePoint.x;
	float textureYDiff = to.texturePoint.y - from.texturePoint.y;
	float numberOfSteps = std::max(std::abs(xDiff), std::abs(yDiff));
	if (numberOfSteps == 0.0f) return;
	float xStepSize = xDiff / numberOfSteps;
	float yStepSize = yDiff / numberOfSteps;
	float textureXstepSize = textureXDiff / numberOfSteps;
	float textureYstepSize = textureYDiff / numberOfSteps;
	for (float i = 0.0f; i <= numberOfSteps; i++) {
		float x = from.x + (xStepSize * i);
		float y = from.y + (yStepSize * i);
		float textureX = from.texturePoint.x + (textureXstepSize * i);
		float textureY = from.texturePoint.y + (textureYstepSize * i);
		textureX = std::max(0.0f, std::min(textureX, static_cast<float>(texture.width - 1)));
		textureY = std::max(0.0f, std::min(textureY, static_cast<float>(texture.height - 1)));
		uint32_t color = texture.pixels[static_cast<int>(textureX) + (texture.width * static_cast<int>(textureY))];
		int px = static_cast<int>(std::round(x));
		int py = static_cast<int>(std::round(y));
		if (px >= 0 && px < int(window.width) && py >= 0 && py < int(window.height))
			window.setPixelColour(px, py, color);
	}
}

void drawTexturedTriangle(DrawingWindow &window, CanvasTriangle triangle, TextureMap &map) {
	if ((triangle.v0().y) > (triangle.v1()).y) std::swap(triangle.vertices[0], triangle.vertices[1]);
	if ((triangle.v0().y) > (triangle.v2()).y) std::swap(triangle.vertices[0], triangle.vertices[2]);
	if ((triangle.v1().y) > (triangle.v2()).y) std::swap(triangle.vertices[1], triangle.vertices[2]);
	float xDiff = (triangle.v2().x) - (triangle.v0().x);
	float yDiff = (triangle.v2().y) - (triangle.v0().y);
	float ratio = (yDiff != 0.0f) ? xDiff / yDiff : 0.0f;
	float x = (triangle.v1().y - triangle.v0().y) * ratio;
	CanvasPoint subPoint = CanvasPoint((x + triangle.v0().x), triangle.v1().y);
	int firstDiff = static_cast<int>(triangle.v1().y - triangle.v0().y);
	int secondDiff = static_cast<int>(triangle.v2().y - triangle.v1().y);
	std::vector<float> sideA1 = interpolateSingleFloats(triangle.v0().x, subPoint.x, firstDiff);
	std::vector<float> sideA2 = interpolateSingleFloats(triangle.v0().x, triangle.v1().x, firstDiff);
	std::vector<float> sideB1 = interpolateSingleFloats(subPoint.x, triangle.v2().x, secondDiff);
	std::vector<float> sideB2 = interpolateSingleFloats(triangle.v1().x, triangle.v2().x, secondDiff);
	float subPointX = interpolation(subPoint.x, triangle.v2().x, triangle.v0().x, triangle.v2().texturePoint.x,
	                                triangle.v0().texturePoint.x);
	float subPointY = interpolation(subPoint.y, triangle.v2().y, triangle.v0().y, triangle.v2().texturePoint.y,
	                                triangle.v0().texturePoint.y);
	for (int i = 0; i < firstDiff; i++) {
		CanvasPoint point1(sideA1[i], i + triangle.v0().y);
		point1.texturePoint = TexturePoint(
		        interpolation(sideA1[i], subPoint.x, triangle.v0().x, subPointX, triangle.v0().texturePoint.x),
		        interpolation(i + triangle.v0().y, subPoint.y, triangle.v0().y, subPointY,
		                      triangle.v0().texturePoint.y));
		CanvasPoint point2(sideA2[i], i + triangle.v0().y);
		point2.texturePoint = TexturePoint(
		        interpolation(sideA2[i], triangle.v0().x, triangle.v1().x, triangle.v0().texturePoint.x,
		                      triangle.v1().texturePoint.x),
		        interpolation(i + triangle.v0().y, triangle.v0().y, triangle.v1().y, triangle.v0().texturePoint.y,
		                      triangle.v1().texturePoint.y));
		drawTexturedLine(window, point1, point2, map);
	}
	for (int i = 0; i < secondDiff; i++) {
		CanvasPoint point1(sideB1[i], i + triangle.v1().y);
		point1.texturePoint = TexturePoint(
		        interpolation(sideB1[i], subPoint.x, triangle.v2().x, subPointX, triangle.v2().texturePoint.x),
		        interpolation(i + triangle.v1().y, subPoint.y, triangle.v2().y, subPointY,
		                      triangle.v2().texturePoint.y));
		CanvasPoint point2(sideB2[i], i + triangle.v1().y);
		point2.texturePoint = TexturePoint(
		        interpolation(sideB2[i], triangle.v2().x, triangle.v1().x, triangle.v2().texturePoint.x,
		                      triangle.v1().texturePoint.x),
		        interpolation(i + triangle.v1().y, triangle.v2().y, triangle.v1().y, triangle.v2().texturePoint.y,
		                      triangle.v1().texturePoint.y));
		drawTexturedLine(window, point1, point2, map);
	}
	drawCanvasStrokedTriangle(triangle, Colour(255, 255, 255), window);
}

void drawTexturedTriangleExample(DrawingWindow &window) {
	TextureMap textureMap("assets/texture.ppm");
	CanvasPoint v0(160, 10);
	v0.texturePoint = TexturePoint(195, 5);
	CanvasPoint v1(300, 230);
	v1.texturePoint = TexturePoint(395, 380);
	CanvasPoint v2(10, 150);
	v2.texturePoint = TexturePoint(65, 330);
	CanvasTriangle triangle(v0, v1, v2);
	drawTexturedTriangle(window, triangle, textureMap);
}

// ---------------------------------------------------------------------------
// OBJ / MTL loading
// ---------------------------------------------------------------------------

// Parse a Wavefront .mtl file into a name -> diffuse-colour map.
std::map<std::string, Colour> loadMTL(const std::string &filename) {
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
			        Colour(currentName, static_cast<int>(r * 255), static_cast<int>(g * 255),
			               static_cast<int>(b * 255));
		}
	}
	return materials;
}

// Parse a Wavefront .obj file. Resolves its own `mtllib` reference (relative to
// the .obj) and applies `usemtl` colours to the faces that follow.
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
	if (slash != std::string::npos) directory = filename.substr(0, slash + 1);

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
			if (it != materials.end()) currentColour = it->second;
		} else if (type == "v") {
			float x, y, z;
			ss >> x >> y >> z;
			vertices.push_back(glm::vec3(x * scale, y * scale, z * scale));
		} else if (type == "f") {
			std::string v1, v2, v3;
			ss >> v1 >> v2 >> v3;
			// Face tokens look like "2/", "2/3/", or "2/3/4" — stoi stops at the slash.
			int index1 = std::stoi(v1) - 1;
			int index2 = std::stoi(v2) - 1;
			int index3 = std::stoi(v3) - 1;
			triangles.push_back(
			        ModelTriangle(vertices[index1], vertices[index2], vertices[index3], currentColour));
		}
	}
	return triangles;
}

// ---------------------------------------------------------------------------
// Main render loop
// ---------------------------------------------------------------------------

void draw(DrawingWindow &window) {
	window.clearPixels();
	// The Cornell box is loaded once (function-local static) rather than every frame.
	static std::vector<ModelTriangle> triangles = loadOBJ("assets/cornell-box.obj", 0.35f);
	static bool logged = false;
	if (!logged) {
		std::cout << "Loaded " << triangles.size() << " triangles from cornell-box.obj" << std::endl;
		for (const auto &triangle : triangles) {
			std::cout << triangle.colour << " " << triangle;
		}
		logged = true;
	}
	// TODO(feature roadmap §6): project + rasterise `triangles` here.
}

void handleEvent(SDL_Event event, DrawingWindow &window) {
	if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
		if (event.key.keysym.sym == SDLK_LEFT)
			std::cout << "LEFT" << std::endl;
		else if (event.key.keysym.sym == SDLK_RIGHT)
			std::cout << "RIGHT" << std::endl;
		else if (event.key.keysym.sym == SDLK_UP)
			std::cout << "UP" << std::endl;
		else if (event.key.keysym.sym == SDLK_DOWN)
			std::cout << "DOWN" << std::endl;
		else if (event.key.keysym.sym == SDLK_u)
			drawRandomTriangle(window);
		else if (event.key.keysym.sym == SDLK_f)
			drawRandomFilledTriangle(window);
	} else if (event.type == SDL_MOUSEBUTTONDOWN) {
		window.savePPM("output.ppm");
		window.saveBMP("output.bmp");
	}
}

int main(int argc, char *argv[]) {
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;
	while (true) {
		// We MUST poll for events - otherwise the window will freeze !
		if (window.pollForInputEvents(event)) handleEvent(event, window);
		draw(window);
		// Need to render the frame at the end, or nothing actually gets shown on the screen!
		window.renderFrame();
	}
	return 0;
}
