//
// Created by dc on 12/11/18.
//

#ifndef SUIL_FILE_H
#define SUIL_FILE_H

#include <fcntl.h>
#include <libmill/libmill.h>

#include <suil/zstring.h>
#include <suil/buffer.h>
#include <suil/logging.h>

namespace suil {

    /**
     * File provides asynchronous operations for writing
     * and reading from files. It's an abstraction on top of libmill's
     * file API's
     */
    struct File {
        /**
         * Creates a new file taking a lib mill file object
         * @param mf a lib mill file object
         */
        File(mfile mf);

        /**
         * creates a new file which operates on the given file path
         * see syscall open
         *
         * @param path the path of the file to work with
         * @param flags file open flags
         * @param mode file open mode
         */
        File(const char *path, int flags, mode_t mode);
        /**
         * creates a new file which operates on the given file descriptor
         * @param fd the file descriptor to work with
         * @param own if true, the created \class File object will take
         * ownership of the given descriptor and takes care of cleaning it
         * up
         */
        explicit File(int fd, bool own = false);

        File(File&) = delete;
        File&operator=(File&) = delete;
        /**
         * move constructor
         * @param f
         */
        File(File&& f)
                : fd(f.fd)
        {
            f.fd = nullptr;
        }

        /**
         * move assignment
         * @param f
         * @return
         */
        File& operator=(File&& f) {
            fd = f.fd;
            f.fd = nullptr;
            return Ego;
        }

        /**
         * writes the given data to the file
         *
         * @param data pointer to the data to write
         * @param size the size to write to file from data buffer
         * @param dd the deadline time for the write
         * @return number of bytes written to file
         */
        virtual size_t write(const void*data, size_t size, int64_t dd = -1);

        /**
         * read data from file
         * @param data the buffer to hold the read data.
         * @param size the size of the given output buffer \param data. after reading,
         * this reference variable will have the number of bytes read into buffer
         * @param dd the deadline time for the read
         * @return true for a successful read, false otherwise
         */
        virtual bool   read(void *data, size_t& size, int64_t dd = -1);
        /**
         * file seek to given offset
         * @param off the offset to seek to
         * @return the new position in file
         */
        virtual off_t  seek(off_t off);
        /**
         * get the offset on the file
         * @return the current offset on the file
         */
        virtual off_t  tell();
        /**
         * check if the offset is at the end of file
         * @return true if offset at end of file
         */
        virtual bool   eof();
        /**
         * flush the data to disk
         * @param dd timestamp to wait for until flush is completed
         */
        virtual void   flush(int64_t dd = -1);
        /**
         * closes the file descriptor if it's being owned and it's valid
         */
        virtual void   close();
        /**
         * checks if the given file descriptor is valid
         * @return true if the descriptor is valid, false otherwise
         */
        virtual bool   valid() {
            return fd != nullptr;
        }

        /**
         * get the file descriptor associated with the given file
         * @return
         */
        virtual int raw() {
            return mfget(fd);
        }

        /**
         * eq equality operator, check if two \class File objects
         * point to the same file descriptor
         * @param other
         * @return
         */
        bool operator==(const File& other) {
            return (this == &other)  ||
                   ( fd == other.fd);
        }

        /**
         * neq equality operator, check if two \class File objects
         * point to different file descriptor
         * @param other
         * @return
         */
        bool operator!=(const File& other) {
            return !(*this == other);
        }

        /**
         * stream operator - write a stringview into the file
         * @param sv the string view to write
         * @return
         */
        File& operator<<(strview& sv);

        /**
         * stream operator - write a \class String into the file
         * @param str the \class String to write
         * @return
         */
        File& operator<<(const String& str);

        /**
         * stream operator - write a \class OBuffer into the file
         * @param b the buffer to write to file
         * @return
         */
        File& operator<<(const OBuffer& b);

        /**
         * stream operator - write c-style string into file
         * @param str the string to write
         * @return
         */
        File& operator<<(const char* str);

        virtual ~File();

    protected:
        mfile           fd;
    };

    namespace utils::fs {

        inline String realpath(const char *path) {
            char base[PATH_MAX];
            if (::realpath(path, base) == nullptr) {
                if (errno != EACCES && errno != ENOENT)
                    return String();
            }

            return std::move(String{base}.dup());
        }

        inline size_t size(const char *path) {
            struct stat st;
            if (stat(path, &st) == 0) {
                return (size_t) st.st_size;
            }
            throw Exception::create("file '", path, "' does not exist");
        }

        inline void touch(const char *path, mode_t mode=0777) {
            if (::open(path, O_CREAT|O_TRUNC|O_WRONLY, mode) < 0) {
                throw Exception::create("touching file '", path, "' failed: ", errno_s);
            }
        }

        inline bool  exists(const char *path) {
            return access(path, F_OK) != -1;
        }

        inline bool isdir(const char *path) {
            struct stat st{};
            return (stat(path, &st) == 0) && (S_ISDIR(st.st_mode));
        }

        String currdir();

        bool isdirempty(const char *dir);

        String getname(const char *path);

        void mkdir(const char *path, bool recursive = false, mode_t mode = 0777);

        void mkdir(const char *base, const std::vector<const char*> paths, bool recursive = false, mode_t mode = 0777);

        inline void mkdir(const std::vector<const char*> paths, bool recursive = false, mode_t mode = 0777) {
            char base[PATH_MAX];
            getcwd(base, PATH_MAX);
            mkdir(base, std::move(paths), recursive, mode);
        }

        void remove(const char *path, bool recursive = false, bool contents = false);

        void remove(const char *base, const std::vector<const char*> paths, bool recursive = false);

        inline void remove(const std::vector<const char*>&& paths, bool recursive = false) {
            char base[PATH_MAX];
            getcwd(base, PATH_MAX);
            remove(base, std::move(paths), recursive);
        }

        void forall(const char *path, std::function<bool(const String&, bool)> h, bool recursive = false, bool pod = false);

        std::vector<String> ls(const char *path, bool recursive = false);

        void readall(OBuffer& out, const char *path, bool async = false);

        String readall(const char* path, bool async = false);

        void append(const char *path, const void *data, size_t sz, bool async = true);

        inline void append(const char *path, const OBuffer& b, bool async = true) {
            append(path, b.data(), b.size(), async);
        }

        inline void append(const char *path, const std::string& s, bool async = true) {
            append(path, s.data(), s.size(), async);
        }

        inline void append(const char *path, const strview& s, bool async = true) {
            append(path, s.data(), s.size(), async);
        }

        inline void append(const char *path, const String& s, bool async = true) {
            append(path, s.data(), s.size(), async);
        }

        inline void clear(const char *path) {
            if ((::truncate(path, 0) < 0) && errno != EEXIST) {
                throw Exception::create("clearing file '", path, "' failed: ", errno_s);
            }
        }

        template <typename __T>
        inline void append(const char* path, const __T d, bool async = true) {
            OBuffer b(15);
            b << d;
            append(path, b, async);
        }
    }

    struct FileLogger {

        FileLogger(const std::string dir, const std::string prefix);
        FileLogger()
                : dst(nullptr)
        {}

        virtual void log(const char *, size_t, log::Level);

        inline void close() {
            dst.close();
        }

        void open(const std::string& str, const std::string& prefix);

        inline ~FileLogger() {
            close();
        }

    private:
        File dst;
    };
}

#endif //SUIL_FILE_H
