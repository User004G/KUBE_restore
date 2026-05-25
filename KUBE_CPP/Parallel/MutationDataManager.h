#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <sqlite3.h>
#include <string>
#include <vector>

// Vollständige Definition von EAParams benötigt (für std::vector)
#include "../MutationsManager.h"

// Klasse zur Verwaltung der Fitness-Daten und Bereitstellung für den SelectionManager
class MutationDataManager
{
public:
    /**
     * Constructor
     * @param db_experts_path Path zu KUBE_Experts.db
     * @param db_mutation_path Path zu KUBE_Mutation.db
     */
    MutationDataManager(const std::string& db_experts_path, const std::string& db_mutation_path);

    // Destruktor
    ~MutationDataManager();

    /**
     * Initialisiert den Datenmanager, lädt notwendige Daten
     * @return true, wenn erfolgreich
     */
    bool init();

    /**
     * Gibt die Fitness-Daten aus der Fitness_proBar-Tabelle zurück
     * @param out_data Container für die geladenen Fitness-Daten
     * @return true, wenn erfolgreich
     */
    bool load_fitness_data(std::vector<EAParams>& out_data);

private:
    // Datenbank-Verbindungen
    sqlite3* db_experts_;
    sqlite3* db_mutation_;

    // Pfade
    std::string db_experts_path_;
    std::string db_mutation_path_;

    // Helferfunktionen für DB-Verwaltung
    bool open_db();
    void close_db();
};
