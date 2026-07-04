# vcpkg port for the RedNoise C++ renderer (library + stable C ABI).
#
# MAINTAINER: at each release you MUST fill in two values below:
#   REF     -> the git tag (or commit SHA) being packaged, e.g. "v0.1.0"
#   SHA512  -> the tarball hash for that REF. The easiest way to obtain it:
#              set REF, set SHA512 to "0", run `vcpkg install rednoise
#              --overlay-ports=packaging/vcpkg/ports`, and copy the "Actual
#              hash" that the failed download prints back into SHA512 here.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ReverseZoom2151/rednoise
    # TODO(maintainer): set REF to the release tag being packaged.
    REF "v0.1.0"
    # TODO(maintainer): replace with the real SHA512 of the REF tarball (see note above).
    SHA512 0
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_APP=OFF
        -DBUILD_GPU=OFF
        -DBUILD_HEADLESS=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_LIB=ON
        # Use vcpkg's glm instead of fetching it from upstream.
        -DFETCH_DEPENDENCIES=OFF
)

vcpkg_cmake_install()

# The project installs its package config to lib/cmake/rednoise; move it to the
# vcpkg-standard share/rednoise location and fix up the paths.
vcpkg_cmake_config_fixup(PACKAGE_NAME rednoise CONFIG_PATH lib/cmake/rednoise)

# Headers only need to ship once, from the release tree.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# Install the license (satisfies vcpkg's copyright requirement).
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
