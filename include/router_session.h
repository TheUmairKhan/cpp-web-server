#ifndef ROUTER_SESSION_H
#define ROUTER_SESSION_H

#include "session.h"
#include "router.h"

/*  A session that consults a Router to decide which RequestHandler
    should generate a Response. */
class RouterSession : public session {
public:
    RouterSession(boost::asio::io_service& io_service, Router& router);

    /* Keep the same factory helper you used in _main.cc */
    static RouterSession* Make(boost::asio::io_service& io, Router& r) {
        return new RouterSession(io, r);
    }

    /* override */ void handle_read(const boost::system::error_code& err,
                                    std::size_t bytes_transferred) override;

private:
    Router& router_;
};

#endif  // ROUTER_SESSION_H
