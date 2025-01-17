#include "test/util/task_manager.hpp"
#include "test/util/waitable_task_runner.hpp"

#include "fly/task/task_manager.hpp"
#include "fly/task/task_runner.hpp"
#include "fly/types/concurrency/concurrent_queue.hpp"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <chrono>
#include <memory>

using namespace std::chrono_literals;

namespace {

/**
 * A task to track whether it was exected.
 */
void standalone_task(bool &task_was_called)
{
    task_was_called = true;
}

/**
 * A task to track whether it was exected.
 */
class TaskClass
{
public:
    explicit TaskClass(bool &task_was_called) : m_task_was_called(task_was_called)
    {
    }

    static void static_task(bool &task_was_called)
    {
        task_was_called = true;
    }

    void member_task()
    {
        m_task_was_called = true;
    }

private:
    bool &m_task_was_called;
};

/**
 * A task to count the number of times it is run.
 */
class CountTask
{
public:
    CountTask() noexcept : m_count(0)
    {
    }

    int get_count() const
    {
        return m_count.load();
    }

    void run()
    {
        ++m_count;
    }

private:
    std::atomic_int m_count;
};

/**
 * A task to track its execution order.
 */
class MarkerTask
{
public:
    MarkerTask(fly::ConcurrentQueue<int> *ordering) noexcept : m_ordering(ordering)
    {
    }

    void run(int marker)
    {
        m_ordering->push(std::move(marker));
    }

private:
    fly::ConcurrentQueue<int> *m_ordering;
};

/**
 * A task to track its start and end time.
 */
class TimerTask
{
public:
    TimerTask() noexcept : m_start_time(std::chrono::steady_clock::now())
    {
    }

    std::chrono::milliseconds time() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(m_stop_time - m_start_time);
    }

    void run()
    {
        m_stop_time = std::chrono::steady_clock::now();
    }

private:
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_stop_time;
};

} // namespace

CATCH_TEST_CASE("Task", "[task]")
{
    CATCH_SECTION("Tasks may be posted as lambdas")
    {
        auto task_runner = fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        auto task = [&task_was_called]()
        {
            task_was_called = true;
        };

        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::move(task)));
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
    }

    CATCH_SECTION("Tasks may be posted as mutable lambdas")
    {
        auto task_runner = fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        std::string task_id = "not set";

        auto task = [&task_was_called, task_id = std::move(task_id)]() mutable
        {
            task_was_called = true;
            task_id = "set";
        };

        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::move(task)));
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
    }

    CATCH_SECTION("Tasks may be posted as standalone functions")
    {
        auto task_runner = fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        auto task = std::bind(standalone_task, std::ref(task_was_called));

        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::move(task)));
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
    }

    CATCH_SECTION("Tasks may be posted as static class functions")
    {
        auto task_runner = fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        auto task = std::bind(TaskClass::static_task, std::ref(task_was_called));

        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::move(task)));
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
    }

    CATCH_SECTION("Tasks may be posted as member class functions")
    {
        auto task_runner = fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        TaskClass task_class(task_was_called);

        auto task = std::bind(&TaskClass::member_task, &task_class);

        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::move(task)));
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
    }

    CATCH_SECTION("Tasks may pass their result to a reply task")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        bool reply_was_called = false;

        auto task = [&task_was_called]() -> bool
        {
            task_was_called = true;
            return task_was_called;
        };
        auto reply = [&reply_was_called](bool result)
        {
            reply_was_called = result;
        };

        CATCH_REQUIRE(
            task_runner->post_task_with_reply(FROM_HERE, std::move(task), std::move(reply)));
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
        CATCH_CHECK(reply_was_called);
    }

    CATCH_SECTION("Void tasks may indicate their completion to a reply task")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        bool reply_was_called = false;

        auto task = [&task_was_called]()
        {
            task_was_called = true;
        };
        auto reply = [&reply_was_called]()
        {
            reply_was_called = true;
        };

        CATCH_REQUIRE(
            task_runner->post_task_with_reply(FROM_HERE, std::move(task), std::move(reply)));
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
        CATCH_CHECK(reply_was_called);
    }

    CATCH_SECTION("Delayed tasks execute no sooner than their specified delay")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        TimerTask task;
        const std::chrono::milliseconds delay(10);

        CATCH_REQUIRE(
            task_runner->post_task_with_delay(FROM_HERE, delay, std::bind(&TimerTask::run, &task)));
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task.time() >= delay);
    }

    CATCH_SECTION("Delayed tasks execute after immediate tasks posted at the same time")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        fly::ConcurrentQueue<int> ordering;
        MarkerTask task(&ordering);

        CATCH_REQUIRE(task_runner->post_task_with_delay(
            FROM_HERE,
            10ms,
            std::bind(&MarkerTask::run, &task, 1)));
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &task, 2)));
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &task, 3)));

        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);

        int marker = 0;
        ordering.pop(marker);
        CATCH_CHECK(marker == 2);

        ordering.pop(marker);
        CATCH_CHECK(marker == 3);

        ordering.pop(marker);
        CATCH_CHECK(marker == 1);
    }

    CATCH_SECTION("Delayed tasks may pass their result to a reply task")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        bool reply_was_called = false;

        auto task = [&task_was_called]() -> bool
        {
            task_was_called = true;
            return task_was_called;
        };
        auto reply = [&reply_was_called](bool result)
        {
            reply_was_called = result;
        };

        CATCH_REQUIRE(task_runner->post_task_with_delay_and_reply(
            FROM_HERE,
            10ms,
            std::move(task),
            std::move(reply)));
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
        CATCH_CHECK(reply_was_called);
    }

    CATCH_SECTION("Delayed void tasks may indicate their completion to a reply task")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        bool task_was_called = false;
        bool reply_was_called = false;

        auto task = [&task_was_called]()
        {
            task_was_called = true;
        };
        auto reply = [&reply_was_called]()
        {
            reply_was_called = true;
        };

        CATCH_REQUIRE(task_runner->post_task_with_delay_and_reply(
            FROM_HERE,
            10ms,
            std::move(task),
            std::move(reply)));
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);

        CATCH_CHECK(task_was_called);
        CATCH_CHECK(reply_was_called);
    }

    CATCH_SECTION("Cancelled tasks")
    {
        CATCH_SECTION("Strong tasks may be ensured to execute")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            CATCH_REQUIRE(task_runner->post_task(FROM_HERE, weak_task_class, std::move(task)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK(task_was_called);
        }

        CATCH_SECTION("Weak tasks may be cancelled")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            task_class.reset();

            CATCH_REQUIRE(task_runner->post_task(FROM_HERE, weak_task_class, std::move(task)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);
        }

        CATCH_SECTION("Weak tasks with replies may be cancelled before task")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class) -> bool
            {
                strong_task_class->member_task();
                return true;
            };
            auto reply = [&reply_was_called](bool result, std::shared_ptr<TaskClass>)
            {
                reply_was_called = result;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            task_class.reset();

            CATCH_REQUIRE(task_runner->post_task_with_reply(
                FROM_HERE,
                weak_task_class,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Weak tasks with replies may be cancelled before reply")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [&task_class](std::shared_ptr<TaskClass> strong_task_class) -> bool
            {
                strong_task_class->member_task();
                strong_task_class.reset();
                task_class.reset();
                return true;
            };
            auto reply = [&reply_was_called](bool result, std::shared_ptr<TaskClass>)
            {
                reply_was_called = result;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;

            CATCH_REQUIRE(task_runner->post_task_with_reply(
                FROM_HERE,
                weak_task_class,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Weak void tasks with replies may be cancelled before task")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
            };
            auto reply = [&reply_was_called](std::shared_ptr<TaskClass>)
            {
                reply_was_called = true;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            task_class.reset();

            CATCH_REQUIRE(task_runner->post_task_with_reply(
                FROM_HERE,
                weak_task_class,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Weak void tasks with replies may be cancelled before reply")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [&task_class](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
                strong_task_class.reset();
                task_class.reset();
            };
            auto reply = [&reply_was_called](std::shared_ptr<TaskClass>)
            {
                reply_was_called = true;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;

            CATCH_REQUIRE(task_runner->post_task_with_reply(
                FROM_HERE,
                weak_task_class,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Strong delayed tasks may be ensured to execute")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            CATCH_REQUIRE(
                task_runner
                    ->post_task_with_delay(FROM_HERE, weak_task_class, 10ms, std::move(task)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK(task_was_called);
        }

        CATCH_SECTION("Weak delayed tasks may be cancelled")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            task_class.reset();

            CATCH_REQUIRE(
                task_runner
                    ->post_task_with_delay(FROM_HERE, weak_task_class, 10ms, std::move(task)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);
        }

        CATCH_SECTION("Weak delayed tasks with replies may be cancelled before task")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class) -> bool
            {
                strong_task_class->member_task();
                return true;
            };
            auto reply = [&reply_was_called](bool result, std::shared_ptr<TaskClass>)
            {
                reply_was_called = result;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            task_class.reset();

            CATCH_REQUIRE(task_runner->post_task_with_delay_and_reply(
                FROM_HERE,
                weak_task_class,
                10ms,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Weak delayed tasks with replies may be cancelled before reply")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [&task_class](std::shared_ptr<TaskClass> strong_task_class) -> bool
            {
                strong_task_class->member_task();
                strong_task_class.reset();
                task_class.reset();
                return true;
            };
            auto reply = [&reply_was_called](bool result, std::shared_ptr<TaskClass>)
            {
                reply_was_called = result;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;

            CATCH_REQUIRE(task_runner->post_task_with_delay_and_reply(
                FROM_HERE,
                weak_task_class,
                10ms,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Weak delayed void tasks with replies may be cancelled before task")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
            };
            auto reply = [&reply_was_called](std::shared_ptr<TaskClass>)
            {
                reply_was_called = true;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;
            task_class.reset();

            CATCH_REQUIRE(task_runner->post_task_with_delay_and_reply(
                FROM_HERE,
                weak_task_class,
                10ms,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Weak delayed void tasks with replies may be cancelled before reply")
        {
            auto task_runner =
                fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

            bool task_was_called = false;
            bool reply_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);

            auto task = [&task_class](std::shared_ptr<TaskClass> strong_task_class)
            {
                strong_task_class->member_task();
                strong_task_class.reset();
                task_class.reset();
            };
            auto reply = [&reply_was_called](std::shared_ptr<TaskClass>)
            {
                reply_was_called = true;
            };

            std::weak_ptr<TaskClass> weak_task_class = task_class;

            CATCH_REQUIRE(task_runner->post_task_with_delay_and_reply(
                FROM_HERE,
                weak_task_class,
                10ms,
                std::move(task),
                std::move(reply)));
            task_runner->wait_for_task_to_complete(__FILE__);
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK(task_was_called);
            CATCH_CHECK_FALSE(reply_was_called);
        }

        CATCH_SECTION("Cancelled tasks do not execute while other tasks do execute")
        {
            auto task_runner =
                fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

            fly::ConcurrentQueue<int> ordering;
            MarkerTask marker_task(&ordering);

            bool task_was_called = false;
            auto task_class = std::make_shared<TaskClass>(task_was_called);
            std::weak_ptr<TaskClass> weak_task_class = task_class;

            auto task = [weak_task_class]()
            {
                if (auto strong_task_class = weak_task_class.lock(); strong_task_class)
                {
                    strong_task_class->member_task();
                }
            };

            task_class.reset();

            CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::move(task)));
            CATCH_REQUIRE(
                task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &marker_task, 2)));
            CATCH_REQUIRE(
                task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &marker_task, 3)));

            task_runner->wait_for_task_to_complete(__FILE__);
            task_runner->wait_for_task_to_complete(__FILE__);

            CATCH_CHECK_FALSE(task_was_called);

            int marker = 0;
            ordering.pop(marker);
            CATCH_CHECK(marker == 2);

            ordering.pop(marker);
            CATCH_CHECK(marker == 3);
        }
    }

    CATCH_SECTION("Parallel task runner does not enforce execution order")
    {
        auto task_runner = fly::test::WaitableParallelTaskRunner::create(fly::test::task_manager());

        CountTask task;
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&CountTask::run, &task)));
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&CountTask::run, &task)));
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&CountTask::run, &task)));

        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);
    }

    CATCH_SECTION("Sequenced task runner enforces execution order")
    {
        auto task_runner =
            fly::test::WaitableSequencedTaskRunner::create(fly::test::task_manager());

        fly::ConcurrentQueue<int> ordering;
        MarkerTask task(&ordering);

        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &task, 1)));
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &task, 2)));
        CATCH_REQUIRE(task_runner->post_task(FROM_HERE, std::bind(&MarkerTask::run, &task, 3)));

        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);
        task_runner->wait_for_task_to_complete(__FILE__);

        int marker = 0;
        ordering.pop(marker);
        CATCH_CHECK(marker == 1);

        ordering.pop(marker);
        CATCH_CHECK(marker == 2);

        ordering.pop(marker);
        CATCH_CHECK(marker == 3);
    }
}

CATCH_TEST_CASE("TaskManager", "[task]")
{
    auto task_manager = fly::task::TaskManager::create(1);
    CATCH_REQUIRE(task_manager);

    CATCH_SECTION("Cannot start the task manager multiple times")
    {
        CATCH_CHECK_FALSE(task_manager->start());
        CATCH_REQUIRE(task_manager->stop());
    }

    CATCH_SECTION("Cannot stop the task manager multiple times")
    {
        CATCH_REQUIRE(task_manager->stop());
        CATCH_CHECK_FALSE(task_manager->stop());
    }

    CATCH_SECTION("Parallel tasks cannot be posted after the task manager is deleted")
    {
        auto task_runner = fly::task::ParallelTaskRunner::create(task_manager);

        CATCH_REQUIRE(task_manager->stop());
        task_manager.reset();

        CATCH_CHECK_FALSE(task_runner->post_task(FROM_HERE, []() {}));
        CATCH_CHECK_FALSE(task_runner->post_task_with_delay(FROM_HERE, 0ms, []() {}));
    }

    CATCH_SECTION("Sequenced tasks cannot be posted after the task manager is deleted")
    {
        auto task_runner = fly::task::SequencedTaskRunner::create(task_manager);

        CATCH_REQUIRE(task_manager->stop());
        task_manager.reset();

        CATCH_CHECK_FALSE(task_runner->post_task(FROM_HERE, []() {}));
        CATCH_CHECK_FALSE(task_runner->post_task_with_delay(FROM_HERE, 0ms, []() {}));
    }
}
