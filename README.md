# VulkanBot
A Discord Bot that renders GLSL shaders

## Installation

### Raspberry Pi

The Bot can be run from a Raspberry Pi 4B.

#### Requirements

- Raspberry Pi 4B
- Kernel with 64 bit support
- Debian sid arm64 schroot (``debootstrap --arch=arm64 sid /<path to your schroot>``)
- Vulkan driver installed *inside the schroot* (https://www.raspberrypi.org/forums/viewtopic.php?t=289168)

*Note: You probably won't need a schroot if you are using a 64 bit system already, but I only tried it using schroot.*

### Steps

1. Enter your schroot: ``schroot -c chroot:sid64``
2. Clone the repository recursively: ``git clone --recursive https://github.com/JnCrMx/VulkanBot``
3. Change into the repository: ``cd VulkanBot``
4. Edit ``external/sleepy-discord/include/sleepy_discord/server.h`` and change ``NotSet = '\xFE', //-2 in hex`` in line ``126`` to ``NotSet = -2,``
   (see https://github.com/yourWaifu/sleepy-discord/blob/ec255f7c879daed3de0df5cdedee17b4d558c927/include/sleepy_discord/server.h#L126)
5. Create a ``build/`` directory: ``mkdir build/``
6. Change into this directory: ``cd build/``
7. Create the build files with CMake: ``cmake ..``
8. Build: ``make -j4``
9. Copy (or link) ``vulkan_bot`` and ``shaders/`` into your prefered run directory.
10. Copy ``config.example.json`` from this repository to ``config.json`` in your run directory and customize it.
