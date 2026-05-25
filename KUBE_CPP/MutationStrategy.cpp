// MutationStrategy.cpp
#include "MutationStrategy.h"

#include <sqlite3.h>
#include <cstdio>

// -------------------------------------------------------
// Helper der Basisklasse: Doppelungsprüfung in KUBE_Schwarm.db
// -------------------------------------------------------
bool IMutationStrategy::is_unique_param_triple(
    sqlite3* db_schwarm,
    int      swarm_id,
    int      p1,
    int      p2,
    int      p3
) const
{
    if (!db_schwarm)
    {
        std::fprintf(stderr,
            "[MutationStrategy] is_unique_param_triple: db_schwarm is null\n");
        // konservativ: lieber false, damit wir kein Unfug treiben
        return false;
    }

    const char* sql =
        "SELECT COUNT(*) "
        "FROM swarms "
        "WHERE swarm = ?1 "
        "  AND param1 = ?2 "
        "  AND param2 = ?3 "
        "  AND param3 = ?4;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_schwarm, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        std::fprintf(stderr,
            "[MutationStrategy] is_unique_param_triple: prepare failed: %s\n",
            sqlite3_errmsg(db_schwarm));
        return false;
    }

    sqlite3_bind_int(stmt, 1, swarm_id);
    sqlite3_bind_int(stmt, 2, p1);
    sqlite3_bind_int(stmt, 3, p2);
    sqlite3_bind_int(stmt, 4, p3);

    int count = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        count = sqlite3_column_int(stmt, 0);
    }
    else
    {
        std::fprintf(stderr,
            "[MutationStrategy] is_unique_param_triple: step failed (rc=%d)\n", rc);
    }

    sqlite3_finalize(stmt);

    // unique, wenn Count==0
    return (count == 0);
}
