#include <suil/sys.hpp>
#include <suil/cmdl.hpp>
#include <suil/http/endpoint.hpp>
#include <suil/http/wsock.hpp>

using namespace suil;

int main(int argc, char *argv[])
{
    suil::init();
    log::setup(opt(verbose, 1));

    // create a new HTTP endpoint listening on port 80
    http::TcpEndpoint<> ep("/api",
                        opt(name, "0.0.0.0"),
                        opt(port, 1080));

    struct User {
        zcstring id;
        zcstring username;
    };

    typedef decltype(iod::D(
        prop(username,  zcstring),
        prop(data,      zcstring)
    )) HistoryEntry;

    std::deque<std::string> history;
    auto appendHistory = [&history](std::string&& he) {
        if (history.size() > 64) {
            // keep a short history
            history.pop_back();
        }
        // add as latest message
        history.push_front(he);
    };

    // We need to create a web socket API
    http::WebSockApi wsApi = http::ws_api(
        var(onConnect) = [&](http::WebSock& ws) -> bool {
            // handle client connection
            auto user = ws.data<User>();
            auto msg = utils::catstr(user->username, " joined chat room");
            sdebug("%s", msg());

            ws.broadcast(msg.data(), msg.size(), http::WsOp::TEXT);
            for (auto& he : history) {
                // send the entry to the joined user
                ws.send(he.c_str(), he.size(), http::WsOp::TEXT);
            }
            return true;
        },

        var(onMessage) = [&](http::WebSock& ws, const zbuffer& data, http::WsOp) {
            // prepare message
            auto user = ws.data<User>();
            zcstring msg = zcstring{data.data(), data.size(), false};
            HistoryEntry he{user->username.peek(), msg};
            auto entry = json::encode(he);
            // send message to other connected clients
            ws.broadcast(entry.c_str(), entry.size(), http::WsOp::TEXT);
            appendHistory(std::move(entry));
        },

        var(onClose) = [&](http::WebSock& ws) {
            // handle web socket close
            auto user = ws.data<User>();
            auto msg = utils::catstr(user->username, " left chat room");

            sdebug("%s", msg());
            ws.broadcast(msg);
        }
    );

    // add an http route on which web sockets will connect
    ep("/chat/<string>")
    ("GET"_method)
    ([&](const http::Request& req, http::Response& resp, std::string username) {
        // start the websocket handshake
        http::ws_handshake<User>(req, resp, wsApi, [username](http::WebSock& ws) {
            // configure the websocket user
            auto user = ws.data<User>();
            user->username = zcstring(username).dup();
        });
    });

    ep("/chat")
    ("GET"_method)
    ([]{
        // return the HTML chat page
        return utils::fs::readall(
                CHAT_HTML, true);
    });


    return ep.start();
}