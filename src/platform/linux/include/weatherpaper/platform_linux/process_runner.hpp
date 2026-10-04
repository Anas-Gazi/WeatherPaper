// weatherpaper/platform_linux/process_runner.hpp
//
// SECURITY (Section 5): this is the ONLY place in platform_linux that
// actually spawns a process. It takes a WallpaperCommand (program + argv,
// see backend.hpp) and executes it via fork()+execvp() - never via
// system()/popen()/sh -c, so there is no shell parsing of any argument at
// any point, regardless of what characters a file path or theme-pack name
// contains.
#pragma once

#include <optional>
#include <string>

#include "weatherpaper/platform_linux/backend.hpp"

namespace weatherpaper::platform_linux {

struct ProcessResult {
    bool spawn_failed = false; // true if fork/exec itself failed (e.g. program not on PATH)
    int exit_code = -1;        // valid only if !spawn_failed
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    // Runs a short-lived command to completion (gsettings/xfconf-query/
    // plasma-apply-wallpaperimage/feh all set state and exit on their own).
    virtual ProcessResult run(const WallpaperCommand& cmd) = 0;
    // Launches a long-running background process WITHOUT waiting for it to
    // exit (swaybg/mpvpaper run continuously to keep rendering the
    // background - Wayland has no persistent root-window concept like X11,
    // so these tools ARE the running wallpaper, not a one-shot setter).
    // Returns true if the fork+exec itself succeeded.
    virtual bool run_detached(const WallpaperCommand& cmd) = 0;
    // Best-effort: find and terminate a previously-launched long-running
    // background process by program name (used to replace a running
    // swaybg/mpvpaper instance, which have no "update image" IPC - see
    // backend.cpp's build_wayland_static comment).
    virtual void terminate_by_program_name(const std::string& program_name) = 0;
};

class RealProcessRunner : public IProcessRunner {
public:
    ProcessResult run(const WallpaperCommand& cmd) override;
    bool run_detached(const WallpaperCommand& cmd) override;
    void terminate_by_program_name(const std::string& program_name) override;
};

} // namespace weatherpaper::platform_linux
