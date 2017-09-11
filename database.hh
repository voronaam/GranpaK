#ifndef DATABASE_HH_
#define DATABASE_HH_

#include "core/reactor.hh"
#include "core/future-util.hh"

class database {
public:
    database();
    void set_writer(std::unique_ptr<void>);
    seastar::future<> stop() {
        return seastar::make_ready_future<>();
    };
};

#endif /* DATABASE_HH_ */
