#include "server/database/Database.h"
#include <algorithm>
#include <random>
#include <stdexcept>

namespace Server::DB
{
    // Helper to extract text safely
    static std::string ExtractText(sqlite3_stmt *stmt, int col)
    {
        const char *txt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, col));
        return txt ? std::string(txt) : "";
    }

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
                game_duration INTEGER DEFAULT 0,
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

        auto safeExec = [&](const char *sql) {
            char *errMsg = 0;
            sqlite3_exec(m_db, sql, 0, 0, &errMsg);
            if (errMsg) sqlite3_free(errMsg);
        };

        safeExec("ALTER TABLE users ADD COLUMN wimp_mult_upper REAL DEFAULT 1.0");
        safeExec("ALTER TABLE users ADD COLUMN wimp_mult_lower REAL DEFAULT 1.0");
        safeExec("ALTER TABLE users ADD COLUMN wimp_mult_core REAL DEFAULT 1.0");
        safeExec("ALTER TABLE games ADD COLUMN game_duration INTEGER DEFAULT 0");
    }

    // =========================== USERS ===========================

    void Database::AddUser(const User &user)
    {
        const char *sql =
            "INSERT OR REPLACE INTO users (discord_id, riot_puuid, riot_name, riot_tag, region, "
            "last_match_id, wimp_mult_upper, wimp_mult_lower, wimp_mult_core) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        
        std::optional<std::string> lastMatch;
        if (!user.last_match_id.empty()) lastMatch = user.last_match_id;

        Execute(sql, user.discord_id, user.riot_puuid, user.riot_name, user.riot_tag, user.region, 
                lastMatch, user.mult_upper, user.mult_lower, user.mult_core);
    }

    std::vector<User> Database::GetDiscordUsers(int64_t discord_id)
    {
        auto mapper = [](sqlite3_stmt *stmt) {
            User u;
            u.discord_id = sqlite3_column_int64(stmt, 0);
            u.riot_puuid = ExtractText(stmt, 1);
            u.riot_name = ExtractText(stmt, 2);
            u.riot_tag = ExtractText(stmt, 3);
            u.region = ExtractText(stmt, 4);
            u.last_match_id = ExtractText(stmt, 5);
            u.mult_upper = sqlite3_column_double(stmt, 6);
            u.mult_lower = sqlite3_column_double(stmt, 7);
            u.mult_core = sqlite3_column_double(stmt, 8);
            return u;
        };
        return Query<User>("SELECT * FROM users WHERE discord_id = ?", mapper, discord_id);
    }

    std::vector<User> Database::GetAllUsers()
    {
        auto mapper = [](sqlite3_stmt *stmt) {
            User u;
            u.discord_id = sqlite3_column_int64(stmt, 0);
            u.riot_puuid = ExtractText(stmt, 1);
            u.riot_name = ExtractText(stmt, 2);
            u.riot_tag = ExtractText(stmt, 3);
            u.region = ExtractText(stmt, 4);
            u.last_match_id = ExtractText(stmt, 5);
            u.mult_upper = sqlite3_column_double(stmt, 6);
            u.mult_lower = sqlite3_column_double(stmt, 7);
            u.mult_core = sqlite3_column_double(stmt, 8);
            return u;
        };
        return Query<User>("SELECT * FROM users", mapper);
    }

    void Database::UpdateLastMatch(int64_t discord_id, const std::string &puuid, const std::string &match_id)
    {
        Execute("UPDATE users SET last_match_id = ? WHERE discord_id = ? AND riot_puuid = ?", match_id, discord_id, puuid);
    }

    void Database::SetUserMultiplier(int64_t discord_id, double multiplier, const std::string &type)
    {
        if (type.empty())
        {
            Execute("UPDATE users SET wimp_mult_upper = ?, wimp_mult_lower = ?, wimp_mult_core = ? WHERE discord_id = ?", 
                    multiplier, multiplier, multiplier, discord_id);
        }
        else if (type == "upper")
        {
            Execute("UPDATE users SET wimp_mult_upper = ? WHERE discord_id = ?", multiplier, discord_id);
        }
        else if (type == "lower")
        {
            Execute("UPDATE users SET wimp_mult_lower = ? WHERE discord_id = ?", multiplier, discord_id);
        }
        else if (type == "core")
        {
            Execute("UPDATE users SET wimp_mult_core = ? WHERE discord_id = ?", multiplier, discord_id);
        }
    }

    double Database::GetUserMultiplier(int64_t discord_id, const std::string &type)
    {
        std::string col = "wimp_mult_upper";
        if (type == "lower") col = "wimp_mult_lower";
        else if (type == "core") col = "wimp_mult_core";

        std::string sql = "SELECT " + col + " FROM users WHERE discord_id = ? LIMIT 1";

        auto res = QuerySingle<double>(sql, [](sqlite3_stmt* stmt){
            return sqlite3_column_double(stmt, 0);
        }, discord_id);

        return res.value_or(1.0);
    }

    // =========================== EXERCISES ===========================

    void Database::SeedExercises(const std::vector<ExerciseDefinition> &exercises)
    {
        Execute("DELETE FROM exercises");
        if (exercises.empty()) return;

        for (const auto &ex : exercises)
        {
            Execute("INSERT INTO exercises (exercise_name, set_count, exercise_type) VALUES (?, ?, ?)", 
                    ex.name, ex.set_count, ex.type);
        }
    }

    std::vector<ExerciseDefinition> Database::GetAllExercises()
    {
         return Query<ExerciseDefinition>("SELECT id, exercise_name, set_count, exercise_type FROM exercises", 
            [](sqlite3_stmt* stmt){
                ExerciseDefinition def;
                def.id = sqlite3_column_int(stmt, 0);
                def.name = ExtractText(stmt, 1);
                def.set_count = sqlite3_column_int(stmt, 2);
                def.type = ExtractText(stmt, 3);
                return def;
            });
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
        const char *sql =
            "INSERT INTO exercise_queue (user_id, match_id, exercise_name, reps, original_deaths) VALUES (?, ?, ?, ?, ?)";
        Execute(sql, user_id, match_id, exercise, reps, deaths);
    }

    std::vector<ExerciseQueueItem> Database::GetPendingPenance(int64_t user_id)
    {
        const char *sql = "SELECT id, user_id, match_id, exercise_name, reps, original_deaths, timestamp FROM "
                          "exercise_queue WHERE user_id = ?";
        return Query<ExerciseQueueItem>(sql, [](sqlite3_stmt* stmt){
            ExerciseQueueItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.user_id = sqlite3_column_int64(stmt, 1);
            item.match_id = ExtractText(stmt, 2);
            item.exercise_name = ExtractText(stmt, 3);
            item.reps = sqlite3_column_int(stmt, 4);
            item.original_deaths = sqlite3_column_int(stmt, 5);
            item.timestamp = ExtractText(stmt, 6);
            return item;
        }, user_id);
    }

    // New Implementation for Rich Stats
    std::vector<PenanceDisplayInfo> Database::GetPendingPenanceDetailed(int64_t user_id)
    {
        const char *sql = R"(
            SELECT 
                eq.id, eq.match_id, eq.exercise_name, eq.reps, eq.original_deaths,
                g.champion_name, g.kills, g.deaths, g.assists, g.kp_percent, g.cs_total, g.cs_min, g.timestamp
            FROM exercise_queue eq
            LEFT JOIN games g ON eq.match_id = g.match_id AND eq.user_id = g.user_id
            WHERE eq.user_id = ?
            ORDER BY eq.id DESC
        )";

        return Query<PenanceDisplayInfo>(sql, [](sqlite3_stmt* stmt){
            PenanceDisplayInfo item;
            item.id = sqlite3_column_int(stmt, 0);
            item.match_id = ExtractText(stmt, 1);
            item.exercise_name = ExtractText(stmt, 2);
            item.reps = sqlite3_column_int(stmt, 3);
            item.original_deaths = sqlite3_column_int(stmt, 4);

            std::string champ = ExtractText(stmt, 5);
            item.champion_name = champ.empty() ? "Unknown" : champ;

            item.kills = sqlite3_column_int(stmt, 6);
            item.deaths = sqlite3_column_int(stmt, 7);
            item.assists = sqlite3_column_int(stmt, 8);
            item.kp_percent = sqlite3_column_double(stmt, 9);
            item.cs = sqlite3_column_int(stmt, 10);
            item.cs_min = sqlite3_column_double(stmt, 11);
            item.game_timestamp = sqlite3_column_int64(stmt, 12);

            return item;
        }, user_id);
    }

    std::optional<ExerciseQueueItem> Database::GetPenanceByGameID(int64_t user_id, const std::string &match_id)
    {
        const char *sql = "SELECT id, user_id, match_id, exercise_name, reps, original_deaths, timestamp FROM "
                          "exercise_queue WHERE user_id = ? AND match_id = ? LIMIT 1";

        return QuerySingle<ExerciseQueueItem>(sql, [](sqlite3_stmt* stmt){
             ExerciseQueueItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.user_id = sqlite3_column_int64(stmt, 1);
            item.match_id = ExtractText(stmt, 2);
            item.exercise_name = ExtractText(stmt, 3);
            item.reps = sqlite3_column_int(stmt, 4);
            item.original_deaths = sqlite3_column_int(stmt, 5);
            item.timestamp = ExtractText(stmt, 6);
            return item;
        }, user_id, match_id);
    }

    void Database::CompletePenance(int64_t user_id, const std::string &match_id)
    {
        auto item = GetPenanceByGameID(user_id, match_id);
        if (!item) return;

        Execute("DELETE FROM exercise_queue WHERE id = ?", item->id);
        Execute("INSERT INTO exercise_history (user_id, exercise_name, reps) VALUES (?, ?, ?)", 
                 user_id, item->exercise_name, item->reps);
    }

    void Database::UpdatePenance(int row_id, const std::string &new_ex, int new_reps)
    {
        Execute("UPDATE exercise_queue SET exercise_name = ?, reps = ? WHERE id = ?", new_ex, new_reps, row_id);
    }

    // =========================== STATS ===========================

    bool Database::IsMatchProcessed(int64_t discord_id, const std::string &match_id)
    {
        auto res = QuerySingle<int>("SELECT 1 FROM games WHERE match_id = ? AND user_id = ? LIMIT 1", 
            [](sqlite3_stmt*){ return 1; }, match_id, discord_id);
        return res.has_value();
    }

    void Database::LogGame(int64_t user_id, const std::string &match_id, int64_t timestamp, int64_t gameDuration, const std::string &champ, int k,
                           int d, int a, double kp, int cs, double cs_min)
    {
        const char *sql = "INSERT OR IGNORE INTO games (match_id, user_id, timestamp, champion_name, kills, deaths, "
                          "assists, kp_percent, cs_total, cs_min, game_duration) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        Execute(sql, match_id, user_id, timestamp, champ, k, d, a, kp, cs, cs_min, gameDuration);
    }

    UserStats Database::GetUserStats(int64_t user_id)
    {
        // NO LOCK here because Execute/Query take lock.
        // This relies on the fact that we don't need transactional consistency across all these separate selects for the UI view.
        // (It's acceptable if data changes slightly between the death count query and the KDA query).
        UserStats stats = {0, 0, 0.0, 0, 0.0, {}, {}, 0};

        // 1. Basic Stats
        auto basicStats = QuerySingle<std::tuple<int, int, int, double>>("SELECT SUM(deaths), COUNT(*), MAX(deaths), SUM(game_duration) FROM games WHERE user_id = ?",
            [](sqlite3_stmt* s){
                return std::make_tuple(
                    sqlite3_column_int(s, 0),
                    sqlite3_column_int(s, 1),
                    sqlite3_column_int(s, 2),
                    sqlite3_column_double(s, 3)
                );
            }, user_id);

        if (basicStats) {
            auto [deaths, games, max_deaths, duration] = *basicStats;
            stats.total_deaths = deaths;
            stats.total_games = games;
            stats.most_deaths_single = max_deaths;
            if (duration > 0) stats.avg_deaths_min = (double)deaths / (duration / 60.0);
        }

        // 2. KDA
        auto kda = QuerySingle<double>("SELECT MIN(CAST((kills + assists) AS REAL) / NULLIF(deaths, 0)) FROM games WHERE user_id = ? AND deaths > 0",
             [](sqlite3_stmt* s){
                 if (sqlite3_column_type(s, 0) != SQLITE_NULL) return sqlite3_column_double(s, 0);
                 return 0.0;
             }, user_id);
        if(kda) stats.lowest_kda = *kda;

        // 3. Exercise Counts
        auto exCounts = Query<std::pair<std::string, int>>("SELECT exercise_name, SUM(reps) FROM exercise_history WHERE user_id = ? GROUP BY exercise_name",
            [](sqlite3_stmt* s){
                return std::make_pair(ExtractText(s, 0), sqlite3_column_int(s, 1));
            }, user_id);
        for(auto& p : exCounts) stats.exercise_counts[p.first] = p.second;

        // 4. Top Death Champs
        auto topChamps = Query<std::pair<std::string, int>>("SELECT champion_name, SUM(deaths) as d FROM games WHERE user_id = ? GROUP BY champion_name ORDER BY d DESC LIMIT 3",
             [](sqlite3_stmt* s){
                return std::make_pair(ExtractText(s, 0), sqlite3_column_int(s, 1));
             }, user_id);
        stats.top_death_champs = topChamps;

        // 5. Pending Count
        auto pending = QuerySingle<int>("SELECT COUNT(*) FROM exercise_queue WHERE user_id = ?", 
            [](sqlite3_stmt* s){ return sqlite3_column_int(s, 0); }, user_id);
        if(pending) stats.pending_penance_count = *pending;

        return stats;
    }

    // =========================== UI/UX IMPROVEMENTS ===========================

    std::vector<PenanceDisplayInfo> Database::GetRecentGames(int64_t user_id, int limit)
    {
        const char *sql = "SELECT match_id, user_id, timestamp, champion_name, kills, deaths, assists, kp_percent, cs_total, "
                          "cs_min FROM games WHERE user_id = ? ORDER BY timestamp DESC LIMIT ?";
        
        return Query<PenanceDisplayInfo>(sql, [](sqlite3_stmt* stmt){
            PenanceDisplayInfo item;
            item.match_id = ExtractText(stmt, 0);
            item.game_timestamp = sqlite3_column_int64(stmt, 2);
            item.champion_name = ExtractText(stmt, 3);
            item.kills = sqlite3_column_int(stmt, 4);
            item.deaths = sqlite3_column_int(stmt, 5);
            item.assists = sqlite3_column_int(stmt, 6);
            item.kp_percent = sqlite3_column_double(stmt, 7);
            item.cs = sqlite3_column_int(stmt, 8);
            item.cs_min = sqlite3_column_double(stmt, 9);
            return item;
        }, user_id, limit);
    }

    std::vector<std::pair<std::string, int>> Database::GetLeaderboard(const std::string &type)
    {
        std::string sql;
        if (type == "reps")
        {
            sql = "SELECT u.riot_name, SUM(h.reps) as val FROM exercise_history h "
                  "JOIN users u ON h.user_id = u.discord_id "
                  "GROUP BY h.user_id ORDER BY val DESC LIMIT 10";
        }
        else if (type == "deaths")
        {
             sql = "SELECT u.riot_name, SUM(g.deaths) as val FROM games g "
                  "JOIN users u ON g.user_id = u.discord_id "
                  "GROUP BY g.user_id ORDER BY val DESC LIMIT 10";
        }
        else if (type == "kda")
        {
             sql = "SELECT u.riot_name, (CAST((SUM(g.kills) + SUM(g.assists)) AS REAL) / MAX(SUM(g.deaths), 1)) * 100 as val "
                   "FROM games g "
                   "JOIN users u ON g.user_id = u.discord_id "
                   "GROUP BY g.user_id HAVING COUNT(*) > 5 ORDER BY val DESC LIMIT 10";
        }

        if (sql.empty()) return {};

        return Query<std::pair<std::string, int>>(sql, [](sqlite3_stmt* stmt){
            std::string name = ExtractText(stmt, 0);
            return std::make_pair(name.empty() ? "Unknown" : name, sqlite3_column_int(stmt, 1));
        });
    }
} // namespace Server::DB