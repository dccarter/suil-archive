//
// Created by dc on 09/11/18.
//

#ifndef SUIL_ZSTRING_H
#define SUIL_ZSTRING_H

#include <vector>

#include <suil/base.h>

namespace suil {

    struct OBuffer;

    /**
     * A light-weight string abstraction
     */
    struct String {

        /**
         * creates an empty string
         */
        String();

        /**
         * creates a string of with character \param c duplicated
         * \param n times
         * @param c the character to initialize the string with
         * @param n the initial size of the string
         */
        explicit String(char c, size_t n);

        /**
         * creates a view of the given c-style string. does not own
         * the memory and must not outlive the memory
         * @param str the string to reference
         */
        String(const char *str);

        /**
         * creates a string from the given string view
         * @param str the string view to reference or whose buffer must be own
         * @param own if false, a reference to the string view buffer will be kept
         * otherwise the reference will be own and be free when the String is being destroyed
         */
        explicit String(const strview str);

        /**
         * creates a string from the given standard string
         * @param str the standard string to reference or duplicate
         * @param own if true, the standard string will be duplicated,
         * otherwise a reference to the string will be used
         */
        explicit String(const std::string& str, bool own = false);

        /**
         * creates a new string referencing the given string and constrained
         * to the given size.
         * @param str the buffer to reference
         * @param len the size of the buffer visible to the string
         * @param own true if the string should assume ownership of the buffer
         */
        explicit String(const char *str, size_t len, bool own = true);

        /**
         * creates a string by referencing the buffer of an output buffer
         * @param b the output buffer to be reference
         * @param own if true the output buffer will be released to this String
         */
        String(OBuffer& b, bool own = true);

        /**
         * moves construct string \param s to this string
         * @param s
         */
        String(String&& s) noexcept;

        /**
         * move assign string \param s to this string
         * @param s
         * @return
         */
        String& operator=(String&& s) noexcept;

        /**
         * copy construct string \param s into this string
         * @param s string to copy
         */
        String(const String& s);

        /**
         * copy assign string \param s into this string
         * @param \s the string to copy
         */
        String& operator=(const String& s);

        /**
         * duplicate current string, returning a new string
         * with it's own buffer
         */
        String dup() const;

        /**
         * take a reference to this string
         * @return a string which reference's this string's buffer
         * but does not own it
         */
        String peek() const;

        /**
         * transform all characters in the string to uppercase
         */
        void toupper();

        /**
         * transform all characters in this string to lowercase
         */
        void tolower();

        /**
         * check if the string references any buffer with a size greater than 0
         * @return true if string refers to a valid of size greater than 0
         */
        bool empty() const;

        /**
         * boolean cast operator
         * @return \see String::empty
         */
        inline operator bool() const {
            return !empty();
        }

        /**
         * strview cast operator
         * @return string view whose data is the buffer referred to by this string
         */
        operator strview() const {
            return strview(m_str, m_len);
        }

        /**
         * eq equality operator - compares two string for equality
         * @param s string to compare against
         * @return true if the two strings are equal, false otherwise
         */
        bool operator==(const String& s) const;

        /**
         * neq equality operator - compares two string for equality
         * @param s string to compare against
         * @return true if the two strings are not equal, false otherwise
         */
        bool operator!=(const String& s) const {
            return !(Ego == s);
        }

        /**
         * find the occurrence of the given character, \param ch in the string,
         * searching from the beginning of the string towards the end
         * @param ch the character to search
         * @return the position of the first occurrance of the character if
         * found, otherwise, -1 is returned
         */
        ssize_t find(char ch) const;

        /**
         * find the occurrence of the given character, \param ch in the string,
         * searching from the end of the string towards the beginning
         * @param ch the character to search
         * @return the position of the first occurrence of the character if
         * found, otherwise, -1 is returned
         */
        ssize_t rfind(char ch) const;

        /**
         * get the substring of the current string
         *
         * @param from the position to start capturing the substring
         * @param nchars the number of characters to capture. If not specified
         * the capture will terminate at the last character
         * @param zc used to control how the substring is returned. if true,
         * only a string referencing the calling strings substring portion will
         * be returned, otherwise a copy of that portion will be returned
         *
         * @return the requested substring if not out of bounds, otherwise an
         * empty string is returned
         */
        String substr(size_t from, size_t nchars = 0, bool zc = true) const;

        /**
         * take a chunk of the string starting from first occurrence of the given
         * character depending on the find direction
         * @param ch the character to search and start chunking from
         * @param reverse if true character is search from end of string,
         * otherwise from begining of string
         */
        String chunk(const char ch, bool reverse = false) const {
            ssize_t from = reverse? rfind(ch) : find(ch);
            if (from < 0)
                return String{};
            else
                return String{&Ego.m_cstr[from], (size_t)(Ego.m_len-from), false};
        }

        /**
         * splits the given string at every occurrence of the given
         * delimiter
         *
         * @param the delimiter to use to split the string
         *
         * @return a vector containing pointers to the start of the
         * parts of the split string
         *
         * @note the string will be modified by replacing the end of each
         * part with a '\0' character
         */
        const std::vector<char*> split(const char *delim);

        /**
         * remove the occurrence of the given \param strip character from the string.
         * @param strip the character to remove from the string
         * @param ends if true, only the character at the ends will be removed
         * @return a new string with the give \param character removed from the
         * string
         */
        String strip(char strip = ' ', bool ends = false);

        /**
         * \see String::strip
         * @param what
         * @return
         */
        inline String trim(char what = ' ') {
            return strip(what, true);
        }


        /**
         * compare against given c-style string
         * @param s c-style string to compare against
         * @return \see strncmp
         */
        inline int compare(const char* s, bool igc = false) const {
            size_t  len = strlen(s);
            if (len == m_len)
                return igc? strncasecmp(m_str, s, m_len) : strncmp(m_str, s, m_len);
            return m_len < len? -1 : 1;
        }

        /**
         * compare against another string
         * @param s other string to compare against
         * @return \see strncmp
         */
        inline int compare(const String& s, bool igc = false) const {
            if (s.m_len == m_len)
                return igc? strncasecmp(m_str, s.m_str, m_len) : strncmp(m_str, s.m_str, m_len);
            return m_len < s.m_len? -1 : 1;
        }

        /**
         * gt equality operator - check if this string is greater than
         * string
         * @param s
         * @return
         */
        inline bool operator>(const String& s) const {
            return Ego.compare(s) > 0;
        }

        /**
         * gte equality operator - check if this string is greater equal to
         * given string
         * @param s
         * @return
         */
        inline bool operator>=(const String& s) const {
            return Ego.compare(s) >= 0;
        }

        /**
         * le equality operator - check if this string is less than
         * string
         * @param s
         * @return
         */
        inline bool operator<(const String& s) const {
            return Ego.compare(s) < 0;
        }

        /**
         * lte equality operator - check if this string is less than or equal to
         * given string
         * @param s
         * @return
         */
        inline bool operator<=(const String& s) const {
            return Ego.compare(s) <= 0;
        }

        /**
         * \see String::cm_str
         * @return
         */
        inline const char* operator()() const {
            return Ego.c_str();
        }

        /**
         * get constant pointer to the reference buffer
         * @return
         */
        inline const char* data() const {
            return Ego.m_cstr;
        }

        /**
         * get a modifiable pointer to the reference buffer
         * @return
         */
        inline char* data() {
            return Ego.m_str;
        }

        /**
         * get the string size
         * @return
         */
        inline size_t size() const {
            return Ego.m_len;
        }

        size_t hash() const;

        /**
         * add operator concatenate given parameter to this string, implementation
         * differs depending on the type \tparam T
         * @tparam T the type of the parameter being concatenated
         * @param t the value to concatenate with this string
         * @return
         */
        template <typename T>
        inline String& operator+=(T t);

        template <typename T>
        inline String operator+(const T t);

        /**
         * get the underlying buffer as a c string, if the buffer is null
         * return whatever is passed as \param nil
         * @param nil the value to return incase the buffer is null
         * @return
         */
        const char* c_str(const char *nil = "") const;

        /**
         * casts the string to the give type
         * @tparam T
         * @return
         */
        template <typename T>
        explicit inline operator T() const;

        ~String();

    private suil_ut:
        friend struct hasher;
        union {
            char  *m_str;
            const char *m_cstr;
        };

        uint32_t m_len{0};
        bool     m_own{false};
        size_t   m_hash{0};
    };

    /**
     * std string keyed map case sensitive comparator
     */
    struct std_map_eq {
        inline bool operator()(const std::string& l, const std::string& r) const
        {
            return std::equal(l.begin(), l.end(), r.begin(), r.end());
        }
    };

    /**
     * sapi string (\class String) keyed map case sensitive comparator
     */
    struct map_eq {
        inline bool operator()(const String& l, const String& r) const
        {
            return l == r;
        }
    };

    /**
     * sapi string (\class String) keyed map case insensitive comparator
     */
    struct map_cmp {
        inline bool operator()(const String& l, const String& r) const
        {
            return l < r;
        }
    };

    struct map_case_eq {
        inline bool operator()(const String& l, const String& r) const
        {
            if (l.data() != nullptr) {
                return ((l.data() == r.data()) && (l.size() == r.size())) ||
                       (strncasecmp(l.data(), r.data(), std::min(l.size(), r.size())) == 0);
            }
            return l.data() == r.data();
        }
    };

}
#endif //SUIL_ZSTRING_H
