# Third-Party Notices

This project bundles third-party components. They are vendored under
`third_party/` and retain their original licenses.

## OpenGL Mathematics (GLM) 0.9.7.2

- Location: `third_party/glm/`
- Homepage: https://github.com/g-truc/glm
- License: The Happy Bunny License (Modified MIT) / MIT License
- Copyright (c) 2005 – 2015 G-Truc Creation

GLM is a header-only mathematics library for graphics software. The
Visual Studio debugger visualizers under `third_party/glm/util/`
(`glm.natvis`, `autoexp.*`, `usertype.dat`) ship with GLM as debugging aids.

## "sdw" teaching framework

- Location: `framework/`
- Origin: University of Bristol, Computer Graphics unit (COMS30020)

The `DrawingWindow`, `CanvasPoint`, `CanvasTriangle`, `Colour`,
`ModelTriangle`, `TextureMap`, `TexturePoint`, and `Utils` classes are
based on the framework distributed with the unit for building a software
renderer on top of SDL2.

## SDL2

Linked at build time (not vendored). https://www.libsdl.org — zlib license.
