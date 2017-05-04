/*****************************************************************************/
/**
* \file	ThreadSafeQueue.hpp
*
* \author	Christian Skorup
*
* Copyright &copy; Maquet Critical Care AB, Sweden
*
******************************************************************************/
#pragma once

#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <condition_variable>
#include <mutex>
#include <queue>
#include <memory>
#include <chrono>
#include <cstdint>

/******************************************************************************/
/**
*
* \brief The ThreadSafeQueue class provides a wrapper around a basic queue to provide thread safety.
*
******************************************************************************/
template <typename T>
class ThreadSafeQueue
{
public:


	/*****************************************************************************/
	/**
	* \brief Default constructor
	*
	******************************************************************************/
    ThreadSafeQueue(void)
    {
    }


	/*****************************************************************************/
	/**
	* \brief Copy constructor
	*
	* \param other Reference to other queue.
	*
	******************************************************************************/
    ThreadSafeQueue(ThreadSafeQueue const &other)
    {
        std::lock_guard<std::mutex> lock{ m_mutex };
        m_queue=other.m_queue;
    }


	/*****************************************************************************/
	/**
	* \brief Tries to pop an element from the queue.
	*
	* \param[out] out Reference to popped element.
	*
	* \return true if successful.
	*
	******************************************************************************/
    bool tryPop(T& out)
    {
        std::lock_guard<std::mutex> lock{ m_mutex };
        if (m_queue.empty())
            return false;

        out = m_queue.front();
        m_queue.pop();
        return true;
    }


	/*****************************************************************************/
	/**
	* \brief Tries to pop an element from the queue.
	*
	* \return A shared_ptr to the element if successful, an empty pointer otherwhise.
	*
	******************************************************************************/
    std::shared_ptr<T> tryPop()
    {
        std::lock_guard<std::mutex> lock{ m_mutex };
        if (m_queue.empty())
            return std::shared_ptr<T>();

        std::shared_ptr<T> res(std::make_shared<T>(m_queue.front()));
        m_queue.pop();
        return res;
    }


	/*****************************************************************************/
	/**
	* \brief Blocking pop of an element from the queue.
	*
	* \param[out] out Reference to popped element.
	*
	******************************************************************************/
    void waitPop(T& out)
    {
        std::unique_lock<std::mutex> lock{ m_mutex };
        m_condition.wait(lock, [this]() {return !m_queue.empty();});
        out = m_queue.front();
        m_queue.pop();
    }


	/*****************************************************************************/
	/**
	* \brief Blocking pop of an element from the queue.
	*
	* \return A shared_ptr to the element.
	*
	******************************************************************************/
    std::shared_ptr<T> waitPop()
    {
        std::unique_lock<std::mutex> lock{ m_mutex };
        m_condition.wait(lock, [this]() {return !m_queue.empty();});
        std::shared_ptr<T> res(std::make_shared<T>(m_queue.front()));
        m_queue.pop();
        return res;
    }


	/*****************************************************************************/
	/**
	* \brief Tries to pop an element from the queue.
	*
	* \param[out] out Reference to popped element.
	* \param[in] milliSeconds Pop timeout (in milliseconds).
	*
	* \return true if successful.
	*
	******************************************************************************/
    bool waitPop(T& out, std::uint32_t milliSeconds)
    {
        std::unique_lock<std::mutex> lock{ m_mutex };
        m_condition.wait_for(lock, std::chrono::milliseconds(milliSeconds), [this]() {return !m_queue.empty();});
        if (m_queue.empty())
            return false;
        out = m_queue.front();
        m_queue.pop();
        return true;
    }


	/*****************************************************************************/
	/**
	* \brief Push an element into the queue.
	*
	* \param value The element.
	*
	******************************************************************************/
    void push(T value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_condition.notify_one();
    }


	/*****************************************************************************/
	/**
	* \brief Check if the queue is empty.
	*
	* \return true if the queue is empty, false otherwhise.
	*
	******************************************************************************/
    bool empty(void) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }


	/*****************************************************************************/
	/**
	* \brief Clear the queue.
	*
	******************************************************************************/
    void clear(void)
    {
        std::lock_guard<std::mutex> lock{ m_mutex };
        while (!m_queue.empty())
        {
            m_queue.pop();
        }
        m_condition.notify_all();
    }


private:
    mutable std::mutex m_mutex;		///< Mutex used for sychronization.
    std::queue<T> m_queue;			///< Queue of elements.
    std::condition_variable m_condition;	///< Condition variable used for sychronization.
};

#endif
