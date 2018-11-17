//
// Created by dc on 12/11/18.
//

#include "init.h"
#include "net.h"

namespace suil {

    void server_handler::operator()(suil::SocketAdaptor &sock, void *) {
        //sinfo("received Connection from: %s", sock.id());
        sock.send(version::SWNAME);
        sock.send(" - ");
        sock.send(version::STRING);
        sock.send("\n");
        sock.flush();
    }
}