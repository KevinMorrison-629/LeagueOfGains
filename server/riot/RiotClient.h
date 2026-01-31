#pragma once
#include "server/riot/RateLimiter.h"
#include <dpp/dpp.h>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace Server::Riot
{
    struct MatchStats
    {
        bool valid = false;
        std::string champion_name;
        int kills;
        int deaths;
        int assists;
        double kp_percent;
        int cs;
        double cs_min;
        int64_t timestamp;
        int64_t gameDuration;
        bool win;
    };

    class RiotClient
    {
    public:
        RiotClient(std::shared_ptr<dpp::cluster> bot, const std::string &apiKey);

        std::tuple<std::string, std::string, std::string> GetAccount(const std::string &name, const std::string &tag,
                                                                     const std::string &region);

        // Count defaults to 5 now to catch missed games
        std::vector<std::string> GetLastMatches(const std::string &puuid, const std::string &region, int count = 5);

        MatchStats AnalyzeMatch(const std::string &match_id, const std::string &puuid, const std::string &region);

    private:
        std::shared_ptr<dpp::cluster> m_bot;
        std::string m_apiKey;
        std::unique_ptr<RateLimiter> m_limiter; // Added RateLimiter
        std::map<std::string, std::string> m_routing;

        std::string GetRoute(const std::string &region);
        nlohmann::json Request(const std::string &url);
    };
} // namespace Server::Riot