//
// Created by dc on 10/10/18.
//

#ifndef SUIL_PROCESS_HPP
#define SUIL_PROCESS_HPP

#include <suil/sys.hpp>

namespace suil {

    define_log_tag(PROCESS);

    struct Process : LOGGER(dtag(PROCESS)) {

        using __ReadCallback = std::function<bool(zcstring&&)>;
        struct ReadCallbacks{
            __ReadCallback onStdOutput{nullptr};
            __ReadCallback onStdError{nullptr};
            int            fdOutput{-1};
            int            fdError{-1};
        };

        sptr(Process);

    public:
        template <typename... Args>
        static Ptr launch(const char *cmd, Args... args) {
            // execute with empty environment
            return Process::launch(zmap<zcstring>{}, cmd, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static Ptr launch(zmap<zcstring> env, const char *cmd, Args... args) {
            // reserve buffer for process arguments
            size_t argc  = sizeof...(args);
            char *argv[argc+2];
            // pack parameters into a buffer
            argv[0] = (char *) cmd;
            int index{1};
            Process::pack(index, argv, std::forward<Args>(args)...);
            return Process::start(env, cmd, argc, argv);
        }

        template <typename ...Args>
        static Process::Ptr bash(zmap<zcstring> env, const char *cmd, Args... args) {
            zbuffer tmp{256};
            Process::strfmt(tmp, cmd, std::forward<Args>(args)...);
            zcstring cmdStr(tmp);
            return Process::launch(env, "bash", "-c", cmdStr());
        }

        template <typename ...Args>
        inline static Process::Ptr bash(const char *cmd, Args... args) {
            zbuffer tmp{256};
            Process::strfmt(tmp, cmd, std::forward<Args>(args)...);
            zcstring cmdStr(tmp);
            return Process::launch("bash", "-c", cmdStr());
        }

        bool isExited();

        void terminate();

        void waitExit(int timeout = -1);

        zcstring getStdOutput();

        zcstring getStdError();

        template <typename... Callbacks>
        inline void readAsync(Callbacks&&... callbacks) {
            // cancel pending asynchronous read
            Ego.cancelAsyncRead();
            // configure the callbacks
            utils::apply_config(Ego.readCallbacks, std::forward<Callbacks>(callbacks)...);
            if (Ego.readCallbacks.onStdOutput)
                Ego.readCallbacks.fdOutput = dup(Ego.stdOut);
            if (Ego.readCallbacks.onStdError)
                Ego.readCallbacks.fdError = dup(Ego.stdErr);
            startReadAsync();
        }

        void cancelAsyncRead();

        ~Process() {
            terminate();
        }

    private:

        static void strfmt(zbuffer&) {}

        template <typename Arg>
        static void strfmt(zbuffer& out, Arg arg) { out << arg << " "; }

        template <typename Arg, typename... Args>
        static void strfmt(zbuffer& out, Arg arg, Args... args) {
            out << arg << " ";
            Process::strfmt(out, std::forward<Arg>(args)...);
        }

        static void pack(int& index, char* argv[]) { argv[index] = (char *) NULL; }

        template <typename Arg>
        static void pack(int& index, char* argv[], Arg arg) {
            argv[index++] = arg;
            argv[index++] = (char *) NULL;
        }

        template <typename Arg, typename... Args>
        static void pack(int& index, char* argv[], const Arg arg, Args... args) {
            argv[index++] = arg;
            pack(index, argv, std::forward<Args>(args)...);
        }

        void stopIO();

        void startReadAsync();

        static Ptr start(zmap<zcstring>& env, const char* cmd, int argc, char* argv[]);

        friend void Process_sa_handler(int sig, siginfo_t *info, void *context);
        static void on_SIGCHLD(int sig, siginfo_t *info, void *context);
        static void on_TERMINATION(int sig, siginfo_t *info, void *context);

        static coroutine void processAsyncRead(Process& proc, int fd, Process::__ReadCallback& readCb);

        pid_t           pid{0};
        int             stdOut{0};
        int             stdIn{0};
        int             stdErr{0};
        int             notifChan[2];
        bool            waitingExit{false};
        int             pendingReads{0};
        ReadCallbacks readCallbacks{nullptr, nullptr, -1, -1};
    };

}
#endif //SUIL_PROCESS_HPP
