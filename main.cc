#include "core/app-template.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include <iostream>
#include <stdexcept>

seastar::future<> handle_connection(seastar::connected_socket s,
                                    seastar::socket_address a) {
    auto out = s.output();
    auto in = s.input();
    return seastar::do_with(std::move(s), std::move(out), std::move(in),
        [] (auto& s, auto& out, auto& in) {
            return seastar::repeat([&out, &in] {
                return in.read().then([&out] (auto buf) {
                    if (buf) {
                        return out.write(std::move(buf)).then([&out] {
                            return out.flush();
                        }).then([] {
                            return seastar::stop_iteration::no;
                        });
                    } else {
                        return seastar::make_ready_future<seastar::stop_iteration>(
                            seastar::stop_iteration::yes);
                    }
                });
            }).then([&out] {
                return out.close();
            });
        });
}

seastar::future<> service_loop() {
    seastar::listen_options lo;
    lo.reuse_address = true;
    return seastar::do_with(seastar::listen(seastar::make_ipv4_address({1234}), lo),
            [] (auto& listener) {
        return seastar::keep_doing([&listener] () {
            return listener.accept().then(
                [] (seastar::connected_socket s, seastar::socket_address a) {
                    handle_connection(std::move(s), std::move(a));
                });
        });
    });
}

int main(int argc, char** argv) {
    seastar::app_template app;
    try {
        app.run(argc, argv, [] {
            return seastar::parallel_for_each(boost::irange<unsigned>(0, seastar::smp::count),
                    [] (unsigned c) {
                return seastar::smp::submit_to(c, service_loop);
            });
        });
    } catch(...) {
        std::cerr << "Failed to start application: "
                  << std::current_exception() << "\n";
        return 1;
    }
    return 0;
}
