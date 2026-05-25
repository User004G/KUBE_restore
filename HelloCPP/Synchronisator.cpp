
// FitnessWriter.cpp — kontinuierlich, wartet bei fehlenden Bars/Takten
// Build (Beispiel):
// cl /std:c++17 FitnessWriter.cpp /I"path\\to\\sqlite\\include" /link "path\\to\\sqlite3.lib"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <regex>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <cstdarg>
// eigene Module
#include "FitnessDataManager.h"
#include "Aktivator.h"


// ============= Logger: stdout + Visual-Studio-Ausgabefenster =============
namespace logger {
    // [Logger-Implementierung unverändert]
    static std::mutex g_log_mtx;

    static void init_stdout_unbuffered() {
        static bool inited = false;
        if (!inited) {
            setvbuf(stdout, nullptr, _IONBF, 0);
            inited = true;
        }
    }

    static void vlogf(const char* fmt, va_list ap) {
        init_stdout_unbuffered();
        char msg[2048];
#ifdef _WIN32
        _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, ap);
#else
        vsnprintf(msg, sizeof(msg), fmt, ap);
#endif
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        tm tm_;
#ifdef _WIN32
        localtime_s(&tm_, &t);
        DWORD tid = GetCurrentThreadId();
#else
        localtime_r(&t, &tm_);
        unsigned long tid = 0;
#endif
        char prefix[128];
        std::snprintf(prefix, sizeof(prefix),
            "[%02d:%02d:%02d.%03d T%lu] ",
            tm_.tm_hour, tm_.tm_min, tm_.tm_sec,
            (int)ms.count(), (unsigned long)tid);

        std::string line = std::string(prefix) + msg;
        if (line.empty() || line.back() != '\n') line.push_back('\n');

        std::lock_guard<std::mutex> lock(g_log_mtx);
        std::fwrite(line.data(), 1, line.size(), stdout);

#ifdef _WIN32
        std::string crlf = line;
        for (size_t i = 0; i < crlf.size(); ++i) {
            if (crlf[i] == '\n' && (i == 0 || crlf[i - 1] != '\r')) {
                crlf.insert(i, "\r");
                ++i;
            }
        }
        OutputDebugStringA(crlf.c_str());
#endif
    }

    static void logf(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        vlogf(fmt, ap);
        va_end(ap);
    }
} // namespace logger

#define LOGF(...) ::logger::logf(__VA_ARGS__)

// ---
// ========================== App-Klasse HEADER =====================================
// ---



// 
class Synchronisator {
public:
    struct Config {
        std::string db_experts_path;
        std::string db_schwarm_path;
        std::string config_path;
        std::string timebeat_table = "timebeat_paper10";
        std::string swarm_table = "swarm";
        int swarm_id = 10;
        int poll_ms = 50; // Warteintervall für erneute Prüfungen
    };

    /**
     * Hauptlauflogik der Anwendung.
     */
    int run(int argc, char** argv);

private:
    /**
     * Sucht den neuesten abgeschlossenen Takt und markiert ihn atomar als erledigt.
     */
    static bool next_ready_ts(sqlite3* db, const std::string& table, std::string& out_ts);

    // ---- Pfade (Defaults) ----------------------------------------------------
    static std::string default_db_experts_path();
    static std::string default_db_schwarm_path();
    static std::string default_config_path();

    // ---- CLI -----------------------------------------------------------------
    static Config parseArgs(int argc, char** argv);

    // ---- Utils ---------------------------------------------------------------
    static void die(const std::string& msg, int rc = 1);
    static void sql_fail(sqlite3* db, const char* where);
    static void set_busy_timeout(sqlite3* db);
    static bool table_exists(sqlite3* db, const std::string& name);
    static void require_table(sqlite3* db, const std::string& name);

    // ---- Config lesen --------------------------------------------------------
    static int read_lookback(const std::string& path);
};

int Synchronisator::run(int argc, char** argv)
{
    Config cfg = parseArgs(argc, argv);
    if (cfg.db_experts_path.empty())       cfg.db_experts_path = default_db_experts_path();
    if (cfg.db_schwarm_path.empty())       cfg.db_schwarm_path = default_db_schwarm_path();
    if (cfg.config_path.empty())   cfg.config_path = default_config_path();
    if (cfg.db_experts_path.empty()) die("DB-Expert-Pfad leer (APPDATA nicht gefunden).");
    if (cfg.db_schwarm_path.empty()) die("DB-Schwarm-Pfad leer (APPDATA nicht gefunden).");

    LOGF("[CFG] DB: %s", cfg.db_experts_path.c_str());
    LOGF("[CFG] DB: %s", cfg.db_schwarm_path.c_str());
    LOGF("[CFG] Config: %s", cfg.config_path.c_str());
    LOGF("[CFG] Timebeat: %s | Swarm: %d | poll=%d ms",
        cfg.timebeat_table.c_str(), cfg.swarm_id, cfg.poll_ms);

    const int lookback = read_lookback(cfg.config_path);
    LOGF("[CFG] c_fitness_lookback_EAs = %d (Bars)", lookback);

    // Open DB und Table für Experts-timebeat
    sqlite3* db_experts = nullptr;
    if (sqlite3_open(cfg.db_experts_path.c_str(), &db_experts) != SQLITE_OK) sql_fail(db_experts, "open");
    set_busy_timeout(db_experts);

    require_table(db_experts, cfg.timebeat_table);

    // Open DB und Table für Experts-timebeat
    sqlite3* db_swarm = nullptr;
    if (sqlite3_open(cfg.db_schwarm_path.c_str(), &db_swarm) != SQLITE_OK) sql_fail(db_swarm, "open");
    set_busy_timeout(db_swarm);


    // Manager initialisieren
    FitnessDataManager fitness_manager(db_experts, "Balance_proBar", "Fitness_proBar", lookback);
    Aktivator          aktivator(db_experts, db_swarm, "Fitness_proBar", "swarms"); // gleiche DB-Verbindung

    std::string last_ts;
    for (;;)
    {
        std::string ts;
        if (next_ready_ts(db_experts, cfg.timebeat_table, ts))  // liefert nächsten ts_key mit row_complete=1 & cpp_done=0
        {
            if (ts != last_ts)
            {
                LOGF("[TIMEBEAT] Verarbeite ts=%s", ts.c_str());

                // 1) Fitness berechnen & schreiben
                bool ok = fitness_manager.compute_and_upsert_for_ts(ts, cfg.swarm_id, nullptr, false); // keine eigene TX
                if (!ok) {
                    LOGF("[FITNESS] Fehler bei compute/upsert für %s", ts.c_str());
                    last_ts = ts;
                    continue;
                }
                LOGF("[FITNESS] Fitness für %s gespeichert", ts.c_str());

                // nach Fitness-Write:
                const int source_swarm = cfg.swarm_id; // 10 (Paper)
                const int target_swarm = 0;            // Live in swarms
                const int magic_delta = -10000;       // falls Magics verschieden; sonst 0

                if (!aktivator.apply_for_ts(ts, source_swarm, target_swarm, magic_delta)) {
                    LOGF("[AKTIVATOR] apply_for_ts fehlgeschlagen für %s – cpp_done bleibt 0", ts.c_str());
                    last_ts = ts;
                    continue;
                }
                LOGF("[AKTIVATOR] aktiv-Flags aktualisiert (swarms.swarm=%d) für %s", target_swarm, ts.c_str());


                // 3) cpp_done=1 setzen (erst NACH erfolgreicher De/Aktivierung)
                const std::string q_update_cpp_done =
                    "UPDATE " + cfg.timebeat_table + " "
                    "SET cpp_done=1 "
                    "WHERE ts_key=? AND row_complete=1 AND cpp_done=0;";
                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db_experts, q_update_cpp_done.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                    LOGF("[ERROR] prepare cpp_done=1: %s", sqlite3_errmsg(db_experts));
                    if (stmt) sqlite3_finalize(stmt);
                    last_ts = ts;
                    continue;
                }
                sqlite3_bind_text(stmt, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    LOGF("[ERROR] update cpp_done=1: %s", sqlite3_errmsg(db_experts));
                }
                else {
                    LOGF("[TIMEBEAT] cpp_done=1 gesetzt für %s", ts.c_str());
                }
                sqlite3_finalize(stmt);

                last_ts = ts;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.poll_ms));
    }

    // (unerreichbar)
    // sqlite3_close(db_experts);
    // return 0;
}


bool Synchronisator::next_ready_ts(sqlite3* db_experts, const std::string& table, std::string& out_ts)
{
    // Atomare Read+Claim-Transaktion
    if (sqlite3_exec(db_experts, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[sqlite] BEGIN IMMEDIATE failed: %s\n", sqlite3_errmsg(db_experts));
        return false;
    }

    bool ok = false;
    sqlite3_stmt* st_sel = nullptr;

    // Neuesten vollständigen und noch nicht verarbeiteten Eintrag holen (SELECT)
    const std::string q_sel =
        "SELECT rowid, ts_key "
        "FROM " + table + " "
        "WHERE row_complete=1 AND cpp_done=0 "
        "ORDER BY ts_key DESC, rowid DESC "
        "LIMIT 1;";

    if (sqlite3_prepare_v2(db_experts, q_sel.c_str(), -1, &st_sel, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[sqlite] prepare(select) failed: %s\n", sqlite3_errmsg(db_experts));
        sqlite3_exec(db_experts, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    int rc = sqlite3_step(st_sel);
    if (rc == SQLITE_ROW) {
        const sqlite3_int64 rowid = sqlite3_column_int64(st_sel, 0);
        const unsigned char* t = sqlite3_column_text(st_sel, 1);
        const std::string ts = t ? reinterpret_cast<const char*>(t) : "";
        sqlite3_finalize(st_sel); st_sel = nullptr; // finalize hier

        // Erfolgreich geclaimed
        out_ts = ts;
        ok = true;
        if (sqlite3_exec(db_experts, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
            std::fprintf(stderr, "[sqlite] COMMIT failed: %s\n", sqlite3_errmsg(db_experts));
            sqlite3_exec(db_experts, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
    }
    else if (rc == SQLITE_DONE) {
        // Nichts Neues
        sqlite3_finalize(st_sel);
        sqlite3_exec(db_experts, "ROLLBACK;", nullptr, nullptr, nullptr);
        ok = false;
    }
    else {
        std::fprintf(stderr, "[sqlite] select step failed: %s\n", sqlite3_errmsg(db_experts));
        sqlite3_finalize(st_sel);
        sqlite3_exec(db_experts, "ROLLBACK;", nullptr, nullptr, nullptr);
        ok = false;
    }

    return ok;
}


// ---- Pfade (Defaults) ----------------------------------------------------

std::string Synchronisator::default_db_experts_path() {
#ifdef _WIN32
    char* appdata = nullptr; size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") != 0 || appdata == nullptr) return "";
    std::string p(appdata);
    free(appdata);
    p += "\\MetaQuotes\\Terminal\\Common\\Files\\KUBE_Experts.db";
    return p;
#else
    return "KUBE_Experts.db";
#endif
}

std::string Synchronisator::default_config_path() {
    return "C:\\MQL_Shared\\Includes\\Config1\\KUBE_config.mqh";
}
// ---- Pfade (Defaults) ----------------------------------------------------

std::string Synchronisator::default_db_schwarm_path() {
#ifdef _WIN32
    char* appdata = nullptr; size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") != 0 || appdata == nullptr) return "";
    std::string p(appdata);
    free(appdata);
    p += "\\MetaQuotes\\Terminal\\Common\\Files\\KUBE_Schwarm.db";
    return p;
#else
    return "KUBE_Schwarm.db";
#endif
}
//
//std::string Synchronisator::default_config_path() {
//    return "C:\\MQL_Shared\\Includes\\Config1\\KUBE_config.mqh";
//}

// ---- CLI -----------------------------------------------------------------

Synchronisator::Config Synchronisator::parseArgs(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) die("Fehlender Argumentwert.");
            return std::string(argv[++i]);
            };
        if (s == "--db")           c.db_experts_path = next();
        else if (s == "--config")  c.config_path = next();
        else if (s == "--swarm")   c.swarm_id = std::stoi(next());
        else if (s == "--timebeat") c.timebeat_table = next();
        else if (s == "--poll-ms")  c.poll_ms = std::stoi(next());
        else die("Unbekanntes Argument: " + s);
    }
    if (c.poll_ms < 50) c.poll_ms = 50; // vernünftiges Minimum
    return c;
}

// ---- Utils ---------------------------------------------------------------

void Synchronisator::die(const std::string& msg, int rc) {
    LOGF("[FATAL] %s", msg.c_str());
    std::fprintf(stderr, "%s\n", msg.c_str());
    std::exit(rc);
}

void Synchronisator::sql_fail(sqlite3* db, const char* where) {
    std::string m = std::string("[sqlite] ") + where + ": " + sqlite3_errmsg(db);
    die(m, 2);
}

void Synchronisator::set_busy_timeout(sqlite3* db) {
    if (sqlite3_exec(db, "PRAGMA busy_timeout=1000;", nullptr, nullptr, nullptr) != SQLITE_OK)
        sql_fail(db, "busy_timeout");
}

bool Synchronisator::table_exists(sqlite3* db, const std::string& name) {
    sqlite3_stmt* st = nullptr;
    const char* q =
        "SELECT 1 FROM sqlite_master WHERE type IN('table','view') AND name=?1 LIMIT 1;";
    if (sqlite3_prepare_v2(db, q, -1, &st, nullptr) != SQLITE_OK)
        sql_fail(db, "prepare exists");
    sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(st) == SQLITE_ROW);
    sqlite3_finalize(st);
    return ok;
}

void Synchronisator::require_table(sqlite3* db, const std::string& name) {
    if (!table_exists(db, name)) die("Benötigte Tabelle fehlt: " + name);
}

// ---- Config lesen --------------------------------------------------------

int Synchronisator::read_lookback(const std::string& path) {
    std::ifstream in(path);
    if (!in) die("Config nicht gefunden: " + path);

    std::string all(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());

    std::regex re(R"(c_fitness_lookback_EAs\s*=\s*([0-9]+))");
    std::smatch m;
    if (std::regex_search(all, m, re)) return std::stoi(m[1].str());
    die("c_fitness_lookback_EAs nicht in Config gefunden.");
    return 0;
}


// ---- main --------------------------------------------------------------------
int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    Synchronisator app;
    return app.run(argc, argv);
}