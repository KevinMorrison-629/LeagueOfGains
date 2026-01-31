#pragma once
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace Server::Riot
{
    /**
     * @brief A thread-safe Token Bucket Rate Limiter.
     * Prevents the bot from exceeding Riot API limits (e.g., 20 req / 1 sec, 100 req / 2 min).
     */
    class RateLimiter
    {
    public:
        RateLimiter(int max_tokens, int refill_duration_ms)
            : m_max_tokens(max_tokens), m_tokens(max_tokens), m_refill_duration(refill_duration_ms)
        {
            m_last_refill = std::chrono::steady_clock::now();
        }

        void Wait()
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            while (true)
            {
                RefillTokens();

                if (m_tokens > 0)
                {
                    m_tokens--;
                    return;
                }

                // Calculate time to next refill to sleep efficiently
                auto now = std::chrono::steady_clock::now();
                auto time_since_refill = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_refill);
                auto time_to_wait = m_refill_duration - time_since_refill;

                if (time_to_wait.count() > 0)
                {
                    // Wait for condition variable or timeout
                    // We log this because it implies we are hitting capacity
                    std::cout << "[RateLimiter] Throttling request for " << time_to_wait.count() << "ms..." << std::endl;
                    m_cv.wait_for(lock, time_to_wait);
                }
            }
        }

    private:
        void RefillTokens()
        {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_refill);

            if (duration >= m_refill_duration)
            {
                // Logic: How many intervals passed?
                // For simplicity in this specific bucket type, if the duration passed, we reset full tokens.
                // A more complex sliding window could be used, but this is sufficient for Riot's short burst windows.
                m_tokens = m_max_tokens;
                m_last_refill = now;
                m_cv.notify_all();
            }
        }

        std::mutex m_mutex;
        std::condition_variable m_cv;
        int m_max_tokens;
        int m_tokens;
        std::chrono::milliseconds m_refill_duration;
        std::chrono::steady_clock::time_point m_last_refill;
    };
} // namespace Server::Riot