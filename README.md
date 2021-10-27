# STM32CubeMX Empty Makefile Projects

This repository provides empty Makefile-based projects created with STM32CubeMX.

The STM32CubeFx material has been removed from the generated code, as it can be downloaded from STMicroelectronic's GitHub repositories. Es example, for STM32F4 device the URL to use is:

https://github.com/STMicroelectronics/STM32CubeF4

Because of the way STM32CubeMX generates code, as a self-contained directory that includes STM32CubeFx material in every project, users have to clone the relevant STM32CubeFx repo and then add project files found in this repository to it.

# TODO

Change the default project Makefiles so that this repository can use a Git submodule for STM32CubeFx material that's common to multiple projects.
