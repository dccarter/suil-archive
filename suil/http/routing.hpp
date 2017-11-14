//
// Created by dc on 04/04/17.
//

#ifndef SUIL_ROUTES_HPP
#define SUIL_ROUTES_HPP

#include <suil/http/request.hpp>
#include <suil/http/response.hpp>

namespace suil {

    namespace http {

        class base_rule_t {
        public:
            base_rule_t(std::string rule)
                : rule_(std::move(rule))
            {}

            virtual ~base_rule_t() = default;

            virtual void validate() = 0;

            virtual void handle(const request&,
                                response&,
                                const suil::detail::routing_params &) = 0;

            uint32_t get_methods() {
                return methods_;
            }

            friend class router_t;
            route_attributes_t attrs_{false, false, false, false};

        protected:
            uint32_t methods_{1 << (uint16_t) method_t::Get};

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
                    const request &req;
                    response &res;
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
                            !std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request &>::value, int>::type = 0) {
                        handler_ = (
                                [f = std::move(f)]
                                        (const request &, response &res, Args... args) {
                                    res = response(f(args...));
                                    res.end();
                                });
                    }

                    template<typename Req, typename ... Args>
                    struct req_handler_wrapper {
                        req_handler_wrapper(Func f)
                                : f(std::move(f)) {
                        }

                        void operator()(const request &req, response &res, Args... args) {
                            res = response(f(req, args...));
                            res.end();
                        }

                        Func f;
                    };

                    template<typename ... Args>
                    void set(Func f, typename std::enable_if<
                            std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request &>::value &&
                            !std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, response &>::value, int>::type = 0) {
                        handler_ = req_handler_wrapper<Args...>(std::move(f));
                        /*handler_ = (
                            [f = std::move(f)]
                            (const request& req, response& res, Args... args){
                                 res = response(f(req, args...));
                                 res.end();
                            });*/
                    }

                    template<typename ... Args>
                    void set(Func f, typename std::enable_if<
                            std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request &>::value &&
                            std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, response &>::value, int>::type = 0) {
                        handler_ = std::move(f);
                    }

                    template<typename ... Args>
                    struct handler_type_helper {
                        using type = std::function<void(const request &, response &, Args...)>;
                        using args_type = magic::S<typename magic::promote_t<Args>...>;
                    };

                    template<typename ... Args>
                    struct handler_type_helper<const request &, Args...> {
                        using type = std::function<void(const request &, response &, Args...)>;
                        using args_type = magic::S<typename magic::promote_t<Args>...>;
                    };

                    template<typename ... Args>
                    struct handler_type_helper<const request &, response &, Args...> {
                        using type = std::function<void(const request &, response &, Args...)>;
                        using args_type = magic::S<typename magic::promote_t<Args>...>;
                    };

                    typename handler_type_helper<ArgsWrapped...>::type handler_;

                    void operator()(const request &req, response &res, const suil::detail::routing_params &params) {
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

            self_t& methods(method_t method)
            {
                ((self_t*)this)->methods_ = 1 << (int)method;
                return (self_t&)*this;
            }

            template <typename ... MethodArgs>
            self_t& methods(method_t method, MethodArgs ... args_method)
            {
                methods(args_method...);
                ((self_t*)this)->methods_ |= 1 << (int)method;
                return (self_t&)*this;
            }
        };

        class dynamic_rule : public base_rule_t, public rule_parameter_traits<dynamic_rule>
        {
        public:

            dynamic_rule(std::string rule)
                : base_rule_t(std::move(rule))
            {
            }

            void validate() override
            {
                if (!erased_handler_)
                {
                    throw std::runtime_error((name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_).c_str());
                }
            }

            void handle(const request& req, response& res, const suil::detail::routing_params& params) override
            {
                erased_handler_(req, res, params);
            }

            dynamic_rule&operator()(method_t m) {
                return methods(m);
            }

            template <typename... _Methods>
            dynamic_rule& operator()(method_t m, _Methods... ms) {
                return methods(m, ms...);
            }

            template <typename... __Opts>
            dynamic_rule& attrs(__Opts... opts) {
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

            // enable_if Arg1 == request && Arg2 == response
            // enable_if Arg1 == request && Arg2 != resposne
            // enable_if Arg1 != request
            template <typename Func, unsigned ... Indices>
            std::function<void(const request&, response&, const suil::detail::routing_params&)>
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
            std::function<void(const request&, response&, const suil::detail::routing_params&)> erased_handler_;

        };

        template <typename ... Args>
        class tagged_rule : public base_rule_t, public rule_parameter_traits<tagged_rule<Args...>> {
        public:
            using self_t = tagged_rule<Args...>;

            tagged_rule(std::string rule)
                : base_rule_t(std::move(rule))
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
                              magic::CallHelper<Func, magic::S<request, Args...>>::value,
                              "Handler type is mismatched with URL parameters");
                static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value,
                              "Handler function cannot have void return type; valid return types: string, int, resposne, json object");

                handler_ = [f = std::move(f)](const request &, response &res, Args ... args) {
                    res = response(f(args...));
                    res.end();
                };
            }

            self_t &operator()(method_t m) {
                return rule_parameter_traits<tagged_rule< Args...>>::methods(m);
            }

            template<typename... _Methods>
            self_t &operator()(method_t m, _Methods... ms) {
                return rule_parameter_traits<tagged_rule<Args...>>::methods(m, ms...);
            }

            template <typename... __Opts>
            self_t& attrs(__Opts... opts) {
                /* apply configuration options */
                utils::apply_config(attrs_, opts...);
                return *this;
            }

            template<typename Func>
            typename std::enable_if<
                    !magic::CallHelper<Func, magic::S<Args...>>::value &&
                    magic::CallHelper<Func, magic::S<request, Args...>>::value,
                    void>::type
            operator()(Func &&f) {
                static_assert(magic::CallHelper<Func, magic::S<Args...>>::value ||
                              magic::CallHelper<Func, magic::S<request, Args...>>::value,
                              "Handler type is mismatched with URL parameters");
                static_assert(!std::is_same<void, decltype(f(std::declval<request>(), std::declval<Args>()...))>::value,
                              "Handler function cannot have void return type; valid return types: string, Status, resposne, IOD objects");

                handler_ = [f = std::move(f)](const request &req, response &res, Args ... args) {
                    res = response(f(req, args...));
                    res.end();
                };
            }

            template<typename Func>
            typename std::enable_if<
                    !magic::CallHelper<Func, magic::S<Args...>>::value &&
                    !magic::CallHelper<Func, magic::S<request, Args...>>::value,
                    void>::type
            operator()(Func &&f) {
                static_assert(magic::CallHelper<Func, magic::S<Args...>>::value ||
                              magic::CallHelper<Func, magic::S<request, Args...>>::value ||
                              magic::CallHelper<Func, magic::S<request, response &, Args...>>::value,
                              "Handler type is mismatched with URL parameters");
                static_assert(std::is_same<void, decltype(f(std::declval<request>(), std::declval<response &>(),
                                                            std::declval<Args>()...))>::value,
                              "Handler function with response argument should have void return type");

                handler_ = std::move(f);
            }

            template<typename Func>
            void operator()(std::string name, Func &&f) {
                name_ = std::move(name);
                (*this).template operator()<Func>(std::forward(f));
            }

            void handle(const request &req, response &res, const suil::detail::routing_params &params) override {
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
            std::function<void(const request &, response &, Args...)>
            handler_;
        };

        const int RULE_SPECIAL_REDIRECT_SLASH = 1;

        class trie_t
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

            trie_t()
                : nodes_(1)
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
                    const strview_t& req_url,
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
                return &nodes_.front();
            }

            node_t* head()
            {
                return &nodes_.front();
            }

            unsigned new_node()
            {
                nodes_.resize(nodes_.size()+1);
                return nodes_.size() - 1;
            }

            std::vector<node_t> nodes_;
        };

        define_log_tag(HTTP_ROUTER);
        class router_t : LOGGER(dtag(HTTP_ROUTER))
        {
        public:
            router_t(std::string& base)
                : rules_(2),
                  api_base(base)
            {}

            dynamic_rule& new_rule_dynamic(const std::string& rule);

            template <uint64_t N>
            typename suil::magic::arguments<N>::type::template rebind<tagged_rule>& new_rule_tagged(const std::string& rule)
            {
                using rule_t = typename suil::magic::arguments<N>::type::template rebind<tagged_rule>;
                auto rule_obj = new rule_t(rule);

                internal_add_rule_object(rule, rule_obj);

                return *rule_obj;
            }

            void internal_add_rule_object(const std::string& rule, base_rule_t* rule_obj);

            void validate();

            void handle(const request& req, response& res);

            void debug_print() {
                trie_.debug_print();
            }

        private:
            template <typename __H, typename... __Mws>
            friend struct connection;
            void before(request& req, response& resp);
            std::vector<std::unique_ptr<base_rule_t>> rules_{};
            trie_t trie_;
            std::string api_base{""};
        };

        struct system_attrs {
            struct Context{
            };

            void before(request& req, response&, Context&);

            void after(request&, http::response&, Context&);

            template<typename __T>
            void configure(__T& opts) {
                cookies = opts.get(sym(cookies), false);
            }

            template <typename...__Opts>
            void setup(__Opts... args) {
                auto opts = iod::D(args...);
                configure(opts);
            }

        private:
            bool cookies{false};
        };
    }
}

#endif //SUIL_ROUTES_HPP
