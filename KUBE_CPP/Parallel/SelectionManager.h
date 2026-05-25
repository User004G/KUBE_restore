#pragma once

#include <vector>
#include <string>

// Forward declaration to avoid circular dependency
struct sqlite3;
struct EAParams;

// Klasse zur Auswahl von Best und Worst EAs für die Mutation
class SelectionManager
{
public:
    SelectionManager();
    ~SelectionManager();

    bool init(const std::string& config_path, const std::string& db_experts_path);

    /**
     * Wählt die 'count' schlechtesten EAs basierend auf NetProfitNorm aus der DB.
     * @param count Anzahl der zu selektierenden EAs
     * @return Vektor mit Magic-Numbers
     */
    std::vector<int> select_worst_eas(int count);

    /**
     * Wählt die besten und schlechtesten EAs für die Mutation.
     * @param fitness_data Die Fitness-Daten der EAs
     * @param best_eas Container für die besten EAs (aktuell leer gelassen)
     * @param worst_eas Container für die schlechtesten EAs
     * @return true, wenn die Selektion erfolgreich war
     */
    bool select_best_and_worst(const std::vector<EAParams>& fitness_data,
        std::vector<EAParams>& best_eas,
        std::vector<EAParams>& worst_eas);


private:
    bool open_db();
    void close_db();

    std::string config_path_;
    std::string db_experts_path_;
    int mutation_lookback_ = 0;

    sqlite3* db_experts_ = nullptr;
};
