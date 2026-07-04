#include "Drawing.h"
#include "ObjLoader.h"
#include <DrawingWindow.h>
#include <ModelTriangle.h>
#include <iostream>
#include <vector>

#define WIDTH 320
#define HEIGHT 240

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
	if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
		if (event.key.key == SDLK_LEFT)
			std::cout << "LEFT" << std::endl;
		else if (event.key.key == SDLK_RIGHT)
			std::cout << "RIGHT" << std::endl;
		else if (event.key.key == SDLK_UP)
			std::cout << "UP" << std::endl;
		else if (event.key.key == SDLK_DOWN)
			std::cout << "DOWN" << std::endl;
		else if (event.key.key == SDLK_U)
			drawRandomTriangle(window);
		else if (event.key.key == SDLK_F)
			drawRandomFilledTriangle(window);
	} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		window.savePPM("output.ppm");
		window.saveBMP("output.bmp");
	}
}

int main(int argc, char *argv[]) {
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;
	while (true) {
		// We MUST poll for events - otherwise the window will freeze !
		if (window.pollForInputEvents(event))
			handleEvent(event, window);
		draw(window);
		// Need to render the frame at the end, or nothing actually gets shown on the screen!
		window.renderFrame();
	}
	return 0;
}
