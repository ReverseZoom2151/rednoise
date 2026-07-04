#pragma once

#include <CanvasPoint.h>
#include <CanvasTriangle.h>
#include <Colour.h>
#include <DrawingWindow.h>
#include <TextureMap.h>
#include <cstdint>

// Pack 8-bit RGB into the window's 0xAARRGGBB pixel format (alpha forced opaque).
uint32_t packColour(int red, int green, int blue);

// Full-screen demos.
void redNoise(DrawingWindow &window);
void singleDimensionGreyscaleInterpolation(DrawingWindow &window);
void twoDimensionalColourInterpolation(DrawingWindow &window);

// Lines and triangles.
void drawLine(const CanvasPoint &from, const CanvasPoint &to, const Colour &colour, DrawingWindow &window);
void drawStraightLines(DrawingWindow &window);
void drawCanvasStrokedTriangle(CanvasTriangle triangle, const Colour &colour, DrawingWindow &window);
void drawRandomTriangle(DrawingWindow &window);
void drawFilledTriangle(CanvasTriangle triangle, const Colour &colour, DrawingWindow &window);
void drawRandomFilledTriangle(DrawingWindow &window);

// Texture mapping.
void drawTexturedLine(DrawingWindow &window, CanvasPoint from, CanvasPoint to, TextureMap texture);
void drawTexturedTriangle(DrawingWindow &window, CanvasTriangle triangle, TextureMap &map);
void drawTexturedTriangleExample(DrawingWindow &window);
