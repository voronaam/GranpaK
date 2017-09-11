#include "database.hh"

seastar::logger dblog("db");

database::database() {
    dblog.info("Starting DB on shard {}", seastar::engine().cpu_id());
    /*
    auto name = seastar::sprint("newdump-%d", seastar::engine().cpu_id());
    seastar::open_file_dma(name, seastar::open_flags::wo | seastar::open_flags::create).then([&] (auto f) {
        auto fout = seastar::make_file_output_stream(std::move(f));
        this->writer = std::move(fout);
    };
    */
};
