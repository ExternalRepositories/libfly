#include "test/util/task_manager.hpp"

#include "fly/fly.hpp"
#include "fly/task/task_manager.hpp"

#include "catch2/catch_test_macros.hpp"

#include <cstdint>
#include <thread>

#if !defined(NDEBUG)
#    include <cassert>
#endif

namespace fly::test {

namespace {

    const std::uint32_t s_num_workers = std::thread::hardware_concurrency();

    /**
     * Singleton wrapper around the task manager to own the creation, set up, tear down, and
     * destruction of that task manager.
     */
    class ScopedTaskManager
    {
    public:
        static std::shared_ptr<fly::task::TaskManager> instance()
        {
            static ScopedTaskManager s_task_manager;
            return s_task_manager.m_task_manager;
        }

    private:
        ScopedTaskManager() : m_task_manager(fly::task::TaskManager::create(s_num_workers))
        {
            CATCH_REQUIRE(m_task_manager);
        }

        ~ScopedTaskManager()
        {
            // Cannot wrap with CATCH_REQUIRE because the Catch2 framework will have been torn down.
            const bool stopped = m_task_manager->stop();

#if defined(NDEBUG)
            FLY_UNUSED(stopped);
#else
            assert(stopped);
#endif
        }

        std::shared_ptr<fly::task::TaskManager> m_task_manager;
    };

} // namespace

//==================================================================================================
std::shared_ptr<fly::task::TaskManager> task_manager()
{
    return ScopedTaskManager::instance();
}

} // namespace fly::test
