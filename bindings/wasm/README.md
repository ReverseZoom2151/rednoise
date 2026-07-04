# rednoise (WebAssembly)

A WebAssembly build of the RedNoise CPU software renderer. It compiles the
C++23 engine to WASM with Emscripten and exposes a small JavaScript API that
renders a Wavefront OBJ into an RGBA pixel buffer.

Four render modes are available: wireframe, rasterised, raytraced, and
pathtraced.

## Install

```
npm install rednoise
```

## Usage (Node)

```js
import { readFileSync } from "node:fs";
import { createRenderer } from "rednoise";

const obj = readFileSync("model.obj", "utf8");
const renderer = await createRenderer();

const width = 640;
const height = 480;
const rgba = renderer.render({ obj, mode: "raytraced", width, height });
// rgba is a Uint8ClampedArray of width * height * 4 bytes (RGBA).

console.log("rednoise version:", renderer.version());
```

## Usage (browser)

```js
import { createRenderer } from "rednoise";

const renderer = await createRenderer();
const rgba = renderer.render({ obj: objText, mode: "raytraced", width: 640, height: 480 });

const canvas = document.querySelector("canvas");
canvas.width = 640;
canvas.height = 480;
const ctx = canvas.getContext("2d");
ctx.putImageData(new ImageData(rgba, 640, 480), 0, 0);
```

### render options

| option    | default       | meaning                                                   |
| --------- | ------------- | --------------------------------------------------------- |
| `obj`     | (required)    | OBJ file contents as a string                             |
| `mode`    | `"raytraced"` | `wireframe`, `rasterised`, `raytraced`, or `pathtraced`   |
| `width`   | `640`         | output width in pixels                                    |
| `height`  | `480`         | output height in pixels                                   |
| `samples` | `64`          | per-pixel samples (path tracer only)                      |
| `camZ`    | `4.0`         | camera z position; the camera looks at the origin         |

## How OBJ loading works

The engine's `loadOBJ` reads a file by path. Under WebAssembly there is no host
filesystem, so the wrapper writes the OBJ text you pass into Emscripten's MEMFS
(an in-memory filesystem) and loads it from there. Because a companion `.mtl`
file is not written alongside it, materials are not resolved and faces use their
default colour, matching the headless native behaviour.

## Building locally

The `dist/` folder is produced by Emscripten and is not checked into git; it is
built in CI before publishing. To build it yourself you need the Emscripten SDK
(emsdk) installed and activated so that `emcc` is on your PATH:

```
bash build.sh
```

This produces `dist/rednoise.mjs` and `dist/rednoise.wasm`.

## License

MIT
