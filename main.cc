#include "core/app-template.hh"
#include "core/reactor.hh"
#include "core/future-util.hh"
#include "util/log.hh"
#include <core/sharded.hh>
#include "core/fstream.hh"

#include <iostream>
#include <stdexcept>

#include "database.hh"

seastar::logger startlog("init");

static seastar::future<> disk_sanity(seastar::sstring path) {
    return check_direct_io_support(path).then([] {
        return seastar::make_ready_future<>();
    }).handle_exception([path](auto ep) {
        startlog.error("Could not access {}: {}", path, ep);
        return seastar::make_exception_future<>(ep);
    });
};

static void tcp_syncookies_sanity() {
    try {
        auto f = seastar::file_desc::open("/proc/sys/net/ipv4/tcp_syncookies", O_RDONLY | O_CLOEXEC);
        char buf[128] = {};
        f.read(buf, 128);
        if (seastar::sstring(buf) == "0\n") {
            startlog.warn("sysctl entry net.ipv4.tcp_syncookies is set to 0.\n"
                          "For better performance, set following parameter on sysctl is strongly recommended:\n"
                          "net.ipv4.tcp_syncookies=1");
        }
    } catch (const std::system_error& e) {
            startlog.warn("Unable to check if net.ipv4.tcp_syncookies is set {}", e);
    }
}

void init_storage_service(seastar::sharded<database>& db) {
    
}

/*
 * dead code
 
                         return seastar::open_file_dma(name, seastar::open_flags::wo | seastar::open_flags::create).then([buf = std::move(buf)] (auto f) {
                            auto fout = seastar::make_file_output_stream(std::move(f));
                            return fout.write(buf).then([&fout] {
                                fout.flush();
                            }).then([&fout] {
                                fout.close();
                            });
                        / * echo
                        return out.write(std::move(buf)).then([&out] {
                            return out.flush();
                        * /
                        }).then([] {
                            return seastar::stop_iteration::no;
                        });

*/

void test() {
    auto name = seastar::sprint("dump-%d", seastar::engine().cpu_id());
    auto file_future = seastar::open_file_dma(name, seastar::open_flags::wo | seastar::open_flags::create).then([] (auto f) {
    });

}

seastar::future<> handle_connection(seastar::connected_socket s,
                                    seastar::socket_address a) {
    auto out = s.output();
    auto in = s.input();
    return seastar::do_with(std::move(s), std::move(out), std::move(in),
        [] (auto& s, auto& out, auto& in) {
            return seastar::repeat([&out, &in] {
                return in.read().then([&out] (auto buf) {
                    if (buf) {
                        return seastar::do_with(std::move(buf), [&out] (auto &buf) {
                            auto name = seastar::sprint("dump-%d", seastar::engine().cpu_id());
                            return seastar::open_file_dma(name, seastar::open_flags::wo | seastar::open_flags::create).then([&buf, &out] (auto f) {
                                auto fout = seastar::make_file_output_stream(std::move(f));
                                return seastar::do_with(std::move(fout), [&buf, &out] (auto& fout) {
                                    return out.write(buf.share()).then([&] {
                                        return fout.write(buf.get(), buf.size());
                                    }).then([&out] {
                                        return out.flush();
                                    }).then([&] {
                                        return fout.flush();
                                    });
                                    // return out.write(std::move(buf));
                                });
                            });
                        }).then([&out] {
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
    startlog.info("Starting on CPU {}", seastar::engine().cpu_id());
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
    seastar::sharded<database> db;
    try {
        app.run(argc, argv, [&db] {
            startlog.info("Starting GranpaK server...");
            tcp_syncookies_sanity();
            return disk_sanity(".").then([&db] {
                init_storage_service(db);
            }).then([] {
                return seastar::parallel_for_each(boost::irange<unsigned>(0, seastar::smp::count),
                        [] (unsigned c) {
                    return seastar::smp::submit_to(c, service_loop);
                });
            }).handle_exception([](auto ep) {
                startlog.error("Terminating due to a severe error: {}", ep);
                return seastar::make_ready_future<>();
            });
        });
    } catch(...) {
        std::cerr << "Failed to start application: "
                  << std::current_exception() << "\n";
        return 1;
    }
    return 0;
}
