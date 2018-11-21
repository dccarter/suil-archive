//
// Created by dc on 04/04/17.
//

#ifndef SUIL_ROUTES_HPP
#define SUIL_ROUTES_HPP

#include <suil/http/request.h>
#include <suil/http/response.h>

namespace suil {

    namespace http {

        class BaseRule {
        public:
            BaseRule(std::string rule)
                : rule_(std::move(rule))
            {}

            virtual ~BaseRule() = default;

            virtual void validate() = 0;

            virtual void handle(const Request&,
                                Response&,
                                const suil::detail::routing_params &) = 0;

            uint32_t get_methods() {
                return methods_;
            }

            friend class Router;
            route_attributes_t attrs_{false, false, false, false, nullptr};

        protected:
            uint32_t methods_{1 << (uint16_t) Method::Get};

            std::string rule_;
            std::string name_;

            template<typename T>
            friend struct rule_parameter_traits;
        };

        namespace detail {
            namespace routing_handler_call_helper {
                template<typename T, int Pos>
                struct call_pair {
                    using type = T;
                    static const int pos = Pos;
                };

                template<typename H1>
                struct call_params {
                    H1 &handler;
                    const suil::detail::routing_params &params;
                    const Request &req;
                    Response &res;
                };

                template<typename F, int NInt, int NUint, int NDouble, int NString, typename S1, typename S2>
                struct call {
                };

                template<typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2>
                struct call<F, NInt, NUint, NDouble, NString, magic::S<int64_t, Args1...>, magic::S<Args2...>> {
                    void operator()(F cparams) {
                        using pushed = typename magic::S<Args2...>::template push_back<call_pair<int64_t, NInt>>;
                        call<F, NInt + 1, NUint, NDouble, NString,
                                magic::S<Args1...>, pushed>()(cparams);
                    }
                };

                template<typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2>
                struct call<F, NInt, NUint, NDouble, NString, magic::S<uint64_t, Args1...>, magic::S<Args2...>> {
                    void operator()(F cparams) {
                        using pushed = typename magic::S<Args2...>::template push_back<call_pair<uint64_t, NUint>>;
                        call<F, NInt, NUint + 1, NDouble, NString,
                                magic::S<Args1...>, pushed>()(cparams);
                    }
                };

                template<typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2>
                struct call<F, NInt, NUint, NDouble, NString, magic::S<double, Args1...>, magic::S<Args2...>> {
                    void operator()(F cparams) {
                        using pushed = typename magic::S<Args2...>::template push_back<call_pair<double, NDouble>>;
                        call<F, NInt, NUint, NDouble + 1, NString,
                                magic::S<Args1...>, pushed>()(cparams);
                    }
                };

                template<typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2>
                struct call<F, NInt, NUint, NDouble, NString, magic::S<std::string, Args1...>, magic::S<Args2...>> {
                    void operator()(F cparams) {
                        using pushed = typename magic::S<Args2...>::template push_back<call_pair<std::string, NString>>;
                        call<F, NInt, NUint, NDouble, NString + 1,
                                magic::S<Args1...>, pushed>()(cparams);
                    }
                };

                template<typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1>
                struct call<F, NInt, NUint, NDouble, NString, magic::S<>, magic::S<Args1...>> {
                    void operator()(F cparams) {
                        cparams.handler(
                                cparams.req,
                                cparams.res,
                                cparams.params.template get<typename Args1::type>(Args1::pos)...
                        );
                    }
                };

                template<typename Func, typename ... ArgsWrapped>
                struct Wrapped {
                    template<typename ... Args>
                    void set(Func f, typename std::enable_if<
                            !std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const Request &>::value, int>::type = 0) {
                        handler_ = (
                                [f = std::move(f)]
                                        (const Request &, Response &res, Args... args) {
                                    res = Response(f(args...));
                                    res.end();
                                });
                    }

                    template<typename Req, typename ... Args>
                    struct req_handler_wrapper {
                        req_handler_wrapper(Func f)
                                : f(std::move(f)) {
                        }

                        void operator()(const Request &req, Response &res, Args... args) {
                            res = Response(f(req, args...));
                            res.end();
                        }

                        Func f;
                    };

                    template<typename ... Args>
                    void set(Func f, typename std::enable_if<
                            std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const Request &>::value &&
                            !std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, Response &>::value, int>::type = 0) {
                        handler_ = req_handler_wrapper<Args...>(std::move(f));
                        /*handler_ = (
                            [f = std::move(f)]
                            (const Request& req, Response& res, Args... args){
                                 res = Response(f(req, args...));
                                 res.end();
                            });*/
                    }

                    template<typename ... Args>
                    void set(Func f, typename std::enable_if<
                            std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const Request &>::value &&
                            std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, Response &>::value, int>::type = 0) {
                        handler_ = std::move(f);
                    }

                    template<typename ... Args>
                    struct handler_type_helper {
                        using type = std::function<void(const Request &, Response &, Args...)>;
                        using args_type = magic::S<typename magic::promote_t<Args>...>;
                    };

                    template<typename ... Args>
                    struct handler_type_helper<const Request &, Args...> {
                        using type = std::function<void(const Request &, Response &, Args...)>;
                        using args_type = magic::S<typename magic::promote_t<Args>...>;
                    };

                    template<typename ... Args>
                    struct handler_type_helper<const Request &, Response &, Args...> {
                        using type = std::function<void(const Request &, Response &, Args...)>;
                        using args_type = magic::S<typename magic::promote_t<Args>...>;
                    };

                    typename handler_type_helper<ArgsWrapped...>::type handler_;

                    void operator()(const Request &req, Response &res, const suil::detail::routing_params &params) {
                        detail::routing_handler_call_helper::call<
                        detail::routing_handler_call_helper::call_params<
                        decltype(handler_)>,
                                0, 0, 0, 0,
                                typename handler_type_helper<ArgsWrapped...>::args_type,
                                magic::S<>
                                >()(
                                        detail::routing_handler_call_helper::call_params<
                                        decltype(handler_)>
                                        {handler_, params, req, res}
                                );
                    }
                };
            }
        }

        template <typename T>
        struct rule_parameter_traits
        {
            using self_t = T;

            self_t& name(std::string name) noexcept
            {
                ((self_t*)this)->name_ = std::move(name);
                return (self_t&)*this;
            }

            self_t& methods(Method method)
            {
                ((self_t*)this)->methods_ = 1 << (int)method;
                return (self_t&)*this;
            }

            template <typename ... MethodArgs>
            self_t& methods(Method method, MethodArgs ... args_method)
            {
                methods(args_method...);
                ((self_t*)this)->methods_ |= 1 << (int)method;
                return (self_t&)*this;
            }
        };

        class DynamicRule : public BaseRule, public rule_parameter_traits<DynamicRule>
        {
        public:

            DynamicRule(std::string rule)
                : BaseRule(std::move(rule))
            {
            }

            void validate() override
            {
                if (!erased_handler_)
                {
                    throw std::runtime_error((name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_).c_str());
                }
            }

            void handle(const Request& req, Response& res, const suil::detail::routing_params& params) override
            {
                erased_handler_(req, res, params);
            }

            DynamicRule&operator()(Method m) {
                return methods(m);
            }

            template <typename... Methods>
            DynamicRule& operator()(Method m, Methods... ms) {
                return methods(m, ms...);
            }

            template <typename... Opts>
            DynamicRule& attrs(Opts... opts) {
                /* apply configuration options */
                utils::apply_config(attrs_, opts...);
                return *this;
            }

            template <typename Func>
            void operator()(Func f)
            {
                using function_t = function_traits<Func>;
                erased_handler_ = wrap(std::move(f), magic::gen_seq<function_t::arity>());
            }

            // enable_if Arg1 == Request && Arg2 == Response
            // enable_if Arg1 == Request && Arg2 != resposne
            // enable_if Arg1 != Request
            template <typename Func, unsigned ... Indices>
            std::function<void(const Request&, Response&, const suil::detail::routing_params&)>
            wrap(Func f, magic::seq<Indices...>)
            {
                using function_t = function_traits<Func>;
                if (!magic::is_parameter_tag_compatible(
                        magic::get_parameter_tag_runtime(rule_.c_str()),
                        magic::compute_parameter_tag_from_args_list<
                                typename function_t::template arg<Indices>...>::value))
                {
                    throw std::runtime_error("route_dynamic: Handler type is mismatched with URL parameters: " + rule_);
                }
                auto ret = detail::routing_handler_call_helper::Wrapped<Func, typename function_t::template arg<Indices>...>();
                ret.template set<
                        typename function_t::template arg<Indices>...
                >(std::move(f));
                return ret;
            }

            template <typename Func>
            void operator()(std::string name, Func&& f)
            {
                name_ = std::move(name);
                (*this).template operator()<Func>(std::forward(f));
            }
        private:
            std::function<void(const Request&, Response&, const suil::detail::routing_params&)> erased_handler_;

        };

        template <typename ... Args>
        class TaggedRule : public BaseRule, public rule_parameter_traits<TaggedRule<Args...>> {
        public:
            using self_t = TaggedRule<Args...>;

            TaggedRule(std::string rule)
                : BaseRule(std::move(rule))
            {}

            void validate() override {
                if (!handler_) {
                    throw std::runtime_error(
                            (name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_).c_str());
                }
            }

            template<typename Func>
            typename std::enable_if<magic::CallHelper<Func, magic::S<Args...>>::value, void>::type
            operator()(Func &&f) {
                static_assert(magic::CallHelper<Func, magic::S<Args...>>::value ||
                              magic::CallHelper<Func, magic::S<Request, Args...>>::value,
                              "Handler type is mismatched with URL parameters");
                static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value,
                              "Handler function cannot have void return type; valid return types: string, int, resposne, json object");

                handler_ = [f = std::move(f)](const Request &, Response &res, Args ... args) {
                    res = Response(f(args...));
                    res.end();
                };
            }

            self_t &operator()(Method m) {
                return rule_parameter_traits<TaggedRule< Args...>>::methods(m);
            }

            template<typename... Methods>
            self_t &operator()(Method m, Methods... ms) {
                return rule_parameter_traits<TaggedRule<Args...>>::methods(m, ms...);
            }

            template <typename... Opts>
            self_t& attrs(Opts... opts) {
                /* apply configuration options */
                utils::apply_config(attrs_, opts...);
                return *this;
            }

            template<typename Func>
            typename std::enable_if<
                    !magic::CallHelper<Func, magic::S<Args...>>::value &&
                    magic::CallHelper<Func, magic::S<Request, Args...>>::value,
                    void>::type
            operator()(Func &&f) {
                static_assert(magic::CallHelper<Func, magic::S<Args...>>::value ||
                              magic::CallHelper<Func, magic::S<Request, Args...>>::value,
                              "Handler type is mismatched with URL parameters");
                static_assert(!std::is_same<void, decltype(f(std::declval<Request>(), std::declval<Args>()...))>::value,
                              "Handler function cannot have void return type; valid return types: string, Status, resposne, IOD objects");

                handler_ = [f = std::move(f)](const Request &req, Response &res, Args ... args) {
                    res = Response(f(req, args...));
                    res.end();
                };
            }

            template<typename Func>
            typename std::enable_if<
                    !magic::CallHelper<Func, magic::S<Args...>>::value &&
                    !magic::CallHelper<Func, magic::S<Request, Args...>>::value,
                    void>::type
            operator()(Func &&f) {
                static_assert(magic::CallHelper<Func, magic::S<Args...>>::value ||
                              magic::CallHelper<Func, magic::S<Request, Args...>>::value ||
                              magic::CallHelper<Func, magic::S<Request, Response &, Args...>>::value,
                              "Handler type is mismatched with URL parameters");
                static_assert(std::is_same<void, decltype(f(std::declval<Request>(), std::declval<Response &>(),
                                                            std::declval<Args>()...))>::value,
                              "Handler function with Response argument should have void return type");

                handler_ = std::move(f);
            }

            template<typename Func>
            void operator()(std::string name, Func &&f) {
                name_ = std::move(name);
                (*this).template operator()<Func>(std::forward(f));
            }

            void handle(const Request &req, Response &res, const suil::detail::routing_params &params) override {
                detail::routing_handler_call_helper::call <
                detail::routing_handler_call_helper::call_params <
                decltype(handler_) > ,
                        0, 0, 0, 0, magic::S<Args...>,
                        magic::S<>>
                ()(detail::routing_handler_call_helper::call_params <
                   decltype(handler_) >
                   {handler_, params, req, res});
            }

        private:
            std::function<void(const Request &, Response &, Args...)>
            handler_;
        };

        const int RULE_SPECIAL_REDIRECT_SLASH = 1;

        class Trie
        {
        public:
            struct node_t
            {
                unsigned rule_index{};
                std::array<unsigned, (int)suil::detail::ParamType::MAX> param_childrens{};
                std::unordered_map<std::string, unsigned> children;

                bool issimple() const
                {
                    return
                            !rule_index &&
                            std::all_of(
                                    std::begin(param_childrens),
                                    std::end(param_childrens),
                                    [](unsigned x){ return !x; });
                }
            };

            Trie()
                : m_nodes(1)
            {}

        private:
            void optimize_node(node_t* node);

            void optimize()
            {
                optimize_node(head());
            }

        public:

            void validate()
            {
                if (!head()->issimple())
                    throw std::runtime_error("Internal error: Trie header should be simple!");
                optimize();
            }

            suil::http::router_params_t find(
                    const strview& req_url,
                    const node_t* node = nullptr,
                    unsigned pos = 0,
                    suil::detail::routing_params* params = nullptr) const;

            void add(const std::string& url, unsigned rule_index);

        private:

            void debug_node_print(std::string& dbpr, node_t* n, int level);

        public:

            void debug_print()
            {
                std::string dbpr;
                dbpr.append("\n");
                debug_node_print(dbpr, head(), 0);
                sdebug("%s", dbpr.c_str());
            }

        private:
            const node_t* head() const
            {
                return &m_nodes.front();
            }

            node_t* head()
            {
                return &m_nodes.front();
            }

            unsigned new_node()
            {
                m_nodes.resize(m_nodes.size()+1);
                return m_nodes.size() - 1;
            }

            std::vector<node_t> m_nodes;
        };

        define_log_tag(HTTP_ROUTER);
        class Router : LOGGER(HTTP_ROUTER)
        {
        public:
            Router(std::string& base)
                : m_rules(2),
                  api_base((base=="/")? "": base)
            {}

            DynamicRule& new_rule_dynamic(const std::string& rule);

            template <uint64_t N>
            typename suil::magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
            {
                using rule_t = typename suil::magic::arguments<N>::type::template rebind<TaggedRule>;
                auto rule_obj = new rule_t(rule);

                internal_add_rule_object(rule, rule_obj);

                return *rule_obj;
            }

            void internal_add_rule_object(const std::string& rule, BaseRule* rule_obj);

            void validate();

            void handle(const Request& req, Response& res);

            void debug_print() {
                m_trie.debug_print();
            }

            inline const std::string& apiBaseRoute() const {
                return api_base;
            }

        private:
            template <typename H, typename... Mws>
            friend struct Connection;
            void before(Request& req, Response& resp);
            std::vector<std::unique_ptr<BaseRule>> m_rules{};
            Trie m_trie;
            std::string api_base{""};
        };

        struct SystemAttrs {
            struct Context{
            };

            void before(Request& req, Response&, Context&);

            void after(Request&, http::Response&, Context&);

            template<typename T>
            void configure(T& opts) {
                cookies = opts.get(sym(cookies), false);
            }

            template <typename... Opts>
            void setup(Opts... args) {
                auto opts = iod::D(args...);
                configure(opts);
            }

        private:
            bool cookies{false};
        };
    }
}

#endif //SUIL_ROUTES_HPP
