Reserved for a public header if platform_windows ever needs to expose
Windows-specific types beyond what platform_common::create_platform_*()
already returns. Currently empty: wallpaper_windows.cpp and
hooks_windows.cpp implement platform_common's interfaces directly with no
additional public surface of their own.
