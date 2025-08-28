# Welcome

## Linux

### Getting clangd happy
The Cmake command needs to explicitly mention outputting the `compile_commands.json` file. Like so:

```sh
cmake -G Ninja -S . -B build \
       -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_PREFIX_PATH="$VULKAN_SDK/lib/cmake" \
       -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Once the `compile_commands.json` file is made, there are additional commands in it that need deleted from the "command" field.
1. -fmodules-ts
2. I don't remember, I moved on too quick. clangd will show the error at the top, remove the commands it doesn't know

### Building
Run the above cmake command from the project root, then follow up with:

```sh
cd build
ninja
```


## Windows

Create a `CMakeUserPresets.json` file, this points to the vcpkg location. Here is mine:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-msvc-user",
      "inherits": "windows-msvc",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "C:/Users/marct/ghq/github.com/microsoft/vcpkg/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ]
}
```

You will also need to follow the setup directions for libs given by the Khronos Vulkan Tutorial (here)[https://docs.vulkan.org/tutorial/latest/02_Development_environment.html#_dependency_install_scripts]

Once setup run:
```sh
cmake --preset windows-msvc-user
```

1. Open `./build/HelloVulkan.sln` in Visual Studio 2022
2. Right-Click on HelloVulkan in the Solution Explorer and click `Build`
