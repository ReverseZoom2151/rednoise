// ES module wrapper around the Emscripten-built RedNoise renderer.
//
// Usage:
//     import { createRenderer } from "rednoise";
//     const renderer = await createRenderer();
//     const rgba = renderer.render({ obj: objText, mode: "raytraced", width: 640, height: 480 });
//
// `rgba` is a Uint8ClampedArray of width*height*4 bytes (RGBA), suitable for
// wrapping in an ImageData in the browser.

import createModule from "./dist/rednoise.mjs";

// mode string -> rn_render_mode enum int.
const MODES = {
	wireframe: 0,
	rasterised: 1,
	rasterized: 1,
	raytraced: 2,
	pathtraced: 3,
};

export async function createRenderer() {
	const Module = await createModule();

	// Bind the exported C functions once.
	const wasmRender = Module.cwrap("rn_wasm_render", "number", [
		"number", // objText (char* pointer)
		"number", // objLen
		"number", // mode
		"number", // width
		"number", // height
		"number", // samples
		"number", // camZ (float)
		"number", // outRgba (unsigned char* pointer)
	]);
	const wasmVersion = Module.cwrap("rn_wasm_version", "string", []);

	function render({ obj, mode = "raytraced", width = 640, height = 480, samples = 64, camZ = 4.0 } = {}) {
		if (typeof obj !== "string" || obj.length === 0) {
			throw new Error("render: `obj` must be a non-empty OBJ string");
		}
		const modeInt = MODES[mode];
		if (modeInt === undefined) {
			throw new Error(`render: unknown mode "${mode}" (expected wireframe|rasterised|raytraced|pathtraced)`);
		}
		if (!Number.isInteger(width) || !Number.isInteger(height) || width <= 0 || height <= 0) {
			throw new Error("render: width and height must be positive integers");
		}

		// Copy the OBJ text into WASM memory as UTF-8.
		const objLen = Module.lengthBytesUTF8(obj);
		const objPtr = Module._malloc(objLen + 1);
		// The +1 leaves room for the NUL terminator stringToUTF8 writes.
		Module.stringToUTF8(obj, objPtr, objLen + 1);

		// Allocate the output RGBA buffer.
		const outLen = width * height * 4;
		const outPtr = Module._malloc(outLen);

		try {
			const ok = wasmRender(objPtr, objLen, modeInt, width, height, samples, camZ, outPtr);
			if (!ok) {
				throw new Error("render: renderer returned failure (bad OBJ or invalid arguments)");
			}
			// Copy the pixels out of WASM memory before freeing.
			const out = new Uint8ClampedArray(outLen);
			out.set(Module.HEAPU8.subarray(outPtr, outPtr + outLen));
			return out;
		} finally {
			Module._free(objPtr);
			Module._free(outPtr);
		}
	}

	function version() {
		return wasmVersion();
	}

	return { render, version };
}

export default createRenderer;
