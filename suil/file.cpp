//
// Created by dc on 12/11/18.
//

#include <dirent.h>
#include <libmill/libmill.h>

#include "file.h"
#include "logging.h"
#include "utils.h"

namespace suil {

    File::File(mfile fd)
            : fd(fd)
    {}

    File::File(int fd, bool own)
            : fd(mfcreate(fd, own))
    {
        if (Ego.fd == nullptr) {
            throw Exception::invalidArguments(
                    "creating file for descriptor '", fd, "' failed: ", errno_s);
        }
    }

    File::File(const char *pname, int flags, mode_t mode)
            : File(mfopen(pname, flags, mode))
    {
        if (fd == nullptr) {
            throw Exception::invalidArguments(
                    "opening file '", pname, "' failed: ",errno_s);
        }
    }

    void File::close() {
        if (fd != nullptr) {
            flush(500);
            mfclose(fd);
            fd = nullptr;
        }
    }

    void File::flush(int64_t dd) {
        if (fd == nullptr) {
            // invalid call context
            throw Exception::accessViolation(
                    "File::flush - unsupported call context");
        }

        mfflush(fd, dd);
        if (errno) {
            printf("flushing failed: %s", errno_s);
        }
    }

    off_t File::seek(off_t off) {
        if (fd == nullptr) {
            // invalid call context
            throw Exception::accessViolation(
                    "File::seek - unsupported call context");
        }
        return mfseek(fd, off);
    }

    bool File::eof() {
        if (fd == nullptr) {
            // invalid call context
            throw Exception::accessViolation(
                    "File::eof - unsupported call context");
        }
        return mfeof(fd) != 0;
    }

    off_t File::tell() {
        if (fd == nullptr) {
            // invalid call context
            throw Exception::accessViolation(
                    "File::tell - unsupported call context");
        }
        return mftell(fd);
    }

    bool File::read(void *buf, size_t &len, int64_t dd) {
        bool status{true};
        if (fd == nullptr) {
            // invalid call context
            throw Exception::accessViolation(
                    "File::read - unsupported call context");
        }
        size_t ret = mfread(fd, buf, len, dd);
        if (errno) {
            strace("%p: File read error: %s", fd, errno_s);
            status = false;
        }
        len = ret;
        return status;
    }


    size_t File::write(const void *buf, size_t len, int64_t dd) {
        if (fd == nullptr) {
            // invalid call context
            throw Exception::accessViolation(
                    "File::write - unsupported call context");
        }
        size_t ret = mfwrite(fd, buf, len, dd);
        if (errno) {
            strace("%p: File write error: %s", fd, errno_s);
        }
        return ret;
    }

    File& File::operator<<(const char* str) {
        if (str) {
            size_t len = strlen(str);
            size_t nwr = write(str, len, -1);
            if (nwr != len) {
                throw Exception::create(
                        "Writing failed to file failed");
            }
        }
        return *this;
    }

    File& File::operator<<(const OBuffer& b) {
        size_t nwr = write(b.data(), b.size(), -1);
        if (nwr != b.size()) {
            throw Exception::create(
                    "Writing failed to file failed");
        }
        return *this;
    }

    File& File::operator<<(const String& str) {
        size_t nwr = write(str.data(), str.size(), -1);
        if (nwr != str.size()) {
            throw Exception::create(
                    "Writing failed to file failed");
        }
        return *this;
    }

    File& File::operator<<(strview& sv) {
        size_t nwr = write(sv.data(), sv.size(), -1);
        if (nwr != sv.size()) {
            throw Exception::create(
                    "Writing failed to file failed");
        }
        return *this;
    }

    File::~File() {
        close();
    }

    FileLogger::FileLogger(const std::string dir, const std::string prefix)
            : dst(nullptr)
    {
        open(dir, prefix);
    }

    void FileLogger::open(const std::string &dir, const std::string& prefix) {
        if (!utils::fs::exists(dir.c_str())) {
            sdebug("creating file logger directory: %s", dir.c_str());
            utils::fs::mkdir(dir.c_str(), true, 0777);
        }
        String tmp{utils::catstr(dir, "/", prefix, "-", Datetime()("%Y%m%d_%H%M%S"), ".log")};
        dst = std::move(File(tmp.data(), O_WRONLY|O_APPEND|O_CREAT, 0666));
    }

    void FileLogger::log(const char *data, size_t sz, log::Level) {
        dst.write(data, sz, 1500);
        dst.flush(1500);
    }

    static inline bool _mkdir(const char *dir, mode_t m) {
        bool rc = true;
        if (::mkdir(dir, m) != 0)
            if (errno != EEXIST)
                rc = false;
        return rc;
    }

    static inline bool _mkpath(char *p, mode_t m) {
        char *base = p, *dir = p;
        bool rc;

        while (*dir == '/') dir++;

        while ((dir = strchr(dir,'/')) != nullptr) {
            *dir = '\0';
            rc = _mkdir(base, m);
            *dir = '/';
            if (!rc)
                break;

            while (*dir == '/') dir++;
        }

        return _mkdir(base, m);
    }

    void utils::fs::mkdir(const char *path, bool recursive, mode_t mode) {

        bool status;
        String tmp = String{path}.dup();
        if (!tmp) {
            /* creating directory failed */
            throw Exception::create("mkdir '", path, "' failed: ",  errno_s);
        }

        if (recursive)
            status = _mkpath(tmp.data(), mode);
        else
            status = _mkdir(tmp.data(), mode);

        if (!status) {
            /* creating directory failed */
            throw Exception::create("mkdir '", path, "' failed: ",  errno_s);
        }
    }

    void utils::fs::mkdir(const char *base, const std::vector<const char*> paths, bool recursive, mode_t mode) {
        for (auto& p : paths) {
            if (p[0] == '/') {
                mkdir(p, recursive, mode);
            }
            else {
                String tmp = catstr(base, "/", p);
                mkdir(tmp.data(), recursive, mode);
            }
        }
    }

    String utils::fs::currdir() {
        char buf[PATH_MAX];
        if (getcwd(buf, PATH_MAX) == nullptr)
            throw Exception::create("getcwd failed: ", errno_s);
        return String{buf}.dup();
    }

    String utils::fs::getname(const char *path) {
        String tmp{String(path).dup()};
        char *name = basename(tmp.data());
        if (!name)
            throw Exception::create("basename failed: ", errno_s);
        return String{name}.dup();
    }

    bool utils::fs::isdirempty(const char *path) {
        DIR *dir = opendir(path);
        if (dir == nullptr)
            return false;
        struct dirent *d;
        size_t n{0};
        while ((d = readdir(dir))!= nullptr)
            if (++n > 2) break;
        closedir(dir);

        return (n <= 2);
    }

    static void _forall(const char *base, String& path, std::function<bool(const String&, bool)> h, bool recursive, bool pod) {
        String cdir(utils::catstr(base, "/", path));
        DIR *d = opendir(cdir.data());

        if (d == nullptr) {
            /* openining directory failed */
            throw Exception::create("opendir('", path, "') failed: ", errno_s);
        }

        struct dirent *tmp;

        while ((tmp = readdir(d)) != NULL)
        {
            /* ignore parent and current directory */
            if (utils::strmatchany(tmp->d_name, ".", ".."))
                continue;


            String ipath = path? utils::catstr(path, "/", tmp->d_name) : String(tmp->d_name);
            bool is_dir{tmp->d_type == DT_DIR};

            if (!pod) {
                if (!h(ipath, is_dir)) {
                    /* delegate cancelled travesal*/
                    break;
                }

                if (recursive && is_dir) {
                    /* recursively iterate current directory*/
                    _forall(base, ipath, h, recursive, pod);
                }
            }
            else {
                if (recursive && is_dir) {
                    /* recursively iterate current directory*/
                    _forall(base, ipath, h, recursive, pod);
                }

                if (!h(ipath, is_dir)) {
                    /* delegate cancelled travesal*/
                    break;
                }
            }
        }

        closedir(d);
    }

    void utils::fs::forall(const char *path, std::function<bool(const String&, bool)> h, bool recursive, bool pod) {
        String tmp{""};
        _forall(path, tmp, h, recursive, pod);
    }

    std::vector<String> utils::fs::ls(const char *path, bool recursive) {
        std::vector<String> all;
        fs::forall(path, [&all](const String& d, bool dir) {
            all.emplace_back(d.dup());
            return true;
        }, recursive);

        return std::move(all);
    }

    static inline void __remove(const char *path) {
        if (::remove(path) != 0) {
            throw Exception::create("remove '", path, "' failed: ", errno_s);
        }
    }

    static inline void _rmdir(const char *path) {
        utils::fs::forall(path,
              [&](const String& name, bool d) -> bool {
                  String tmp = utils::catstr(path, "/", name);
                  __remove(tmp.data());
                  return true;
              }, true, true);
    }

    void utils::fs::remove(const char *path, bool recursive, bool contents) {
        bool is_dir{isdir(path)};
        if (recursive && is_dir) {
            _rmdir(path);
        }

        if (!is_dir || !contents)
            __remove(path);
    }

    void utils::fs::remove(const char *base, const std::vector<const char*> paths, bool recursive) {
        for (auto& p : paths) {
            String tmp = p[0] == '/'? utils::catstr(base, p) : utils::catstr(base, "/", p);
            remove(tmp.data(), recursive);
        }
    }

    String utils::fs::readall(const char *path, bool async) {
        OBuffer b(512);
        fs::readall(b, path, async);
        return String(b);
    }

    void utils::fs::readall(OBuffer& out, const char *path, bool async) {
        /* read file contents into a buffer */
        if (!exists(path)) {
            throw Exception::create("file '", path, "' does not exist");
        }

        struct stat st;
        if (::stat(path, &st)) {
            /* getting file information failed */
            throw Exception::create("stat('", path, "') failed: ", errno_s);
        }

        if (st.st_size > 1048576) {
            /* size too large to be read by this API */
            throw Exception::create("file '", path, "' to large (",
                                    st.st_size, " bytes) to be read by fs::read_all");
        }

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            /* opening file failed */
            throw Exception::create("opening file '", path, "' failed: ", errno_s);
        }

        out.reserve((uint32_t) st.st_size);
        char *data = out.data();
        ssize_t nread = 0, rc = 0;
        do {
            rc = ::read(fd, &data[nread], (size_t)(st.st_size - nread));
            if (rc < 0) {
                /* reading file failed */
                throw Exception::create("reading '", path, "' failed: ", errno_s);
            }

            nread += rc;
        } while (nread < st.st_size);
        out.seek(nread);
    }

    void utils::fs::append(const char *path, const void *data, size_t sz, bool async) {
        File f(path, O_WRONLY|O_CREAT|O_APPEND, 0666);
        f.write(data, sz, 1500);
        f.close();
    }
}

#ifdef unit_test

#include <catch/catch.hpp>

using namespace suil;

TEST_CASE("utils::filesystem", "[utils][fs]") {
    // creating a single directory
    REQUIRE_NOTHROW(utils::fs::mkdir("test"));

    SECTION("basic file operations" "[fs][file_ops]") {
        // directory should have been created
        REQUIRE(utils::fs::exists("test"));
        // shouldn't files that don't exist
        REQUIRE_FALSE(utils::fs::exists("test1"));
        // Create another directory
        REQUIRE_NOTHROW(utils::fs::touch("test/file.txt"));
        // File should have been created
        REQUIRE(utils::fs::exists("test/file.txt"));
        // Size should be 0
        REQUIRE(utils::fs::size("test/file.txt") == 0);
        // Delete file, should not exist there after
        REQUIRE_NOTHROW(utils::fs::remove("test/file.txt"));
        // shouldn't files that don't exist
        REQUIRE_FALSE(utils::fs::exists("test/file.txt"));
    }

    SECTION("basic directory operations", "[fs][dir_ops]") {
        // Using created test directory
        REQUIRE(utils::fs::isdir("test"));
        // Create directory within test
        REQUIRE_NOTHROW(utils::fs::mkdir("test/test1"));
        // Should exist
        REQUIRE(utils::fs::exists("test/test1"));
        // Can be deleted if empty
        REQUIRE_NOTHROW(utils::fs::remove("test/test1"));
        // Should not exist
        REQUIRE_FALSE(utils::fs::exists("test/test1"));
        // Directory with parents requires recursive flag
        REQUIRE_THROWS(utils::fs::mkdir("test/test1/child1"));
        REQUIRE_NOTHROW(utils::fs::mkdir("test/test1/child1", true));
        // Should exist
        REQUIRE(utils::fs::exists("test/test1/child1"));
        // Deleting non-empty directory also requires the recursive flag
        REQUIRE_THROWS(utils::fs::remove("test/test1"));
        REQUIRE_NOTHROW(utils::fs::remove("test/test1", true));
        // Should not exist
        REQUIRE_FALSE(utils::fs::exists("test/test1"));
        // Multiple directories can be created at once
        REQUIRE_NOTHROW(utils::fs::mkdir("test", {"test1", "test2"}));
        // Test should have 2 directories
        REQUIRE(utils::fs::ls("test").size() == 2);
        // Remove all directories
        REQUIRE_NOTHROW(utils::fs::remove("test", {"test1", "test2"}));
        REQUIRE(utils::fs::ls("test").empty());
        // Recursive again
        REQUIRE_NOTHROW(utils::fs::mkdir("test",
                {"test1/child1/gchild1",
                 "test1/child1/gchild2"
                 "test1/child2/gchild1/ggchild/ggchild1",}, true));
        // list recursively
        REQUIRE(utils::fs::ls("test", true).size() == 8);
        // Remove recursively
        REQUIRE_NOTHROW(utils::fs::remove("test", true, true));
    }

    SECTION("Basic file IO", "[fs][file_io]") {
        const char *fname = "test/file.txt";
        // Reading non-existent file
        String data;
        REQUIRE_THROWS((data = utils::fs::readall(fname)));
        // Create empty file
        utils::fs::touch(fname, 0777);
        data = utils::fs::readall(fname);
        REQUIRE(data.empty());
        // Write data to file
        data = "The quick brown fox";
        utils::fs::append(fname, data);
        REQUIRE(utils::fs::size("test/file.txt") == data.size());
        String out = utils::fs::readall(fname);
        REQUIRE(data == out);
        // Append to file
        data = utils::catstr(data, "\n jumped over the lazy dog");
        utils::fs::append(fname, "\n jumped over the lazy dog");
        REQUIRE(utils::fs::size(fname) == data.size());
        out = utils::fs::readall(fname);
        REQUIRE(data == out);
        // Clear the file
        utils::fs::clear(fname);
        REQUIRE(utils::fs::size(fname) == 0);
    }

    // Cleanup
    REQUIRE_NOTHROW(utils::fs::remove("test", true));
}

#endif