#include "Canvas.h"

#include <algorithm>
#include <array>
#include <fstream>

Canvas::Canvas(size_t w, size_t h) : width(w), height(h), pixels(w * h, 0) {}

void Canvas::setPixelColour(size_t x, size_t y, uint32_t colour) {
	if (x < width && y < height)
		pixels[(y * width) + x] = colour;
}

uint32_t Canvas::getPixelColour(size_t x, size_t y) const {
	if (x < width && y < height)
		return pixels[(y * width) + x];
	return 0;
}

void Canvas::clearPixels() {
	std::fill(pixels.begin(), pixels.end(), 0u);
}

void Canvas::savePPM(const std::string &filename) const {
	std::ofstream outputStream(filename, std::ofstream::out | std::ofstream::binary);
	outputStream << "P6\n";
	outputStream << width << " " << height << "\n";
	outputStream << "255\n";
	for (size_t i = 0; i < width * height; i++) {
		std::array<char, 3> rgb{{
		    static_cast<char>((pixels[i] >> 16) & 0xFF),
		    static_cast<char>((pixels[i] >> 8) & 0xFF),
		    static_cast<char>((pixels[i] >> 0) & 0xFF),
		}};
		outputStream.write(rgb.data(), 3);
	}
	outputStream.close();
}
