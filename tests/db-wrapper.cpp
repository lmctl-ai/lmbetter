#include "db.hpp"
#include <iostream>
#include <stdexcept>
#include <utility>

static void require(bool value) { if (!value) throw std::runtime_error("DB wrapper assertion failed"); }
int main(int argc, char **argv) {
  try {
    require(argc == 2);
    lmok::Database db(std::filesystem::u8path(argv[1]), lmok::Database::Mode::Create);
    try {
      lmok::Transaction rollback(db);
      db.exec("INSERT INTO runs VALUES(1,1,'rolled back')");
      // Simulate a failed per-file import; destructor must roll back all writes.
      db.exec("INSERT INTO no_such_table VALUES(1)");
    } catch (const lmok::DbError &) {}
    auto count = db.prepare("SELECT count(*) FROM runs");
    require(count.step() && count.integer(0) == 0 && !count.step());
    {
      lmok::Transaction commit(db);
      auto insert = db.prepare("INSERT INTO runs VALUES(?,?,?)");
      insert.bind(1, int64_t(1)); insert.bind(2, int64_t(10)); insert.bind(3, std::string("a\0b", 3));
      require(!insert.step());
      insert.reset();
      insert.bind(1, int64_t(2)); insert.bind(2, int64_t(11)); insert.bind(3, std::string("second"));
      require(!insert.step());
      commit.commit();
    }
    auto query = db.prepare("SELECT command FROM runs ORDER BY id");
    auto moved = std::move(query);
    require(moved.step() && moved.text(0) == std::string("a\0b", 3));
    require(moved.step() && moved.text(0) == "second" && !moved.step());
    auto null = db.prepare("SELECT ?"); null.bind_null(1);
    require(null.step() && null.is_null(0));
    auto options = db.prepare("SELECT sqlite_version(), sqlite_compileoption_used('ENABLE_FTS5'), sqlite_compileoption_used('THREADSAFE=1')");
    require(options.step() && options.text(0) == "3.54.1" && options.integer(1) == 1 && options.integer(2) == 1);
    auto fk = db.prepare("PRAGMA foreign_keys"); require(fk.step() && fk.integer(0) == 1);
    bool rejected = false;
    try { db.exec("INSERT INTO symbols VALUES(1,999,'x','function',0,0,0,1,'','exact')"); }
    catch (const lmok::DbError &) { rejected = true; }
    require(rejected);
    std::cout << "DB wrapper: rollback, commit, bindings/reset/move/null, foreign keys\n";
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
