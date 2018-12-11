//
// Created by dc on 28/06/17.
//

#ifndef SUIL_COMMON_HPP
#define SUIL_COMMON_HPP

#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <functional>
#include <vector>

#include <suil/json.h>

namespace suil {

    struct Auth {
        template <typename... A>
        Auth(A... a)
            : enabled(true)
        {
            addRoles(std::forward<A>(a)...);
        }

        Auth(bool en)
                : enabled(en)
        {}

        Auth(const Auth& auth) {
            for(auto& r: auth.roles) {
                roles.emplace_back(std::move(r.dup()));
            }
            enabled = auth.enabled;
        }
        Auth&operator=(const Auth& other) {
            for(auto& r: other.roles) {
                roles.emplace_back(std::move(r.dup()));
            }
            enabled = other.enabled;
            return *this;
        }

        Auth(Auth&& auth)
            : roles(std::move(auth.roles)),
              enabled(auth.enabled)
        {}

        Auth&operator=(const Auth&& other) {
            roles = std::move(other.roles);
            enabled = other.enabled;
            return *this;
        }

        Auth()
        {}

        operator bool() const {
            return enabled;
        }

        bool check(const std::vector<String>& rs) const {
            if (roles.empty()) {
                return true;
            }

            for (auto& r: roles) {
                for (auto& tmp: rs) {
                    if (r == tmp)
                        return true;
                }
            }
        }

        bool check(const json::Object& rs) const {

            if (roles.empty()) {
                /* not roles setup */
                return true;
            }

            for (auto& r: roles) {
                /* check if any of the roles is present */
                for (const auto [_, role] : rs)
                    /* roles must be strings */
                    if (((String)rs).compare(r, true))
                        return true;
            };
            return false;
        }

    private:
        template <typename... A>
        void addRoles(const char *r, A... a) {
            roles.emplace_back(String{r}.dup());
            if constexpr (sizeof...(a))
                addRoles(std::forward<A>(a)...);
        }

        bool  enabled{true};
        std::vector<String> roles{};
    };

    using Roles = Auth;

    typedef decltype(iod::D(
        prop(STATIC,         bool),
        prop(AUTHORIZE,      Auth),
        prop(PARSE_COOKIES,  bool),
        prop(PARSE_FORM,     bool),
        prop(REPLY_TYPE,     String)
    )) route_attributes_t;

    namespace magic {
        struct out_of_range {
            out_of_range(unsigned /*pos*/, unsigned /*length*/) {}
        };

        constexpr unsigned requires_in_range(unsigned i, unsigned len) {
            return i >= len ? throw out_of_range(i, len) : i;
        }

        class const_str {
            const char *const begin_;
            unsigned size_;

        public:
            template<unsigned N>
            constexpr const_str(const char(&arr)[N])
                    : begin_(arr), size_(N - 1) {
                static_assert(N >= 1, "not a string literal");
            }

            constexpr char operator[](unsigned i) const {
                return requires_in_range(i, size_), begin_[i];
            }

            constexpr operator const char *() const {
                return begin_;
            }

            constexpr const char *begin() const { return begin_; }

            constexpr const char *end() const { return begin_ + size_; }

            constexpr unsigned size() const {
                return size_;
            }
        };

        constexpr unsigned find_closing_tag(const_str s, unsigned p) {
            return s[p] == '}' ? p : find_closing_tag(s, p + 1);
        }

        constexpr bool is_valid(const_str s, unsigned i = 0, int f = 0) {
            return
                    i == s.size()
                    ? f == 0 :
                    f < 0 || f >= 2
                    ? false :
                    s[i] == '{'
                    ? is_valid(s, i + 1, f + 1) :
                    s[i] == '}'
                    ? is_valid(s, i + 1, f - 1) :
                    is_valid(s, i + 1, f);
        }

        constexpr bool is_equ_p(const char *a, const char *b, unsigned n) {
            return
                    *a == 0 && *b == 0 && n == 0
                    ? true :
                    (*a == 0 || *b == 0)
                    ? false :
                    n == 0
                    ? true :
                    *a != *b
                    ? false :
                    is_equ_p(a + 1, b + 1, n - 1);
        }

        constexpr bool is_equ_n(const_str a, unsigned ai, const_str b, unsigned bi, unsigned n) {
            return
                    ai + n > a.size() || bi + n > b.size()
                    ? false :
                    n == 0
                    ? true :
                    a[ai] != b[bi]
                    ? false :
                    is_equ_n(a, ai + 1, b, bi + 1, n - 1);
        }

        constexpr bool is_int(const_str s, unsigned i) {
            return is_equ_n(s, i, "{int}", 0, 5);
        }

        constexpr bool is_uint(const_str s, unsigned i) {
            return is_equ_n(s, i, "{uint}", 0, 6);
        }

        constexpr bool is_float(const_str s, unsigned i) {
            return is_equ_n(s, i, "{float}", 0, 7) ||
                   is_equ_n(s, i, "{double}", 0, 8);
        }

        constexpr bool is_str(const_str s, unsigned i) {
            return is_equ_n(s, i, "{str}", 0, 5) ||
                   is_equ_n(s, i, "{string}", 0, 8);
        }

        constexpr bool is_path(const_str s, unsigned i) {
            return is_equ_n(s, i, "{path}", 0, 6);
        }

        template<typename T>
        struct parameter_tag {
            static const int value = 0;
        };

#define INTERNAL_PARAMETER_TAG(t, i) \
        template <> \
        struct parameter_tag<t> \
        { \
            static const int value = i; \
        }

        INTERNAL_PARAMETER_TAG(int, 1);
        INTERNAL_PARAMETER_TAG(char, 1);
        INTERNAL_PARAMETER_TAG(short, 1);
        INTERNAL_PARAMETER_TAG(long, 1);
        INTERNAL_PARAMETER_TAG(long
                                       long, 1);
        INTERNAL_PARAMETER_TAG(unsigned
                                       int, 2);
        INTERNAL_PARAMETER_TAG(unsigned
                                       char, 2);
        INTERNAL_PARAMETER_TAG(unsigned
                                       short, 2);
        INTERNAL_PARAMETER_TAG(unsigned
                                       long, 2);
        INTERNAL_PARAMETER_TAG(unsigned
                               long
                                       long, 2);
        INTERNAL_PARAMETER_TAG(double, 3);
        INTERNAL_PARAMETER_TAG(std::string, 4);
#undef INTERNAL_PARAMETER_TAG

        template<typename ... Args>
        struct compute_parameter_tag_from_args_list;
        template<>
        struct compute_parameter_tag_from_args_list<> {
            static const int value = 0;
        };

        template<typename Arg, typename ... Args>
        struct compute_parameter_tag_from_args_list<Arg, Args...> {
            static const int sub_value =
                    compute_parameter_tag_from_args_list<Args...>::value;
            static const int value =
                    parameter_tag<typename std::decay<Arg>::type>::value
                    ? sub_value * 6 + parameter_tag<typename std::decay<Arg>::type>::value
                    : sub_value;
        };

        static inline bool is_parameter_tag_compatible(uint64_t a, uint64_t b) {
            if (a == 0)
                return b == 0;
            if (b == 0)
                return a == 0;
            int sa = a % 6;
            int sb = a % 6;
            if (sa == 5) sa = 4;
            if (sb == 5) sb = 4;
            if (sa != sb)
                return false;
            return is_parameter_tag_compatible(a / 6, b / 6);
        }

        static inline unsigned find_closing_tag_runtime(const char *s, unsigned p) {
            return
                    s[p] == 0
                    ? throw std::runtime_error("unmatched tag {") :
                    s[p] == '}'
                    ? p : find_closing_tag_runtime(s, p + 1);
        }

        static inline uint64_t get_parameter_tag_runtime(const char *s, unsigned p = 0) {
            return
                    s[p] == 0
                    ? 0 :
                    s[p] == '{' ? (
                            std::strncmp(s + p, "{int}", 5) == 0
                            ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 1 :
                            std::strncmp(s + p, "{uint}", 6) == 0
                            ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 2 :
                            (std::strncmp(s + p, "{float}", 7) == 0 ||
                             std::strncmp(s + p, "{double}", 8) == 0)
                            ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 3 :
                            (std::strncmp(s + p, "{str}", 5) == 0 ||
                             std::strncmp(s + p, "{string}", 8) == 0)
                            ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 4 :
                            std::strncmp(s + p, "{path}", 6) == 0
                            ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 5 :
                            throw std::runtime_error("invalid parameter type")
                    ) :
                    get_parameter_tag_runtime(s, p + 1);
        }

        constexpr uint64_t get_parameter_tag(const_str s, unsigned p = 0) {
            return
                    p == s.size()
                    ? 0 :
                    s[p] == '{' ? (
                            is_int(s, p)
                            ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 1 :
                            is_uint(s, p)
                            ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 2 :
                            is_float(s, p)
                            ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 3 :
                            is_str(s, p)
                            ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 4 :
                            is_path(s, p)
                            ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 5 :
                            throw std::runtime_error("invalid parameter type")
                    ) :
                    get_parameter_tag(s, p + 1);
        }

        template<typename ... T>
        struct S {
            template<typename U>
            using push = S<U, T...>;
            template<typename U>
            using push_back = S<T..., U>;
            template<template<typename ... Args> class U>
            using rebind = U<T...>;
        };
        template<typename F, typename Set>
        struct CallHelper;

        template<typename F, typename ...Args>
        struct CallHelper<F, S<Args...>> {
            template<typename F1, typename ...Args1, typename =
            decltype(std::declval<F1>()(std::declval<Args1>()...))
            >
            static char __test(int);

            template<typename ...>
            static int __test(...);

            static constexpr bool value = sizeof(__test<F, Args...>(0)) == sizeof(char);
        };


        template<int N>
        struct single_tag_to_type {
        };

        template<>
        struct single_tag_to_type<1> {
            using type = int64_t;
        };

        template<>
        struct single_tag_to_type<2> {
            using type = uint64_t;
        };

        template<>
        struct single_tag_to_type<3> {
            using type = double;
        };

        template<>
        struct single_tag_to_type<4> {
            using type = std::string;
        };

        template<>
        struct single_tag_to_type<5> {
            using type = std::string;
        };


        template<uint64_t Tag>
        struct arguments {
            using subarguments = typename arguments<Tag / 6>::type;
            using type =
            typename subarguments::template push<typename single_tag_to_type<Tag % 6>::type>;
        };

        template<>
        struct arguments<0> {
            using type = S<>;
        };

        template<typename ... T>
        struct last_element_type {
            using type = typename std::tuple_element<sizeof...(T) - 1, std::tuple<T...>>::type;
        };


        template<>
        struct last_element_type<> {
        };


        // from http://stackoverflow.com/questions/13072359/c11-compile-time-array-with-logarithmic-evaluation-depth
        template<class T> using Invoke = typename T::type;

        template<unsigned...>
        struct seq {
            using type = seq;
        };

        template<class S1, class S2>
        struct concat;

        template<unsigned... I1, unsigned... I2>
        struct concat<seq<I1...>, seq<I2...>>
                : seq<I1..., (sizeof...(I1) + I2)...> {
        };

        template<class S1, class S2>
        using Concat = Invoke<concat<S1, S2>>;

        template<unsigned N>
        struct gen_seq;
        template<unsigned N> using GenSeq = Invoke<gen_seq<N>>;

        template<unsigned N>
        struct gen_seq : Concat<GenSeq<N / 2>, GenSeq<N - N / 2>> {
        };

        template<>
        struct gen_seq<0> : seq<> {
        };
        template<>
        struct gen_seq<1> : seq<0> {
        };

        template<typename Seq, typename Tuple>
        struct pop_back_helper;

        template<unsigned ... N, typename Tuple>
        struct pop_back_helper<seq<N...>, Tuple> {
            template<template<typename ... Args> class U>
            using rebind = U<typename std::tuple_element<N, Tuple>::type...>;
        };

        template<typename ... T>
        struct pop_back //: public pop_back_helper<typename gen_seq<sizeof...(T)-1>::type, std::tuple<T...>>
        {
            template<template<typename ... Args> class U>
            using rebind = typename pop_back_helper<typename gen_seq<
                    sizeof...(T) - 1>::type, std::tuple<T...>>::template rebind<U>;
        };

        template<>
        struct pop_back<> {
            template<template<typename ... Args> class U>
            using rebind = U<>;
        };

        // from http://stackoverflow.com/questions/2118541/check-if-c0x-parameter-pack-contains-a-type
        template<typename Tp, typename... List>
        struct contains : std::true_type {
        };

        template<typename Tp, typename Head, typename... Rest>
        struct contains<Tp, Head, Rest...>
                : std::conditional<std::is_same<Tp, Head>::value,
                        std::true_type,
                        contains<Tp, Rest...>
                >::type {
        };

        template<typename Tp>
        struct contains<Tp> : std::false_type {
        };

        template<typename T>
        struct empty_context {
        };

        template<typename T>
        struct promote {
            using type = T;
        };

#define INTERNAL_PROMOTE_TYPE(t1, t2) \
        template<> \
        struct promote<t1> \
        {  \
            using type = t2; \
        }

        INTERNAL_PROMOTE_TYPE(char, int64_t);
        INTERNAL_PROMOTE_TYPE(short, int64_t);
        INTERNAL_PROMOTE_TYPE(int, int64_t);
        INTERNAL_PROMOTE_TYPE(long, int64_t);
        INTERNAL_PROMOTE_TYPE(long
                                      long, int64_t);
        INTERNAL_PROMOTE_TYPE(unsigned
                                      char, uint64_t);
        INTERNAL_PROMOTE_TYPE(unsigned
                                      short, uint64_t);
        INTERNAL_PROMOTE_TYPE(unsigned
                                      int, uint64_t);
        INTERNAL_PROMOTE_TYPE(unsigned
                                      long, uint64_t);
        INTERNAL_PROMOTE_TYPE(unsigned
                              long
                                      long, uint64_t);
        INTERNAL_PROMOTE_TYPE(float, double);
#undef INTERNAL_PROMOTE_TYPE

        template<typename T>
        using promote_t = typename promote<T>::type;

    } // namespace magic

    namespace detail {

        template<class T, std::size_t N, class... Args>
        struct get_index_of_element_from_tuple_by_type_impl {
            static constexpr auto value = N;
        };

        template<class T, std::size_t N, class... Args>
        struct get_index_of_element_from_tuple_by_type_impl<T, N, T, Args...> {
            static constexpr auto value = N;
        };

        template<class T, std::size_t N, class U, class... Args>
        struct get_index_of_element_from_tuple_by_type_impl<T, N, U, Args...> {
            static constexpr auto value = get_index_of_element_from_tuple_by_type_impl<T, N + 1, Args...>::value;
        };

    } // namespace detail

    template<class T, class... Args>
    T &get_element_by_type(std::tuple<Args...> &t) {
        return std::get<detail::get_index_of_element_from_tuple_by_type_impl<T, 0, Args...>::value>(t);
    }

    template<typename T>
    struct function_traits;

    template<typename T>
    struct function_traits : public function_traits<decltype(&T::operator())> {
        using parent_t = function_traits<decltype(&T::operator())>;
        static const size_t arity = parent_t::arity;
        using result_type = typename parent_t::result_type;
        template<size_t i>
        using arg = typename parent_t::template arg<i>;

    };

    template<typename ClassType, typename R, typename ...Args>
    struct function_traits<R(ClassType::*)(Args...) const> {
        static const size_t arity = sizeof...(Args);

        typedef R result_type;

        template<size_t i>
        using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };

    template<typename ClassType, typename R, typename ...Args>
    struct function_traits<R(ClassType::*)(Args...)> {
        static const size_t arity = sizeof...(Args);

        typedef R result_type;

        template<size_t i>
        using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };

    template<typename R, typename ...Args>
    struct function_traits<std::function<R(Args...)>> {
        static const size_t arity = sizeof...(Args);
        typedef R result_type;
        template<size_t i>
        using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };

    namespace detail {

        enum class ParamType {
            INT,
            UINT,
            DOUBLE,
            STRING,
            PATH,
            MAX
        };

        struct routing_params {
            struct strparams {
                const char *str;
                uint32_t len;

                strparams(strview &sv)
                        : str(sv.data()),
                          len(sv.size()) {}

                strparams(const strparams &other)
                        : str(other.str),
                          len(other.len) {}

                strparams &operator=(const strparams &other) {
                    str = other.str;
                    len = other.len;
                    return *this;
                }

                operator strview() {
                    return strview(str, len);
                }
            };

            template<typename _T>
            struct params_container {
                _T *params{nullptr};
                uint8_t size{0};
                uint8_t back{0};

                params_container() {}

                params_container(params_container &&pc)
                        : params(pc.params),
                          size(pc.size),
                          back(pc.back) {
                    pc.params = nullptr;
                    pc.size = 0;
                    pc.back = 0;
                }

                params_container<_T> &operator=(params_container<_T> &&pc) {
                    params = pc.params;
                    pc.params = nullptr;
                    size = pc.size;
                    pc.size = 0;
                    back = pc.back;
                    pc.back = 0;
                    return *this;
                }

                template<typename _U = _T>
                void push(typename std::enable_if<std::is_same<_T,
                        detail::routing_params::strparams>::value,
                        _U>::type &t) {
                    grow();
                    params[back++] = t;
                }

                template<typename _U = _T>
                void push(typename std::enable_if<!std::is_same<_T,
                        detail::routing_params::strparams>::value, _U>::type t) {
                    grow();
                    params[back++] = t;
                };

                void pop() {
                    if (back > 0) {
                        back--;
                    }
                }

                template<typename _U = _T>
                typename std::enable_if<std::is_same<_T,
                        detail::routing_params::strparams>::value, _U>::type &
                operator[](unsigned idx) const {
                    if (params && idx < back) {
                        return params[idx];
                    }
                    throw std::runtime_error("index out of bounds");
                }

                template<typename _U = _T>
                typename std::enable_if<!std::is_same<_T,
                        detail::routing_params::strparams>::value, _U>::type
                operator[](unsigned idx) const {
                    if (params && idx < back) {
                        return params[idx];
                    }
                    throw std::runtime_error("index out of bounds");
                }

                ~params_container() {
                    clear();
                }

                inline void clear() {
                    if (params) {
                        free(params);
                        params = nullptr;
                    }
                }

            private:
                inline void grow() {
                    if (params == nullptr) {
                        size = 8;
                        params = (_T *) malloc(sizeof(_T) * 8);
                    } else if (back == size) {
                        size *= 2;
                        params = (_T *) realloc(params, sizeof(_T) * size);
                    }
                }
            };

            params_container<int64_t> int_params{};
            params_container<uint64_t> uint_params{};
            params_container<double> double_params{};
            params_container<strparams> string_params{};

            void debug_print() const;

            template<typename T>
            T get(unsigned) const;

            template<typename T>
            void push(T);

            template<typename T>
            void pop(T);

            routing_params(const routing_params&) = delete;
            routing_params&operator=(const routing_params&) = delete;

            routing_params(routing_params&& params)
                : int_params(std::move(params.int_params)),
                  double_params(std::move(params.double_params)),
                  uint_params(std::move(params.uint_params)),
                  string_params(std::move(params.string_params))
            {}

            routing_params&operator=(routing_params&& params) {
                int_params = std::move(params.int_params);
                double_params = std::move(params.double_params);
                uint_params = std::move(params.uint_params);
                string_params = std::move(params.string_params);
            }

            routing_params()
            {}

            inline void clear() {
                int_params.clear();
                uint_params.clear();
                double_params.clear();
                string_params.clear();
            }

            ~routing_params() {
                clear();
            }
        };

        template<>
        inline void routing_params::params_container<routing_params::strparams>::pop() {
            if (back > 0) {
                back--;
                // destroy the parameter
                params[back].~strparams();
            }
        }

        template<>
        inline void routing_params::push<int64_t>(int64_t v) {
            int_params.push(v);
        }

        template<>
        inline void routing_params::push<uint64_t>(uint64_t v) {
            uint_params.push(v);
        }

        template<>
        inline void routing_params::push<double>(double v) {
            double_params.push(v);
        }

        template<>
        inline void routing_params::push<strview>(strview v) {
            strparams str(v);
            string_params.push(str);
        }

        template<>
        inline void routing_params::pop<int64_t>(int64_t) {
            int_params.pop();
        }

        template<>
        inline void routing_params::pop<uint64_t>(uint64_t) {
            uint_params.pop();
        }

        template<>
        inline void routing_params::pop<double>(double) {
            double_params.pop();
        }

        template<>
        inline void routing_params::pop<strview>(strview) {
            string_params.pop();
        }

        template<>
        inline int64_t routing_params::get<int64_t>(unsigned index) const {
            return int_params[index];
        }

        template<>
        inline uint64_t routing_params::get<uint64_t>(unsigned index) const {
            return uint_params[index];
        }

        template<>
        inline double routing_params::get<double>(unsigned index) const {
            return double_params[index];
        }

        template<>
        inline std::string routing_params::get<std::string>(unsigned index) const {
            routing_params::strparams &p = string_params[index];
            return std::string(p.str, p.len);
        }
    }

    namespace http {
        struct BaseRule;
        struct router_params_t {
            router_params_t(unsigned first, suil::detail::routing_params&& second)
                : first(first),
                  second(std::move(second))
            {}

            router_params_t(router_params_t&& params)
                : second(std::move(params.second)),
                  first(params.first)
            {}

            router_params_t&operator=(router_params_t&& params) {
                second = std::move(params.second);
                first = params.first;
            }

            router_params_t(const router_params_t&) = delete;
            router_params_t&operator=(const router_params_t&) = delete;

            unsigned first{0};
            suil::detail::routing_params second;
        };

        struct request_params_t {
            unsigned index{0};
            suil::detail::routing_params decoded;
            route_attributes_t  *attrs{nullptr};
            uint32_t            methods{0};

            inline void  clear() {
                decoded.clear();
                attrs = nullptr;
            }
        };
    }
}

#endif //SUIL_COMMON_HPP
