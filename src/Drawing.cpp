#include "Drawing.h"

#include "Interpolation.h"
#include <TexturePoint.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <random>
#include <vector>

uint32_t packColour(int red, int green, int blue) {
	return (255u << 24) | ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
}

// A single, well-seeded random source for the interactive demos.
static int randInt(int upperExclusive) {
	static std::mt19937 generator{std::random_device{}()};
	std::uniform_int_distribution<int> distribution(0, upperExclusive - 1);
	return distribution(generator);
}

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
			window.setPixelColour(x, y, packColour(int(rowColours[x].x), int(rowColours[x].y), int(rowColours[x].z)));
		}
	}
}

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
		if (x >= 0 && x < int(window.width) && y >= 0 && y < int(window.height))
			window.setPixelColour(x, y, packed);
		return;
	}
	float xStepSize = xDiff / numberOfSteps;
	float yStepSize = yDiff / numberOfSteps;
	for (float i = 0.0f; i <= numberOfSteps; i++) {
		int x = static_cast<int>(std::round(from.x + (xStepSize * i)));
		int y = static_cast<int>(std::round(from.y + (yStepSize * i)));
		if (x >= 0 && x < int(window.width) && y >= 0 && y < int(window.height))
			window.setPixelColour(x, y, packed);
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

void drawTexturedLine(DrawingWindow &window, CanvasPoint from, CanvasPoint to, TextureMap texture) {
	float xDiff = to.x - from.x;
	float yDiff = to.y - from.y;
	float textureXDiff = to.texturePoint.x - from.texturePoint.x;
	float textureYDiff = to.texturePoint.y - from.texturePoint.y;
	float numberOfSteps = std::max(std::abs(xDiff), std::abs(yDiff));
	if (numberOfSteps == 0.0f)
		return;
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
	if ((triangle.v0().y) > (triangle.v1()).y)
		std::swap(triangle.vertices[0], triangle.vertices[1]);
	if ((triangle.v0().y) > (triangle.v2()).y)
		std::swap(triangle.vertices[0], triangle.vertices[2]);
	if ((triangle.v1().y) > (triangle.v2()).y)
		std::swap(triangle.vertices[1], triangle.vertices[2]);
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
		    interpolation(i + triangle.v0().y, subPoint.y, triangle.v0().y, subPointY, triangle.v0().texturePoint.y));
		CanvasPoint point2(sideA2[i], i + triangle.v0().y);
		point2.texturePoint = TexturePoint(interpolation(sideA2[i], triangle.v0().x, triangle.v1().x,
		                                                 triangle.v0().texturePoint.x, triangle.v1().texturePoint.x),
		                                   interpolation(i + triangle.v0().y, triangle.v0().y, triangle.v1().y,
		                                                 triangle.v0().texturePoint.y, triangle.v1().texturePoint.y));
		drawTexturedLine(window, point1, point2, map);
	}
	for (int i = 0; i < secondDiff; i++) {
		CanvasPoint point1(sideB1[i], i + triangle.v1().y);
		point1.texturePoint = TexturePoint(
		    interpolation(sideB1[i], subPoint.x, triangle.v2().x, subPointX, triangle.v2().texturePoint.x),
		    interpolation(i + triangle.v1().y, subPoint.y, triangle.v2().y, subPointY, triangle.v2().texturePoint.y));
		CanvasPoint point2(sideB2[i], i + triangle.v1().y);
		point2.texturePoint = TexturePoint(interpolation(sideB2[i], triangle.v2().x, triangle.v1().x,
		                                                 triangle.v2().texturePoint.x, triangle.v1().texturePoint.x),
		                                   interpolation(i + triangle.v1().y, triangle.v2().y, triangle.v1().y,
		                                                 triangle.v2().texturePoint.y, triangle.v1().texturePoint.y));
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
