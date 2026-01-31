#include "server/database/Database.h"
#include <algorithm>
#include <random>
#include <stdexcept>

namespace Server::DB
{
    Database::Database(const std::string &dbPath)
    {
        if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to open database");
        }
        Initialize();
    }

    Database::~Database() { sqlite3_close(m_db); }

    void Database::ExecuteSQL(const std::string &sql)
    {
        char *errMsg = 0;
        if (sqlite3_exec(m_db, sql.c_str(), 0, 0, &errMsg) != SQLITE_OK)
        {
            std::string error = errMsg;
            sqlite3_free(errMsg);
            std::cerr << "SQL Info/Error: " << error << " in " << sql << std::endl;
        }
    }

    void Database::Initialize()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Enable Write-Ahead Logging (WAL) for better concurrency
        ExecuteSQL("PRAGMA journal_mode=WAL;");
        ExecuteSQL("PRAGMA synchronous=NORMAL;");

        const char *schema = R"(
            CREATE TABLE IF NOT EXISTS users (
                discord_id INTEGER,
                riot_puuid TEXT,
                riot_name TEXT,
                riot_tag TEXT,
                region TEXT,
                last_match_id TEXT,
                wimp_mult_upper REAL DEFAULT 1.0,
                wimp_mult_lower REAL DEFAULT 1.0,
                wimp_mult_core REAL DEFAULT 1.0,
                PRIMARY KEY (discord_id, riot_puuid)
            );
            
            CREATE TABLE IF NOT EXISTS exercises (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                exercise_name TEXT,
                set_count INTEGER,
                exercise_type TEXT
            );

            CREATE TABLE IF NOT EXISTS games (
                match_id TEXT,
                user_id INTEGER,
                timestamp INTEGER,
                champion_name TEXT,
                kills INTEGER,
                deaths INTEGER,
                assists INTEGER,
                kp_percent REAL,
                cs_total INTEGER,
                cs_min REAL,
                PRIMARY KEY (match_id, user_id)
            );
            CREATE TABLE IF NOT EXISTS exercise_queue (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                match_id TEXT,
                exercise_name TEXT,
                reps INTEGER,
                original_deaths INTEGER,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            );
            CREATE TABLE IF NOT EXISTS exercise_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                exercise_name TEXT,
                reps INTEGER,
                completed_at DATETIME DEFAULT CURRENT_TIMESTAMP
            );
        )";
        ExecuteSQL(schema);

        // Migrations
        char *errMsg = 0;
        sqlite3_exec(m_db, "ALTER TABLE users ADD COLUMN wimp_mult_upper REAL DEFAULT 1.0", 0, 0, &errMsg);
        sqlite3_free(errMsg);
        sqlite3_exec(m_db, "ALTER TABLE users ADD COLUMN wimp_mult_lower REAL DEFAULT 1.0", 0, 0, &errMsg);
        sqlite3_free(errMsg);
        sqlite3_exec(m_db, "ALTER TABLE users ADD COLUMN wimp_mult_core REAL DEFAULT 1.0", 0, 0, &errMsg);
        sqlite3_free(errMsg);
    }

    // =========================== USERS ===========================

    void Database::AddUser(const User &user)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        const char *sql =
            "INSERT OR REPLACE INTO users (discord_id, riot_puuid, riot_name, riot_tag, region, "
            "last_match_id, wimp_mult_upper, wimp_mult_lower, wimp_mult_core) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
        {
            std::cerr << "SQL Error in AddUser: " << sqlite3_errmsg(m_db) << std::endl;
            return;
        }

        sqlite3_bind_int64(stmt, 1, user.discord_id);
        sqlite3_bind_text(stmt, 2, user.riot_puuid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, user.riot_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, user.riot_tag.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, user.region.c_str(), -1, SQLITE_STATIC);

        if (user.last_match_id.empty())
            sqlite3_bind_null(stmt, 6);
        else
            sqlite3_bind_text(stmt, 6, user.last_match_id.c_str(), -1, SQLITE_STATIC);

        sqlite3_bind_double(stmt, 7, user.mult_upper);
        sqlite3_bind_double(stmt, 8, user.mult_lower);
        sqlite3_bind_double(stmt, 9, user.mult_core);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::vector<User> Database::GetDiscordUsers(int64_t discord_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<User> users;
        sqlite3_stmt *stmt;
        const char *sql = "SELECT * FROM users WHERE discord_id = ?";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return users;

        sqlite3_bind_int64(stmt, 1, discord_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            User u;
            u.discord_id = sqlite3_column_int64(stmt, 0);
            u.riot_puuid = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            u.riot_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            u.riot_tag = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            u.region = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
            const char *match = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
            u.last_match_id = match ? match : "";
            u.mult_upper = sqlite3_column_double(stmt, 6);
            u.mult_lower = sqlite3_column_double(stmt, 7);
            u.mult_core = sqlite3_column_double(stmt, 8);
            users.push_back(u);
        }
        sqlite3_finalize(stmt);
        return users;
    }

    std::vector<User> Database::GetAllUsers()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<User> users;
        sqlite3_stmt *stmt;
        const char *sql = "SELECT * FROM users";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return users;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            User u;
            u.discord_id = sqlite3_column_int64(stmt, 0);
            u.riot_puuid = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            u.riot_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            u.riot_tag = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            u.region = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
            const char *match = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
            u.last_match_id = match ? match : "";
            u.mult_upper = sqlite3_column_double(stmt, 6);
            u.mult_lower = sqlite3_column_double(stmt, 7);
            u.mult_core = sqlite3_column_double(stmt, 8);
            users.push_back(u);
        }
        sqlite3_finalize(stmt);
        return users;
    }

    void Database::UpdateLastMatch(int64_t discord_id, const std::string &puuid, const std::string &match_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(m_db, "UPDATE users SET last_match_id = ? WHERE discord_id = ? AND riot_puuid = ?", -1, &stmt,
                               0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, match_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, discord_id);
            sqlite3_bind_text(stmt, 3, puuid.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    void Database::SetUserMultiplier(int64_t discord_id, double multiplier, const std::string &type)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        std::string sql;
        if (type.empty())
            sql = "UPDATE users SET wimp_mult_upper = ?, wimp_mult_lower = ?, wimp_mult_core = ? WHERE discord_id = ?";
        else if (type == "upper")
            sql = "UPDATE users SET wimp_mult_upper = ? WHERE discord_id = ?";
        else if (type == "lower")
            sql = "UPDATE users SET wimp_mult_lower = ? WHERE discord_id = ?";
        else if (type == "core")
            sql = "UPDATE users SET wimp_mult_core = ? WHERE discord_id = ?";

        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK)
        {
            if (type.empty())
            {
                sqlite3_bind_double(stmt, 1, multiplier);
                sqlite3_bind_double(stmt, 2, multiplier);
                sqlite3_bind_double(stmt, 3, multiplier);
                sqlite3_bind_int64(stmt, 4, discord_id);
            }
            else
            {
                sqlite3_bind_double(stmt, 1, multiplier);
                sqlite3_bind_int64(stmt, 2, discord_id);
            }
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    double Database::GetUserMultiplier(int64_t discord_id, const std::string &type)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        std::string col = "wimp_mult_upper"; // default
        if (type == "lower")
            col = "wimp_mult_lower";
        else if (type == "core")
            col = "wimp_mult_core";

        std::string sql = "SELECT " + col + " FROM users WHERE discord_id = ? LIMIT 1";

        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK)
            return 1.0;
        sqlite3_bind_int64(stmt, 1, discord_id);

        double mult = 1.0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            mult = sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
        return mult;
    }

    // =========================== EXERCISES ===========================

    void Database::SeedExercises(const std::vector<ExerciseDefinition> &exercises)
    {
        ExecuteSQL("DELETE FROM exercises");
        if (exercises.empty())
            return;

        sqlite3_stmt *stmt;
        const char *sql = "INSERT INTO exercises (exercise_name, set_count, exercise_type) VALUES (?, ?, ?)";
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return;

        for (const auto &ex : exercises)
        {
            sqlite3_bind_text(stmt, 1, ex.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, ex.set_count);
            sqlite3_bind_text(stmt, 3, ex.type.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }

    std::vector<ExerciseDefinition> Database::GetAllExercises()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ExerciseDefinition> exs;
        sqlite3_stmt *stmt;
        const char *sql = "SELECT id, exercise_name, set_count, exercise_type FROM exercises";
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return exs;

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ExerciseDefinition def;
            def.id = sqlite3_column_int(stmt, 0);
            def.name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            def.set_count = sqlite3_column_int(stmt, 2);
            def.type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            exs.push_back(def);
        }
        sqlite3_finalize(stmt);
        return exs;
    }

    std::optional<ExerciseDefinition> Database::GetRandomExercise()
    {
        auto exs = GetAllExercises();
        if (exs.empty())
            return std::nullopt;
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, exs.size() - 1);
        return exs[dist(gen)];
    }

    // =========================== QUEUE ===========================

    void Database::AddToQueue(int64_t user_id, const std::string &match_id, const std::string &exercise, int reps,
                              int deaths)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        const char *sql =
            "INSERT INTO exercise_queue (user_id, match_id, exercise_name, reps, original_deaths) VALUES (?, ?, ?, ?, ?)";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, match_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, exercise.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 4, reps);
            sqlite3_bind_int(stmt, 5, deaths);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    std::vector<ExerciseQueueItem> Database::GetPendingPenance(int64_t user_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<ExerciseQueueItem> items;
        sqlite3_stmt *stmt;
        const char *sql = "SELECT id, user_id, match_id, exercise_name, reps, original_deaths, timestamp FROM "
                          "exercise_queue WHERE user_id = ?";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return items;

        sqlite3_bind_int64(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ExerciseQueueItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.user_id = sqlite3_column_int64(stmt, 1);
            item.match_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            item.exercise_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            item.reps = sqlite3_column_int(stmt, 4);
            item.original_deaths = sqlite3_column_int(stmt, 5);
            item.timestamp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
            items.push_back(item);
        }
        sqlite3_finalize(stmt);
        return items;
    }

    // New Implementation for Rich Stats
    std::vector<PenanceDisplayInfo> Database::GetPendingPenanceDetailed(int64_t user_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<PenanceDisplayInfo> items;
        sqlite3_stmt *stmt;

        // Left Join ensures we still get the task even if game data is missing (though unlikely in our flow)
        const char *sql = R"(
            SELECT 
                eq.id, eq.match_id, eq.exercise_name, eq.reps, eq.original_deaths,
                g.champion_name, g.kills, g.deaths, g.assists, g.kp_percent, g.cs_total, g.cs_min, g.timestamp
            FROM exercise_queue eq
            LEFT JOIN games g ON eq.match_id = g.match_id AND eq.user_id = g.user_id
            WHERE eq.user_id = ?
            ORDER BY eq.id DESC
        )";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return items;

        sqlite3_bind_int64(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            PenanceDisplayInfo item;
            item.id = sqlite3_column_int(stmt, 0);
            item.match_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            item.exercise_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            item.reps = sqlite3_column_int(stmt, 3);
            item.original_deaths = sqlite3_column_int(stmt, 4);

            // Handle potentially NULL game stats
            const char *champ = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
            item.champion_name = champ ? champ : "Unknown";

            item.kills = sqlite3_column_int(stmt, 6);
            item.deaths = sqlite3_column_int(stmt, 7);
            item.assists = sqlite3_column_int(stmt, 8);
            item.kp_percent = sqlite3_column_double(stmt, 9);
            item.cs = sqlite3_column_int(stmt, 10);
            item.cs_min = sqlite3_column_double(stmt, 11);
            item.game_timestamp = sqlite3_column_int64(stmt, 12);

            items.push_back(item);
        }
        sqlite3_finalize(stmt);
        return items;
    }

    std::optional<ExerciseQueueItem> Database::GetPenanceByGameID(int64_t user_id, const std::string &match_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        const char *sql = "SELECT id, user_id, match_id, exercise_name, reps, original_deaths, timestamp FROM "
                          "exercise_queue WHERE user_id = ? AND match_id = ? LIMIT 1";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return std::nullopt;

        sqlite3_bind_int64(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, match_id.c_str(), -1, SQLITE_STATIC);

        std::optional<ExerciseQueueItem> res = std::nullopt;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ExerciseQueueItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.user_id = sqlite3_column_int64(stmt, 1);
            item.match_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            item.exercise_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            item.reps = sqlite3_column_int(stmt, 4);
            item.original_deaths = sqlite3_column_int(stmt, 5);
            item.timestamp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
            res = item;
        }
        sqlite3_finalize(stmt);
        return res;
    }

    void Database::CompletePenance(int64_t user_id, const std::string &match_id)
    {
        auto item = GetPenanceByGameID(user_id, match_id);
        if (!item)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *delStmt;
        if (sqlite3_prepare_v2(m_db, "DELETE FROM exercise_queue WHERE id = ?", -1, &delStmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int(delStmt, 1, item->id);
            sqlite3_step(delStmt);
            sqlite3_finalize(delStmt);
        }

        sqlite3_stmt *insStmt;
        if (sqlite3_prepare_v2(m_db, "INSERT INTO exercise_history (user_id, exercise_name, reps) VALUES (?, ?, ?)", -1,
                               &insStmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(insStmt, 1, user_id);
            sqlite3_bind_text(insStmt, 2, item->exercise_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(insStmt, 3, item->reps);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
        }
    }

    void Database::UpdatePenance(int row_id, const std::string &new_ex, int new_reps)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(m_db, "UPDATE exercise_queue SET exercise_name = ?, reps = ? WHERE id = ?", -1, &stmt, 0) ==
            SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, new_ex.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, new_reps);
            sqlite3_bind_int(stmt, 3, row_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    // =========================== STATS ===========================

    // Checks if match already exists in DB
    bool Database::IsMatchProcessed(int64_t discord_id, const std::string &match_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        // Use 1 to just check existence, cheaper than fetching fields
        const char *sql = "SELECT 1 FROM games WHERE match_id = ? AND user_id = ? LIMIT 1";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK)
            return false;

        sqlite3_bind_text(stmt, 1, match_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, discord_id);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            exists = true;
        }
        sqlite3_finalize(stmt);
        return exists;
    }

    void Database::LogGame(int64_t user_id, const std::string &match_id, int64_t timestamp, const std::string &champ, int k,
                           int d, int a, double kp, int cs, double cs_min)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sqlite3_stmt *stmt;
        const char *sql = "INSERT OR IGNORE INTO games (match_id, user_id, timestamp, champion_name, kills, deaths, "
                          "assists, kp_percent, cs_total, cs_min) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, match_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, user_id);
            sqlite3_bind_int64(stmt, 3, timestamp);
            sqlite3_bind_text(stmt, 4, champ.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, k);
            sqlite3_bind_int(stmt, 6, d);
            sqlite3_bind_int(stmt, 7, a);
            sqlite3_bind_double(stmt, 8, kp);
            sqlite3_bind_int(stmt, 9, cs);
            sqlite3_bind_double(stmt, 10, cs_min);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    UserStats Database::GetUserStats(int64_t user_id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        UserStats stats = {0, 0, 0.0, 0, 0.0, {}, {}, 0};
        sqlite3_stmt *stmt;

        if (sqlite3_prepare_v2(m_db, "SELECT SUM(deaths), COUNT(*), MAX(deaths), AVG(deaths) FROM games WHERE user_id = ?",
                               -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                stats.total_deaths = sqlite3_column_int(stmt, 0);
                stats.total_games = sqlite3_column_int(stmt, 1);
                stats.most_deaths_single = sqlite3_column_int(stmt, 2);
                stats.avg_deaths_min = sqlite3_column_double(stmt, 3) / 30.0;
            }
            sqlite3_finalize(stmt);
        }

        const char *kdaSql =
            "SELECT MIN(CAST((kills + assists) AS REAL) / NULLIF(deaths, 0)) FROM games WHERE user_id = ? AND deaths > 0";
        if (sqlite3_prepare_v2(m_db, kdaSql, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
                    stats.lowest_kda = sqlite3_column_double(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        const char *exSql = "SELECT exercise_name, SUM(reps) FROM exercise_history WHERE user_id = ? GROUP BY exercise_name";
        if (sqlite3_prepare_v2(m_db, exSql, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, user_id);
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                std::string name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                int count = sqlite3_column_int(stmt, 1);
                stats.exercise_counts[name] = count;
            }
            sqlite3_finalize(stmt);
        }

        const char *chSql = "SELECT champion_name, SUM(deaths) as d FROM games WHERE user_id = ? GROUP BY champion_name "
                            "ORDER BY d DESC LIMIT 3";
        if (sqlite3_prepare_v2(m_db, chSql, -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, user_id);
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                std::string name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                int d = sqlite3_column_int(stmt, 1);
                stats.top_death_champs.push_back({name, d});
            }
            sqlite3_finalize(stmt);
        }

        if (sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM exercise_queue WHERE user_id = ?", -1, &stmt, 0) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
                stats.pending_penance_count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }

        return stats;
    }
} // namespace Server::DB