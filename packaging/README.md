# Packaging RedNoise

This directory contains registry packaging for the RedNoise C++ renderer so that
downstream projects can consume it through a package manager instead of vendoring
the sources. Both ports build only the installable library and its stable C ABI
(header at `include/rednoise/rednoise.h`), with the application, GPU path tracer,
headless tools, and tests disabled. glm is provided by the package manager rather
than fetched from upstream.

After installation a consumer uses standard CMake:

```cmake
find_package(rednoise CONFIG REQUIRED)
target_link_libraries(app PRIVATE rednoise::rednoise)
```

The CMake package config name is `rednoise` and the imported target is
`rednoise::rednoise`.

## vcpkg (overlay port)

The overlay port lives in `packaging/vcpkg/ports/rednoise`. To install it into a
project or classic environment, point vcpkg at the overlay directory:

```sh
vcpkg install rednoise --overlay-ports=packaging/vcpkg/ports
```

In manifest mode, add `rednoise` to your `vcpkg.json` dependencies and pass the
same `--overlay-ports` flag (or set `VCPKG_OVERLAY_PORTS`) during install.

Maintainer note: before a release, edit `portfile.cmake` and fill in the two
placeholders:

- `REF`: the git tag or commit SHA being packaged (for example `v0.1.0`).
- `SHA512`: the SHA512 of the source tarball for that `REF`. A simple way to get
  it is to set `SHA512` to `0`, run the install command above, and copy the
  "Actual hash" reported by the failed download back into the portfile.

Until those values are filled in with a real published tag and hash, the port
will not download successfully.

## Conan 2.x

The Conan recipe lives in `packaging/conan/conanfile.py`. It builds RedNoise from
the repository sources and is meant to be run from the repository root:

```sh
conan create packaging/conan
```

This builds the library, runs the `test_package` (a small pure-C program that
links `rednoise::rednoise` and calls `rn_version()`), and places the package in
your local Conan cache. Consume it from another Conan project by adding a
`requires("rednoise/0.1.0")` and using the standard `CMakeToolchain` and
`CMakeDeps` generators, then the `find_package(rednoise CONFIG REQUIRED)` snippet
above.

The recipe requires `glm` from Conan Center and disables the in-tree dependency
fetching so the package manager owns the dependency graph.
