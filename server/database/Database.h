#pragma once

#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace Server::DB
{
    struct User
    {
        int64_t discord_id;
        std::string riot_puuid;
        std::string riot_name;
        std::string riot_tag;
        std::string region;
        std::string last_match_id;
        double mult_upper = 1.0;
        double mult_lower = 1.0;
        double mult_core = 1.0;
    };

    struct ExerciseDefinition
    {
        int id;
        std::string name;
        int set_count;
        std::string type;
    };

    struct ExerciseQueueItem
    {
        int id;
        int64_t user_id;
        std::string match_id;
        std::string exercise_name;
        int reps;
        int original_deaths;
        std::string timestamp;
    };

    // New Struct for Rich Display
    struct PenanceDisplayInfo
    {
        int id; // Queue ID
        std::string match_id;
        std::string exercise_name;
        int reps;
        int original_deaths;

        // Game Stats
        std::string champion_name;
        int kills;
        int deaths;
        int assists;
        double kp_percent;
        int cs;
        double cs_min;
        int64_t game_timestamp; // Epoch MS
    };

    struct UserStats
    {
        int total_deaths;
        int total_games;
        double lowest_kda;
        int most_deaths_single;
        double avg_deaths_min;
        std::map<std::string, int> exercise_counts;
        std::vector<std::pair<std::string, int>> top_death_champs;
        int pending_penance_count;
    };

    class Database
    {
    public:
        Database(const std::string &dbPath);
        ~Database();

        void Initialize();

        // User Management
        void AddUser(const User &user);
        std::vector<User> GetDiscordUsers(int64_t discord_id);
        std::vector<User> GetAllUsers();
        void UpdateLastMatch(int64_t discord_id, const std::string &puuid, const std::string &match_id);

        // Multiplier Management
        void SetUserMultiplier(int64_t discord_id, double multiplier, const std::string &type = "");
        double GetUserMultiplier(int64_t discord_id, const std::string &type);

        // Exercise Management
        void SeedExercises(const std::vector<ExerciseDefinition> &exercises);
        std::vector<ExerciseDefinition> GetAllExercises();
        std::optional<ExerciseDefinition> GetRandomExercise();

        // Queue Management
        void AddToQueue(int64_t user_id, const std::string &match_id, const std::string &exercise, int reps, int deaths);
        std::vector<ExerciseQueueItem> GetPendingPenance(int64_t user_id);

        // Rich Displays
        std::vector<PenanceDisplayInfo> GetPendingPenanceDetailed(int64_t user_id);
        std::vector<PenanceDisplayInfo> GetRecentGames(int64_t user_id, int limit);
        std::vector<std::pair<std::string, int>> GetLeaderboard(const std::string& type);

        std::optional<ExerciseQueueItem> GetPenanceByGameID(int64_t user_id, const std::string &match_id);

        void CompletePenance(int64_t user_id, const std::string &match_id);
        void UpdatePenance(int row_id, const std::string &new_ex, int new_reps);

        // Stats & Logic
        bool IsMatchProcessed(int64_t discord_id, const std::string &match_id);

        void LogGame(int64_t user_id, const std::string &match_id, int64_t timestamp, int64_t gameDuration, const std::string &champ, int k, int d,
                     int a, double kp, int cs, double cs_min);
        UserStats GetUserStats(int64_t user_id);

    private:
        sqlite3 *m_db;
        std::mutex m_mutex;
        
        // Base Execute for raw SQL (migrations etc)
        void ExecuteSQL(const std::string &sql);

        // Binding Helpers
        void BindParameter(sqlite3_stmt *stmt, int index, int value)
        {
            sqlite3_bind_int(stmt, index, value);
        }
        void BindParameter(sqlite3_stmt *stmt, int index, int64_t value)
        {
            sqlite3_bind_int64(stmt, index, value);
        }
        void BindParameter(sqlite3_stmt *stmt, int index, double value)
        {
            sqlite3_bind_double(stmt, index, value);
        }
        void BindParameter(sqlite3_stmt *stmt, int index, const std::string &value)
        {
            sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
        }
        void BindParameter(sqlite3_stmt *stmt, int index, const char *value)
        {
             sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT);
        }
        void BindParameter(sqlite3_stmt *stmt, int index, std::nullptr_t)
        {
            sqlite3_bind_null(stmt, index);
        }
        template <typename T>
        void BindParameter(sqlite3_stmt *stmt, int index, const std::optional<T> &value)
        {
            if (value.has_value())
                BindParameter(stmt, index, value.value());
            else
                sqlite3_bind_null(stmt, index);
        }

        template <typename... Args>
        void Bind(sqlite3_stmt *stmt, int index, Args &&...args)
        {
            int i = index;
            (BindParameter(stmt, i++, std::forward<Args>(args)), ...);
        }

        // Variadic Execute
        template <typename... Args>
        void Execute(const std::string &sql, Args &&...args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
            {
                std::cerr << "SQL Error (Prepare): " << sqlite3_errmsg(m_db) << "\nSQL: " << sql << std::endl;
                return;
            }

            Bind(stmt, 1, std::forward<Args>(args)...);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
               // Note: STEP returning ROW is not an error but Execute is usually for non-query
               // const char* err = sqlite3_errmsg(m_db);
               // std::cerr << "SQL Info (Step): " << err << "\nSQL: " << sql << std::endl;
            }
            sqlite3_finalize(stmt);
        }

        // Variadic Query
        template <typename T, typename Func, typename... Args>
        std::vector<T> Query(const std::string &sql, Func mapper, Args &&...args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::vector<T> results;
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
            {
                std::cerr << "SQL Error (Prepare): " << sqlite3_errmsg(m_db) << "\nSQL: " << sql << std::endl;
                return results;
            }

            Bind(stmt, 1, std::forward<Args>(args)...);

            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                results.push_back(mapper(stmt));
            }
            sqlite3_finalize(stmt);
            return results;
        }

        // Variadic QuerySingle
        template <typename T, typename Func, typename... Args>
        std::optional<T> QuerySingle(const std::string &sql, Func mapper, Args &&...args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
             sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
            {
                std::cerr << "SQL Error (Prepare): " << sqlite3_errmsg(m_db) << "\nSQL: " << sql << std::endl;
                return std::nullopt;
            }

            Bind(stmt, 1, std::forward<Args>(args)...);

            std::optional<T> result = std::nullopt;
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                result = mapper(stmt);
            }
            sqlite3_finalize(stmt);
            return result;
        }
    };
} // namespace Server::DB