#include "server/riot/RiotClient.h"
#include <cmath>
#include <future>
#include <iostream>
#include <thread>

namespace Server::Riot
{
    // Initialize Rate Limiters
    // App Rate Limit: 20 requests every 1 second AND 100 requests every 2 minutes.
    // We choose the tighter constraint for safety: 100 req / 120000ms ~ 0.833 req / 1000ms.
    // We can also chain them, but a single strict bucket usually suffices for small bots.
    RiotClient::RiotClient(std::shared_ptr<dpp::cluster> bot, const std::string &apiKey)
        : m_bot(bot), m_apiKey(apiKey), m_limiter(std::make_unique<RateLimiter>(20, 25000)) // 20req/25sec to be safe
    {
        m_routing = {{"na1", "americas"}, {"br1", "americas"}, {"la1", "americas"}, {"la2", "americas"},
                     {"euw1", "europe"},  {"eun1", "europe"},  {"tr1", "europe"},   {"ru", "europe"},
                     {"kr", "asia"},      {"jp1", "asia"},     {"oc1", "sea"}};
    }

    std::string RiotClient::GetRoute(const std::string &region)
    {
        auto it = m_routing.find(region);
        return (it != m_routing.end()) ? it->second : "americas";
    }

    nlohmann::json RiotClient::Request(const std::string &url)
    {
        if (!m_bot)
            return nullptr;

        int retries = 0;
        const int MAX_RETRIES = 3;

        while (retries < MAX_RETRIES)
        {
            // Block until token is available
            m_limiter->Wait();

            // 1. Prepare URL
            std::string clean_url = url;
            if (clean_url.substr(0, 8) == "https://")
            {
                clean_url = clean_url.substr(8);
            }
            else if (clean_url.substr(0, 7) == "http://")
            {
                clean_url = clean_url.substr(7);
            }

            // 2. Prepare Headers
            std::multimap<std::string, std::string> headers;
            headers.emplace("X-Riot-Token", m_apiKey);

            // 3. Sync Execution Wrapper
            auto promise = std::make_shared<std::promise<dpp::http_request_completion_t>>();
            auto future = promise->get_future();

            m_bot->request(
                url, dpp::m_get, [promise](const dpp::http_request_completion_t &cc) { promise->set_value(cc); }, "",
                "application/json", headers);

            if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout)
            {
                std::cerr << "Riot API Timeout: " << url << std::endl;
                return nullptr;
            }

            auto response = future.get();

            // Handle Rate Limits (429) specifically
            if (response.status == 429)
            {
                std::cerr << "⚠️ 429 HIT from Riot. Local limiter failed. Backing off..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2 * (retries + 1)));
                retries++;
                continue;
            }

            if (response.status == 200)
            {
                try
                {
                    return nlohmann::json::parse(response.body);
                }
                catch (...)
                {
                    std::cerr << "JSON Parse Error for " << url << std::endl;
                    return nullptr;
                }
            }

            std::cerr << "Riot API Error " << response.status << ": " << url << std::endl;
            return nullptr;
        }
        return nullptr;
    }

    std::tuple<std::string, std::string, std::string> RiotClient::GetAccount(const std::string &name, const std::string &tag,
                                                                             const std::string &region)
    {
        std::string route = GetRoute(region);
        std::string encodedName = dpp::utility::url_encode(name);
        std::string url =
            "https://" + route + ".api.riotgames.com/riot/account/v1/accounts/by-riot-id/" + encodedName + "/" + tag;

        auto json = Request(url);
        if (!json.is_null() && json.contains("puuid"))
        {
            return {json.value("puuid", ""), json.value("gameName", ""), json.value("tagLine", "")};
        }
        return {};
    }

    std::vector<std::string> RiotClient::GetLastMatches(const std::string &puuid, const std::string &region, int count)
    {
        std::string route = GetRoute(region);
        std::string url = "https://" + route + ".api.riotgames.com/lol/match/v5/matches/by-puuid/" + puuid +
                          "/ids?start=0&count=" + std::to_string(count);

        auto json = Request(url);
        if (json.is_array())
        {
            return json.get<std::vector<std::string>>();
        }
        return {};
    }

    MatchStats RiotClient::AnalyzeMatch(const std::string &match_id, const std::string &puuid, const std::string &region)
    {
        MatchStats stats;
        std::string route = GetRoute(region);
        std::string url = "https://" + route + ".api.riotgames.com/lol/match/v5/matches/" + match_id;

        auto json = Request(url);
        if (json.is_null() || !json.contains("info"))
            return stats;

        try
        {
            auto info = json["info"];
            auto participants = info["participants"];
            long gameDuration = info.value("gameDuration", 0L);

            nlohmann::json userP;
            int teamId = 0;
            bool found = false;
            int teamKills = 0;

            for (const auto &p : participants)
            {
                if (p.value("puuid", "") == puuid)
                {
                    userP = p;
                    teamId = p.value("teamId", 0);
                    found = true;
                }
            }

            if (found)
            {
                // Calculate team kills separately after finding teamId
                for (const auto &p : participants)
                {
                    if (p.value("teamId", 0) == teamId)
                        teamKills += p.value("kills", 0);
                }

                stats.valid = true;
                // SAFE ACCESS: Use .value() defaults to prevent crashes
                stats.champion_name = userP.value("championName", "Unknown");
                stats.kills = userP.value("kills", 0);
                stats.deaths = userP.value("deaths", 0);
                stats.assists = userP.value("assists", 0);
                stats.win = userP.value("win", false);
                stats.timestamp = info.value("gameCreation", 0LL);

                int involvement = stats.kills + stats.assists;
                stats.kp_percent = (teamKills > 0) ? ((double)involvement / teamKills * 100.0) : 0.0;

                int totalMinions = userP.value("totalMinionsKilled", 0);
                int neutralMinions = userP.value("neutralMinionsKilled", 0);
                stats.cs = totalMinions + neutralMinions;
                stats.cs_min = (gameDuration > 0) ? (stats.cs / (gameDuration / 60.0)) : 0.0;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error analyzing match JSON: " << e.what() << std::endl;
            stats.valid = false;
        }

        return stats;
    }
} // namespace Server::Riot