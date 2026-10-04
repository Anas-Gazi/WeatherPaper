This module is header-only (see ../CMakeLists.txt: it's an INTERFACE
library) - platform_common publishes only the shared IPlatformWallpaper /
IPlatformHooks contracts that platform_windows and platform_linux
implement. There is intentionally no .cpp file here.
