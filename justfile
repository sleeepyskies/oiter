set default-list

_configure type:
    conan install .                                     \
        --output-folder=build                           \
        --build=missing                                 \
        -s build_type={{ capitalize(type) }}            \
        -c tools.cmake.cmaketoolchain:generator=Ninja

    cmake --preset conan-{{ lowercase(type) }}

_build type:
    cmake --build --preset conan-{{ lowercase(type) }}

# Downloads dependencies and configures Oiter and 2iREN in Release mode.
configure: (_configure "release")

# Builds Oiter and 2iREN in Release mode.
build: (_build "release")

# Downloads dependencies and configures Oiter and 2iREN in Debug mode.
configure-debug: (_configure "debug")

# Builds Oiter and 2iREN in Debug mode.
build-debug: (_build "debug")

# Starts the Oiter interactive mode.
interactive *args: build
    ./build/build/Release/oiter interactive {{ args }}

# Runs the Oiter render mode.
render *args: build
    ./build/build/Release/oiter render {{ args }}

# Runs the flip metric.
flip: build
    scripts/flip.py
