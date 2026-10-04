#include "weatherpaper/platform_linux/process_runner.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <cstring>
#include <vector>

namespace weatherpaper::platform_linux {

ProcessResult RealProcessRunner::run(const WallpaperCommand& cmd) {
    ProcessResult result;
    if (!cmd.is_supported || cmd.program.empty()) {
        result.spawn_failed = true;
        return result;
    }

    // Build a C-style argv array. execvp() resolves `program` against PATH
    // itself (no shell involved) - each element is passed to the new
    // process as a single, non-reinterpreted argument, which is exactly
    // what makes this safe against injection via file paths (Section 5):
    // a path like "foo; rm -rf ~" is delivered to the child as one literal
    // argv string, never parsed by any shell.
    std::vector<std::string> argv_storage;
    argv_storage.push_back(cmd.program);
    for (const auto& a : cmd.args) argv_storage.push_back(a);

    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 1);
    for (auto& s : argv_storage) argv.push_back(s.data());
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        result.spawn_failed = true;
        return result;
    }
    if (pid == 0) {
        // Child: replace this process image. Never returns on success.
        execvp(argv[0], argv.data());
        _exit(127); // execvp failed (e.g. program not found on PATH)
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        result.spawn_failed = true;
        return result;
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.spawn_failed = true;
    }
    return result;
}

bool RealProcessRunner::run_detached(const WallpaperCommand& cmd) {
    if (!cmd.is_supported || cmd.program.empty()) return false;

    std::vector<std::string> argv_storage;
    argv_storage.push_back(cmd.program);
    for (const auto& a : cmd.args) argv_storage.push_back(a);
    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 1);
    for (auto& s : argv_storage) argv.push_back(s.data());
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // Double-fork so the long-running background process (swaybg/
        // mpvpaper) is reparented to init and doesn't become a zombie our
        // own process would need to reap - we deliberately never wait() on
        // it, since it's meant to keep running for as long as it's the
        // active wallpaper.
        if (fork() == 0) {
            execvp(argv[0], argv.data());
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0); // reap only the intermediate child, instantly
    return true;
}

void RealProcessRunner::terminate_by_program_name(const std::string& program_name) {
    // Best-effort cleanup of a previously-launched long-running background
    // wallpaper process (swaybg/mpvpaper have no "replace image" IPC).
    // Uses `pkill -x <name>` (argv-based, not a shell string) rather than
    // parsing /proc ourselves - pkill is present on essentially every
    // Linux distribution WeatherPaper targets.
    WallpaperCommand cmd{"pkill", {"-x", program_name}};
    (void)run(cmd); // exit code 1 ("no process matched") is an expected, benign outcome
}

} // namespace weatherpaper::platform_linux
