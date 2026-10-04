#if defined(__linux__)
#include "weatherpaper/notify/notify.hpp"

#include <unistd.h>
#include <sys/wait.h>

#include <vector>

namespace weatherpaper::notify {

void LinuxNotifySendNotifier::show(const NotificationText& text) noexcept {
    // SECURITY (Section 5): argv-only exec, exactly like platform_linux's
    // process_runner.cpp - the title/body strings (which may contain a
    // user's configured location display name) are passed as single,
    // literal argv elements, never interpolated into a shell string.
    //
    // NON-BLOCKING (Section 3.13): fork, exec, and do NOT waitpid - a
    // notification daemon replying slowly (or not existing at all, in
    // which case execvp simply fails in the child) must never stall the
    // caller, which is mid-way through the wallpaper-update pipeline.
    pid_t pid = fork();
    if (pid < 0) return; // fork failed - silently give up, per file header contract
    if (pid == 0) {
        std::vector<char*> argv;
        // const_cast is safe here: execvp does not modify argv contents in
        // practice and the child process image is replaced immediately
        // after, so there is no lifetime concern with pointing at these
        // std::string buffers' internal storage.
        static std::string prog = "notify-send";
        static std::string app_name_flag = "--app-name=WeatherPaper";
        argv.push_back(prog.data());
        argv.push_back(app_name_flag.data());
        argv.push_back(const_cast<char*>(text.title.c_str()));
        argv.push_back(const_cast<char*>(text.body.c_str()));
        argv.push_back(nullptr);
        // Double-fork so the grandchild (actual notify-send process) is
        // reparented to init and never becomes our zombie - the same
        // pattern platform_linux/process_runner.cpp uses for detached
        // long-running processes, applied here even though notify-send
        // itself is short-lived, purely so the parent never needs to
        // wait() on it at all.
        if (fork() == 0) {
            execvp("notify-send", argv.data());
            _exit(127); // notify-send not installed - benign, no notification shown
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0); // reap only the instantly-exiting intermediate child
}

} // namespace weatherpaper::notify
#endif // __linux__
