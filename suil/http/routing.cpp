//
// Created by dc on 28/06/17.
//

#include <suil/http/routing.hpp>

namespace suil {

    namespace http {

        void trie_t::optimize_node(node_t *n) {
            for(auto x : n->param_childrens)
            {
                if (!x)
                    continue;
                node_t* child = &nodes_[x];
                optimize_node(child);
            }
            if (n->children.empty())
                return;
            bool merge_with_child = true;
            for(auto& kv : n->children)
            {
                node_t* child = &nodes_[kv.second];
                if (!child->issimple())
                {
                    merge_with_child = false;
                    break;
                }
            }
            if (merge_with_child)
            {
                decltype(n->children) merged;
                for(auto& kv : n->children)
                {
                    node_t* child = &nodes_[kv.second];
                    for(auto& child_kv : child->children)
                    {
                        merged[kv.first + child_kv.first] = child_kv.second;
                    }
                }
                n->children = std::move(merged);
                optimize_node(n);
            }
            else
            {
                for(auto& kv : n->children)
                {
                    node_t* child = &nodes_[kv.second];
                    optimize_node(child);
                }
            }
        }

        suil::http::router_params_t trie_t::find(
                const strview_t &req_url,
                const node_t *node,
                unsigned int pos,
                suil::detail::routing_params *params) const
        {

            suil::detail::routing_params empty;
            if (params == nullptr)
                params = &empty;

            unsigned found{};
            suil::detail::routing_params match_params;

            if (node == nullptr)
                node = head();
            if (pos == req_url.size())
                return {node->rule_index, std::move(*params)};

            auto update_found = [&found, &match_params](router_params_t& ret)
            {
                if (ret.first && (!found || found > ret.first))
                {
                    found = ret.first;
                    match_params = std::move(ret.second);
                }
            };

            if (node->param_childrens[(int)suil::detail::ParamType::INT])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-')
                {
                    char* eptr;
                    errno = 0;
                    int64_t value = (int64_t)strtoll(req_url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->push(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)suil::detail::ParamType::INT]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->pop(value);
                    }
                }
            }

            if (node->param_childrens[(int)suil::detail::ParamType::UINT])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+')
                {
                    char* eptr;
                    errno = 0;
                    uint64_t value = (uint64_t) strtoull(req_url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->push(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)suil::detail::ParamType::UINT]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->pop(value);
                    }
                }
            }

            if (node->param_childrens[(int)suil::detail::ParamType::DOUBLE])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
                {
                    char* eptr;
                    errno = 0;
                    double value = strtod(req_url.data()+pos, &eptr);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->push(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)suil::detail::ParamType::DOUBLE]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->pop(value);
                    }
                }
            }

            if (node->param_childrens[(int)suil::detail::ParamType::STRING])
            {
                size_t epos = pos;
                for(; epos < req_url.size(); epos ++)
                {
                    if (req_url[epos] == '/')
                        break;
                }

                if (epos != pos)
                {
                    auto sv = req_url.substr(pos, epos-pos);
                    params->push(sv);
                    auto ret = find(req_url, &nodes_[node->param_childrens[(int)suil::detail::ParamType::STRING]], epos, params);
                    update_found(ret);
                    params->pop(sv);
                }
            }

            if (node->param_childrens[(int)suil::detail::ParamType::PATH])
            {
                size_t epos = req_url.size();

                if (epos != pos)
                {
                    auto sv = req_url.substr(pos, epos-pos);
                    params->push(sv);
                    auto ret = find(req_url, &nodes_[node->param_childrens[(int)suil::detail::ParamType::PATH]], epos, params);
                    update_found(ret);
                    params->pop(sv);
                }
            }

            for(auto& kv : node->children)
            {
                const std::string& fragment = kv.first;
                const node_t* child = &nodes_[kv.second];

                if (req_url.compare(pos, fragment.size(), fragment) == 0)
                {
                    auto ret = find(req_url, child, pos + fragment.size(), params);
                    update_found(ret);
                }
            }

            return {found, std::move(match_params)};
        }

        void trie_t::add(const std::string &url, unsigned rule_index) {
            unsigned idx{0};

            for(unsigned i = 0; i < url.size(); i ++)
            {
                char c = url[i];
                if (c == '<')
                {
                    static struct ParamTraits
                    {
                        suil::detail::ParamType type;
                        std::string name;
                    } paramTraits[] =
                    {
                            { suil::detail::ParamType::INT, "<int>" },
                            { suil::detail::ParamType::UINT, "<uint>" },
                            { suil::detail::ParamType::DOUBLE, "<float>" },
                            { suil::detail::ParamType::DOUBLE, "<double>" },
                            { suil::detail::ParamType::STRING, "<str>" },
                            { suil::detail::ParamType::STRING, "<string>" },
                            { suil::detail::ParamType::PATH, "<path>" },
                    };

                    for(auto& x:paramTraits)
                    {
                        if (url.compare(i, x.name.size(), x.name) == 0)
                        {
                            if (!nodes_[idx].param_childrens[(int)x.type])
                            {
                                auto new_node_idx = new_node();
                                nodes_[idx].param_childrens[(int)x.type] = new_node_idx;
                            }
                            idx = nodes_[idx].param_childrens[(int)x.type];
                            i += x.name.size();
                            break;
                        }
                    }

                    i --;
                }
                else
                {
                    std::string piece(&c, 1);
                    if (!nodes_[idx].children.count(piece))
                    {
                        auto new_node_idx = new_node();
                        nodes_[idx].children.emplace(piece, new_node_idx);
                    }
                    idx = nodes_[idx].children[piece];
                }
            }
            if (nodes_[idx].rule_index)
                throw std::runtime_error(("handler already exists for " + url).c_str());
            nodes_[idx].rule_index = rule_index;
        }

        void trie_t::debug_node_print(std::string &dbpr, node_t *n, int level) {
            dbpr.reserve(512);
            for (int i = 0; i < (int) suil::detail::ParamType::MAX; i++) {
                if (n->param_childrens[i]) {
                    switch ((suil::detail::ParamType) i) {
                        case suil::detail::ParamType::INT:
                            dbpr.append("<int>\n");
                            break;
                        case suil::detail::ParamType::UINT:
                            dbpr.append("<uint>\n");
                            break;
                        case suil::detail::ParamType::DOUBLE:
                            dbpr.append("<float>\n");
                            break;
                        case suil::detail::ParamType::STRING:
                            dbpr.append("<str>\n");
                            break;
                        case suil::detail::ParamType::PATH:
                            dbpr.append("<path>\n");
                            break;
                        default:
                            dbpr.append("<ERROR>\n");
                            break;
                    }

                    debug_node_print(dbpr, &nodes_[n->param_childrens[i]], level + 1);
                }
            }
            for (auto &kv : n->children) {
                dbpr.append(2 * level, ' ');
                dbpr.append(kv.first.c_str());
                debug_node_print(dbpr, &nodes_[kv.second], level + 1);
            }
        }


        void router_t::validate() {
            trie_.validate();
            for(auto& rule : rules_)
            {
                if (rule)
                {
                    rule->validate();
                }
            }
        }

        dynamic_rule& router_t::new_rule_dynamic(const std::string &rule) {
            auto rule_obj = new dynamic_rule(rule);

            internal_add_rule_object(rule, rule_obj);

            return *rule_obj;
        }

        void router_t::internal_add_rule_object(const std::string &rule, base_rule_t *rule_obj) {
            rules_.emplace_back(rule_obj);
            trie_.add(rule, rules_.size() - 1);

            // directory case:
            //   Request to `/about' url matches `/about/' rule
            if (rule.size() > 1 && rule.back() == '/')
            {
                std::string rule_without_trailing_slash = rule;
                rule_without_trailing_slash.pop_back();
                trie_.add(rule_without_trailing_slash, RULE_SPECIAL_REDIRECT_SLASH);
            }
        }

        void router_t::before(request &req, response &resp) {
            char *url;
            zcstring tmp(req.url);
            if (tmp.empty() || (tmp == "/")) {
                // just use find as it
                static const char *ROOT_URL = "/";
                url = (char *) ROOT_URL;
            }
            else if (api_base.empty() || strncasecmp(req.url, api_base.c_str(), api_base.size()) == 0)
            {
                /* the request is an api` rule */
                size_t index = api_base.empty()? 0: api_base.size();
                url = &req.url[index];
            }
            else {
                const char *FS_URL = "/" SUIL_FILE_SERVER_ROUTE;
                url = (char *) FS_URL;
            }

            auto params = trie_.find(url);

            if (params.first == 0) {
                throw error::not_found();
            }

            if (params.first >= rules_.size())
                throw std::runtime_error("trie internal structure corrupted!");

            // update request with params
            req.params.index = params.first;
            req.params.decoded = std::move(params.second);
            req.params.attrs = &rules_[params.first]->attrs_;
        }

        void router_t::handle(const request &req, response &res) {

            if (req.params.index == RULE_SPECIAL_REDIRECT_SLASH)
            {
                res = response();

                // TODO absolute url building
                if (req.header("Host").empty())
                {
                    buffer_t b(0);
                    b += req.url;
                    b += "/";
                    res.header("Location", b);
                }
                else
                {
                    buffer_t b(16);
                    b += "http://";
                    b += req.header("Host");
                    b += req.url;
                    b += "/";
                    res.header("Location", b);
                }
                res.end(Status::MOVED_PERMANENTLY);
                return;
            }

            if ((rules_[req.params.index]->get_methods() & (1<<(uint32_t)req.method)) == 0)
            {
                throw  error::not_found();
            }

            // any uncaught exceptions become 500s
            try
            {
                rules_[req.params.index]->handle(req, res, req.params.decoded);
            }
            catch(http::exception& e)
            {
                throw std::move(e);
            }
            catch(...)
            {
                throw error::internal();
            }
        }

        void system_attrs::before(request &req, response &, Context &) {
            /* check for system attributes and apply appropriate actions */
            const route_attributes_t& attrs = req.route();
            if (cookies || attrs.PARSE_COOKIES) {
                /* parse cookies if route uses cookies */
                req.parse_cookies();
            }

            if (attrs.PARSE_FORM) {
                /* parse request form if route expects form */
                req.parse_form();
            }
        }

        void system_attrs::after(request &, http::response &, Context &) {
        }
    }
}