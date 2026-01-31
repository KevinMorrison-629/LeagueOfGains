#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace Core::Utils
{
    template <typename T> class ThreadsafeQueue
    {
    public:
        /// @brief Default Constructor
        ThreadsafeQueue() = default;

        /// @brief Pushes a new element to the back of the queue
        /// This operation is thread-safe. It locks the queue, adds the element,
        /// and then notifies one waiting thread that a new element is available.
        /// @param value The element to be added to the queue
        void push(T value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(value));
            m_cond.notify_one();
        }

        /// @brief Tries to pop an element from the queue without blocking.
        /// @param[out] value Reference to store the popped element.
        /// @return true if an element was popped, false if the queue was empty.
        bool try_pop(T &value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                return false;
            }
            value = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        /// @brief Checks if the queue is empty.
        /// This operation is thread-safe
        /// @return true if the queue is empty, false otherwise
        bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        /// @brief Gets the number of elements in the queue.
        /// This operation is thread-safe
        /// @return The number of elements currently in the queue
        size_t size() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

    private:
        /// @brief The underlying std::queue that stores the elements.
        std::queue<T> m_queue;

        /// @brief Mutex to protect access to the queue.
        /// mutable is used to allow locking in const member functions like empty() and size().
        mutable std::mutex m_mutex;

        /// @brief Condition variable to signal waiting threads.
        std::condition_variable m_cond;
    };
} // namespace Core::Utils