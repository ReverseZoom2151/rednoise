# Third-Party Notices

This project bundles third-party components. They are vendored under
`third_party/` and retain their original licenses.

## OpenGL Mathematics (GLM) 1.0.1

- Location: `third_party/glm/`
- Homepage: https://github.com/g-truc/glm
- License: The Happy Bunny License (Modified MIT) / MIT License — see
  `third_party/glm/copying.txt`
- Copyright (c) 2005 – G-Truc Creation

GLM is a header-only mathematics library for graphics software. Only the
`glm/` header tree and its license are vendored here; the upstream
`cmake/`, `doc/`, `test/`, `util/` directories and the C++20 module
(`glm.cppm`) are intentionally omitted.

## "sdw" teaching framework

- Location: `framework/`
- Origin: University of Bristol, Computer Graphics unit (COMS30020)

The `DrawingWindow`, `CanvasPoint`, `CanvasTriangle`, `Colour`,
`ModelTriangle`, `TextureMap`, `TexturePoint`, and `Utils` classes are
based on the framework distributed with the unit for building a software
renderer on top of SDL2.

## SDL2

Linked at build time (not vendored). https://www.libsdl.org — zlib license.
