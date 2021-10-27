# STM32CubeMX Empty Makefile Projects

This repository provides empty Makefile-based projects created with STM32CubeMX.

To prevent code duplication, the STM32CubeFx material included by STM32CubeMX has been removed from the generated code and made available as a Git submodule. Makefiles have been changed accordingly.

Unless you plan to use earlier versions of the STM32CubeFx material, you can get a shallow clone of the submodule itself with:

```
git clone https://github.com/luigidifraia/STM32CubeMX_Empty_Makefile_Projects.git --recursive --shallow-submodules
```

## Clock configuration

For a quick glance at the clock configuration used to generate projects, please refer to the media folder.

As an example, here's the clock configuration for STM32F411CEUx-based projects:

![Clock configuration for STM32F411CEUx-based projects](media/STM32F411CEUx_Clock_Configuration.png)
