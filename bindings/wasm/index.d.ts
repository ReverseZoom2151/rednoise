// TypeScript definitions for the RedNoise WebAssembly renderer.

export type RenderMode = "wireframe" | "rasterised" | "rasterized" | "raytraced" | "pathtraced";

export interface RenderOptions {
	/** The OBJ file contents as a string. Required. */
	obj: string;
	/** Which renderer to run. Defaults to "raytraced". */
	mode?: RenderMode;
	/** Output width in pixels. Defaults to 640. */
	width?: number;
	/** Output height in pixels. Defaults to 480. */
	height?: number;
	/** Per-pixel sample count for the path tracer. Defaults to 64. */
	samples?: number;
	/** Camera z position; the camera looks at the origin. Defaults to 4.0. */
	camZ?: number;
}

export interface Renderer {
	/**
	 * Render an OBJ scene and return the pixels as RGBA8.
	 * The result is a Uint8ClampedArray of width*height*4 bytes, suitable for
	 * `new ImageData(rgba, width, height)` in the browser.
	 */
	render(options: RenderOptions): Uint8ClampedArray;
	/** Library version string, e.g. "0.1.0". */
	version(): string;
}

/** Instantiate the WebAssembly module and return a ready-to-use renderer. */
export function createRenderer(): Promise<Renderer>;

export default createRenderer;
