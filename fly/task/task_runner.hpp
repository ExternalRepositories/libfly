#pragma once

#include "fly/fly.hpp"
#include "fly/task/task_types.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>

/**
 * Helper macro to create a TaskLocation from the current location.
 */
#define FROM_HERE                                                                                  \
    fly::task::TaskLocation({__FILE__, __FUNCTION__, static_cast<std::uint32_t>(__LINE__)})

namespace fly::task {

class TaskManager;

/**
 * Base class for controlling the execution of tasks. Concrete task runners control the ordering and
 * execution of tasks.
 *
 * Tasks may generally be any callable type (lambda, std::function, etc.). Specific posting methods
 * may place restrictions on the callable type, on either the return type of the invocation or the
 * arguments the task accepts.
 *
 * Tasks whose result is a non-void type may pass their result to a reply task. For example:
 *
 *       auto task = []() -> int
 *       {
 *           // Task body here.
 *           return 12389;
 *       };
 *
 *       auto reply = [](int task_result)
 *       {
 *           assert(task_result == 12389);
 *           // Reply body here.
 *       };
 *
 *       task_runner->post_task_with_reply(FROM_HERE, std::move(task), std::move(reply));
 *
 * Tasks whose result is void may indicate their completion to a reply task. For example:
 *
 *       auto task = []()
 *       {
 *           // Task body here.
 *       };
 *
 *       auto reply = []()
 *       {
 *           // Reply body here.
 *       };
 *
 *       task_runner->post_task_with_reply(FROM_HERE, std::move(task), std::move(reply));
 *
 * Mismatching of task result types and reply parameter types is explicitly forbidden at compile
 * time. Tasks that return a non-void result must be paired with a reply which is invocable with
 * only that type. Tasks which return void must be paired with a reply that is invocable without
 * arguments.
 *
 * Reply tasks are not executed immediately after a task is complete. Rather, they are posted for
 * execution on the same task runner on which that task was posted.
 *
 * Once a task is posted, it may be attempted to be cancelled in a number of ways:
 *
 * 1. Use one of the posting methods which accepts a weak pointer to the owner of the task. When the
 *    task is ready to be executed, if the weak pointer cannot be promoted to a strong pointer, the
 *    task is dropped. The task must accept a single argument, the shared pointer obtained from the
 *    weak pointer. For example:
 *
 *       auto task = [](std::shared_ptr<MyClass> self)
 *       {
 *           // Task body here.
 *       };
 *
 *       std::weak_ptr<MyClass> weak_self = shared_from_this();
 *       task_runner->post_task(FROM_HERE, weak_self, std::move(task));
 *
 *    Reply tasks may be cancelled in the same manner. The reply task must then accept the result of
 *    the task and the shared pointer obtained from the weak pointer. If the task was dropped due to
 *    being unable to promote the weak pointer, the reply is also dropped. If the task was executed,
 *    when the reply is ready to be executed, if the weak pointer cannot be promoted to a strong
 *    pointer, the reply is dropped. For example:
 *
 *       auto task = [](std::shared_ptr<MyClass> self) -> int
 *       {
 *           // Task body here.
 *           return 12389;
 *       };
 *
 *       auto reply = [](int task_result, std::shared_ptr<MyClass> self)
 *       {
 *           assert(task_result == 12389);
 *           // Reply body here.
 *       };
 *
 *       std::weak_ptr<MyClass> weak_self = shared_from_this();
 *       task_runner->post_task_with_reply(FROM_HERE, weak_self, std::move(task), std::move(reply));
 *
 * 2. Deleting the task runner onto which the task was posted. This will only cancel the task if the
 *    task manager has not yet instructed the task runner to execute the task.
 *
 * @author Timothy Flynn (trflynn89@pm.me)
 * @version August 12, 2018
 */
class TaskRunner : public std::enable_shared_from_this<TaskRunner>
{
    friend class TaskManager;

public:
    /**
     * Destructor.
     */
    virtual ~TaskRunner() = default;

    /**
     * Post a task for execution. The task may be any callable type.
     *
     * @tparam TaskType Callable type of the task.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution.
     */
    template <typename TaskType>
    bool post_task(TaskLocation &&location, TaskType &&task);

    /**
     * Post a task for execution with protection by the provided weak pointer. The task may be any
     * callable type which accepts a single argument, a locked shared pointer obtained from the weak
     * pointer. When the task is ready to be executed, if the weak pointer fails to be locked, the
     * task is dropped.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam OwnerType Type of the owner of the task.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param weak_owner A weak pointer to the owner of the task.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution.
     */
    template <typename OwnerType, typename TaskType>
    bool post_task(TaskLocation &&location, std::weak_ptr<OwnerType> weak_owner, TaskType &&task);

    /**
     * Post a task for execution. The task may be any callable type.
     *
     * When the task has been executed, the reply task is then posted for execution on this same
     * task runner. The reply task may be any callable type that is invocable with the return type
     * of the task (if non-void), or without any arguments (if void).
     *
     * @tparam TaskType Callable type of the task.
     * @tparam ReplyType Callable type of the reply.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param task The task to be executed.
     * @param reply The reply to be executed with the result of the task.
     *
     * @return True if the task was posted for execution.
     */
    template <typename TaskType, typename ReplyType>
    bool post_task_with_reply(TaskLocation &&location, TaskType &&task, ReplyType &&reply);

    /**
     * Post a task for execution with protection by the provided weak pointer. The task may be any
     * callable type which accepts a single argument, a locked shared pointer obtained from the weak
     * pointer. When the task is ready to be executed, if the weak pointer fails to be locked, the
     * task is dropped.
     *
     * When the task has been executed, the reply task is then posted for execution on this same
     * task runner with protection by the same weak pointer. The reply task may be any callable type
     * that is invocable with the return type of the task (if non-void) and a locked shared pointer
     * obtained from the weak pointer, or with only the locked shared pointer. When the reply is
     * ready to be executed, if the weak pointer fails to be locked, the reply is dropped.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam ReplyType Callable type of the reply.
     * @tparam OwnerType Type of the owner of the task.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param weak_owner A weak pointer to the owner of the task.
     * @param task The task to be executed.
     * @param reply The reply to be executed with the result of the task.
     *
     * @return True if the task was posted for execution.
     */
    template <typename OwnerType, typename TaskType, typename ReplyType>
    bool post_task_with_reply(
        TaskLocation &&location,
        std::weak_ptr<OwnerType> weak_owner,
        TaskType &&task,
        ReplyType &&reply);

    /**
     * Schedule a task to be posted after a delay. The task may be any callable type.
     *
     * @tparam TaskType Callable type of the task.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param delay Delay before posting the task.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for delayed execution.
     */
    template <typename TaskType>
    bool
    post_task_with_delay(TaskLocation &&location, std::chrono::milliseconds delay, TaskType &&task);

    /**
     * Schedule a task to be posted after a delay with protection by the provided weak pointer. The
     * task may be any callable type which accepts a single argument, a locked shared pointer
     * obtained from the weak pointer. When the task is ready to be executed, if the weak pointer
     * fails to be locked, the task is dropped.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam OwnerType Type of the owner of the task.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param weak_owner A weak pointer to the owner of the task.
     * @param delay Delay before posting the task.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for delayed execution.
     */
    template <typename OwnerType, typename TaskType>
    bool post_task_with_delay(
        TaskLocation &&location,
        std::weak_ptr<OwnerType> weak_owner,
        std::chrono::milliseconds delay,
        TaskType &&task);

    /**
     * Schedule a task to be posted after a delay. The task may be any callable type.
     *
     * When the task has been executed, the reply task is then posted for execution on this same
     * task runner. The reply task may be any callable type that is invocable with the return type
     * of the task (if non-void), or without any arguments (if void).
     *
     * @tparam TaskType Callable type of the task.
     * @tparam ReplyType Callable type of the reply.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param delay Delay before posting the task.
     * @param task The task to be executed.
     * @param reply The reply to be executed with the result of the task.
     *
     * @return True if the task was posted for execution.
     */
    template <typename TaskType, typename ReplyType>
    bool post_task_with_delay_and_reply(
        TaskLocation &&location,
        std::chrono::milliseconds delay,
        TaskType &&task,
        ReplyType &&reply);

    /**
     * Schedule a task to be posted after a delay with protection by the provided weak pointer. The
     * task may be any callable type which accepts a single argument, a locked shared pointer
     * obtained from the weak pointer. When the task is ready to be executed, if the weak pointer
     * fails to be locked, the task is dropped.
     *
     * When the task has been executed, the reply task is then posted for execution on this same
     * task runner with protection by the same weak pointer. The reply task may be any callable type
     * that is invocable with the return type of the task (if non-void) and a locked shared pointer
     * obtained from the weak pointer, or with only the locked shared pointer. When the reply is
     * ready to be executed, if the weak pointer fails to be locked, the reply is dropped.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam ReplyType Callable type of the reply.
     * @tparam OwnerType Type of the owner of the task.
     *
     * @param location The location from which the task was posted (use FROM_HERE).
     * @param weak_owner A weak pointer to the owner of the task.
     * @param delay Delay before posting the task.
     * @param task The task to be executed.
     * @param reply The reply to be executed with the result of the task.
     *
     * @return True if the task was posted for execution.
     */
    template <typename OwnerType, typename TaskType, typename ReplyType>
    bool post_task_with_delay_and_reply(
        TaskLocation &&location,
        std::weak_ptr<OwnerType> weak_owner,
        std::chrono::milliseconds delay,
        TaskType &&task,
        ReplyType &&reply);

protected:
    /**
     * Private constructor. Task runners may only be created by the task manager.
     *
     * @param task_manager The task manager.
     */
    TaskRunner(std::shared_ptr<TaskManager> task_manager) noexcept;

    /**
     * Post a task for execution in accordance with the concrete task runner's policy.
     *
     * @param location The location from which the task was posted.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution.
     */
    virtual bool post_task_internal(TaskLocation &&location, Task &&task) = 0;

    /**
     * Completion notification triggered by the task manager that a task has finished execution.
     *
     * @param location The location from which the task was posted.
     */
    virtual void task_complete(TaskLocation &&location) = 0;

    /**
     * Forward a task to the task manager to be executed as soon as a worker thread is available.
     *
     * @param location The location from which the task was posted.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution.
     */
    bool post_task_to_task_manager(TaskLocation &&location, Task &&task);

    /**
     * Forward a task to the task manager to be scheduled for excution after a delay. The task will
     * be stored on the task manager's timer thread. Once the given delay has expired, the task will
     * be handed back to the task runner to govern when the task will be posted from there.
     *
     * @param location The location from which the task was posted.
     * @param delay Delay before posting the task.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for delayed execution.
     */
    bool post_task_to_task_manager_with_delay(
        TaskLocation &&location,
        std::chrono::milliseconds delay,
        Task &&task);

private:
    /**
     * Container to wrap around a generic callable type to allow perfect forwarding into lambdas.
     */
    template <typename TaskType>
    struct TaskHolder
    {
        TaskType m_task;
    };

    /**
     * Wrap a task in a generic lambda to be agnostic to the return type of the task.
     *
     * @tparam TaskType Callable type of the task.
     *
     * @param task The task to be executed.
     *
     * @return The wrapped task.
     */
    template <typename TaskType>
    Task wrap_task(TaskType &&task);

    /**
     * Wrap a task in a generic lambda to be agnostic to the return type of the task. When the task
     * is ready to be executed, if the provided weak pointer fails to be locked, the task is
     * dropped.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam OwnerType Type of the owner of the task.
     *
     * @param weak_owner A weak pointer to the owner of the task.
     * @param task The task to be executed.
     *
     * @return The wrapped task.
     */
    template <typename OwnerType, typename TaskType>
    Task wrap_task(std::weak_ptr<OwnerType> weak_owner, TaskType &&task);

    /**
     * Wrap a task in a generic lambda to be agnostic to the return type of the task.
     *
     * When the task has been executed, the result of the task (if any) is bound to the provided
     * reply task. The reply task is then posted for execution on this same task runner.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam ReplyType Callable type of the reply.
     *
     * @param task The task to be executed.
     * @param reply The reply to be executed with the result of the task.
     *
     * @return The wrapped task.
     */
    template <typename TaskType, typename ReplyType>
    Task wrap_task(TaskType &&task, ReplyType &&reply);

    /**
     * Wrap a task in a generic lambda to be agnostic to the return type of the task. When the task
     * is ready to be executed, if the provided weak pointer fails to be locked, the task is
     * dropped.
     *
     * When the task has been executed, the result of the task (if any) is bound to the provided
     * reply task. The reply task is then posted for execution on this same task runner with
     * protection by the same weak pointer.
     *
     * @tparam TaskType Callable type of the task.
     * @tparam ReplyType Callable type of the reply.
     * @tparam OwnerType Type of the owner of the task.
     *
     * @param weak_owner A weak pointer to the owner of the task.
     * @param task The task to be executed.
     * @param reply The reply to be executed with the result of the task.
     *
     * @return The wrapped task.
     */
    template <typename OwnerType, typename TaskType, typename ReplyType>
    Task wrap_task(std::weak_ptr<OwnerType> weak_owner, TaskType &&task, ReplyType &&reply);

    /**
     * Execute a task.
     *
     * @param location The location from which the task was posted.
     * @param task The task to be executed.
     */
    void execute(TaskLocation &&location, Task &&task);

    std::weak_ptr<TaskManager> m_weak_task_manager;
};

/**
 * Task runner implementation for executing tasks in parallel. Tasks posted to this task runner may
 * be executed in any order.
 *
 * @author Timothy Flynn (trflynn89@pm.me)
 * @version August 12, 2018
 */
class ParallelTaskRunner : public TaskRunner
{
public:
    /**
     * Create a parallel task runner.
     *
     * @param task_manager The task manager this runner should interface with.
     *
     * @return The created task runner.
     */
    static std::shared_ptr<ParallelTaskRunner> create(std::shared_ptr<TaskManager> task_manager);

protected:
    explicit ParallelTaskRunner(std::shared_ptr<TaskManager> task_manager) noexcept;

    /**
     * Post a task for execution immediately.
     *
     * @param location The location from which the task was posted.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution.
     */
    bool post_task_internal(TaskLocation &&location, Task &&task) override;

    /**
     * This implementation does nothing.
     *
     * @param location The location from which the task was posted.
     */
    void task_complete(TaskLocation &&location) override;
};

/**
 * Task runner implementation for executing tasks in sequence. Only one task posted to this task
 * runner will execute at a time. Tasks are executed in a FIFO manner; once one task completes, the
 * next task in line will be posted for execution.
 *
 * The caveat is with delayed tasks. If task A is posted with some delay, then task B is posted with
 * no delay, task B will be posted for execution first. Task A will only be posted for execution
 * once its delay has expired.
 *
 * @author Timothy Flynn (trflynn89@pm.me)
 * @version August 12, 2018
 */
class SequencedTaskRunner : public TaskRunner
{
public:
    /**
     * Create a sequenced task runner.
     *
     * @param task_manager The task manager this runner should interface with.
     *
     * @return The created task runner.
     */
    static std::shared_ptr<SequencedTaskRunner> create(std::shared_ptr<TaskManager> task_manager);

protected:
    explicit SequencedTaskRunner(std::shared_ptr<TaskManager> task_manager) noexcept;

    /**
     * Post a task for execution within this sequence. If a task is not already running, the task is
     * posted for execution immediately. Otherwise, the task is queued until the currently running
     * task (and all tasks queued before it) have completed.
     *
     * @param location The location from which the task was posted.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution.
     */
    bool post_task_internal(TaskLocation &&location, Task &&task) override;

    /**
     * When a task is complete, post the next task in the pending queue.
     *
     * @param location The location from which the task was posted.
     */
    void task_complete(TaskLocation &&location) override;

private:
    /**
     * Structure to hold a task until it is ready to be executed within its sequence.
     */
    struct PendingTask
    {
        TaskLocation m_location;
        Task m_task;
    };

    /**
     * If no task has been posted for execution, post either the first task in the pending queue (if
     * there is one) or the given task (if non-null). If a task is currently posted for execution,
     * or if there was a task in the pending queue queue, add the given task to the queue.
     *
     * @param location The location from which the task was posted.
     * @param task The task to be executed.
     *
     * @return True if the task was posted for execution or added to the pending queue.
     */
    bool maybe_post_task(TaskLocation &&location, Task &&task);

    std::mutex m_pending_tasks_mutex;
    std::queue<PendingTask> m_pending_tasks;
    bool m_has_running_task {false};
};

//==================================================================================================
template <typename TaskType>
bool TaskRunner::post_task(TaskLocation &&location, TaskType &&task)
{
    return post_task_internal(std::move(location), wrap_task(std::forward<TaskType>(task)));
}

//==================================================================================================
template <typename OwnerType, typename TaskType>
bool TaskRunner::post_task(
    TaskLocation &&location,
    std::weak_ptr<OwnerType> weak_owner,
    TaskType &&task)
{
    return post_task_internal(
        std::move(location),
        wrap_task(std::move(weak_owner), std::forward<TaskType>(task)));
}

//==================================================================================================
template <typename TaskType, typename ReplyType>
bool TaskRunner::post_task_with_reply(TaskLocation &&location, TaskType &&task, ReplyType &&reply)
{
    return post_task_internal(
        std::move(location),
        wrap_task(std::forward<TaskType>(task), std::forward<ReplyType>(reply)));
}

//==================================================================================================
template <typename OwnerType, typename TaskType, typename ReplyType>
bool TaskRunner::post_task_with_reply(
    TaskLocation &&location,
    std::weak_ptr<OwnerType> weak_owner,
    TaskType &&task,
    ReplyType &&reply)
{
    return post_task_internal(
        std::move(location),
        wrap_task(
            std::move(weak_owner),
            std::forward<TaskType>(task),
            std::forward<ReplyType>(reply)));
}

//==================================================================================================
template <typename TaskType>
bool TaskRunner::post_task_with_delay(
    TaskLocation &&location,
    std::chrono::milliseconds delay,
    TaskType &&task)
{
    return post_task_to_task_manager_with_delay(
        std::move(location),
        std::move(delay),
        wrap_task(std::forward<TaskType>(task)));
}

//==================================================================================================
template <typename OwnerType, typename TaskType>
bool TaskRunner::post_task_with_delay(
    TaskLocation &&location,
    std::weak_ptr<OwnerType> weak_owner,
    std::chrono::milliseconds delay,
    TaskType &&task)
{
    return post_task_to_task_manager_with_delay(
        std::move(location),
        std::move(delay),
        wrap_task(std::move(weak_owner), std::forward<TaskType>(task)));
}

//==================================================================================================
template <typename TaskType, typename ReplyType>
bool TaskRunner::post_task_with_delay_and_reply(
    TaskLocation &&location,
    std::chrono::milliseconds delay,
    TaskType &&task,
    ReplyType &&reply)
{
    return post_task_to_task_manager_with_delay(
        std::move(location),
        std::move(delay),
        wrap_task(std::forward<TaskType>(task), std::forward<ReplyType>(reply)));
}

//==================================================================================================
template <typename OwnerType, typename TaskType, typename ReplyType>
bool TaskRunner::post_task_with_delay_and_reply(
    TaskLocation &&location,
    std::weak_ptr<OwnerType> weak_owner,
    std::chrono::milliseconds delay,
    TaskType &&task,
    ReplyType &&reply)
{
    return post_task_to_task_manager_with_delay(
        std::move(location),
        std::move(delay),
        wrap_task(std::move(weak_owner), std::forward<TaskType>(task), std::move(reply)));
}

//==================================================================================================
template <typename TaskType>
Task TaskRunner::wrap_task(TaskType &&task)
{
    static_assert(std::is_invocable_v<TaskType>, "Task must be invocable without any arguments");

    TaskHolder<TaskType> holder {std::forward<TaskType>(task)};

    return [holder = std::move(holder)](TaskRunner *, TaskLocation) mutable
    {
        FLY_UNUSED(std::invoke(std::move(holder.m_task)));
    };
}

//==================================================================================================
template <typename OwnerType, typename TaskType>
Task TaskRunner::wrap_task(std::weak_ptr<OwnerType> weak_owner, TaskType &&task)
{
    using StrongOwnerType = std::shared_ptr<OwnerType>;

    static_assert(
        std::is_invocable_v<TaskType, StrongOwnerType>,
        "Task must be invocable with only a strong pointer to its owner");

    TaskHolder<TaskType> holder {std::forward<TaskType>(task)};

    return [weak_owner = std::move(weak_owner),
            holder = std::move(holder)](TaskRunner *, TaskLocation) mutable
    {
        if (StrongOwnerType owner = weak_owner.lock(); owner)
        {
            FLY_UNUSED(std::invoke(std::move(holder.m_task), std::move(owner)));
        }
    };
}

//==================================================================================================
template <typename TaskType, typename ReplyType>
Task TaskRunner::wrap_task(TaskType &&task, ReplyType &&reply)
{
    static_assert(std::is_invocable_v<TaskType>, "Task must be invocable without any arguments");

    using ResultType = std::invoke_result_t<TaskType>;
    static constexpr bool s_result_is_void = std::is_void_v<ResultType>;

    static_assert(
        (s_result_is_void && std::is_invocable_v<ReplyType>) ||
            (!s_result_is_void && std::is_invocable_v<ReplyType, ResultType>),
        "Either the task must return a non-void type and the reply must be invocable with only "
        "that type, or the task must return void and the reply must be invocable without any "
        "arguments");

    TaskHolder<TaskType> task_holder {std::forward<TaskType>(task)};
    TaskHolder<ReplyType> reply_holder {std::forward<ReplyType>(reply)};

    return
        [task_holder = std::move(task_holder),
         reply_holder = std::move(reply_holder)](TaskRunner *runner, TaskLocation location) mutable
    {
        if constexpr (s_result_is_void)
        {
            std::invoke(std::move(task_holder.m_task));
            runner->post_task(std::move(location), std::move(reply_holder.m_task));
        }
        else
        {
            auto result = std::invoke(std::move(task_holder.m_task));

            runner->post_task(
                std::move(location),
                std::bind(std::move(reply_holder.m_task), std::move(result)));
        }
    };
}

//==================================================================================================
template <typename OwnerType, typename TaskType, typename ReplyType>
Task TaskRunner::wrap_task(std::weak_ptr<OwnerType> weak_owner, TaskType &&task, ReplyType &&reply)
{
    using StrongOwnerType = std::shared_ptr<OwnerType>;

    static_assert(
        std::is_invocable_v<TaskType, StrongOwnerType>,
        "Task must be invocable with only a strong pointer to its owner");

    using ResultType = std::invoke_result_t<TaskType, StrongOwnerType>;
    static constexpr bool s_result_is_void = std::is_void_v<ResultType>;

    static_assert(
        (s_result_is_void && std::is_invocable_v<ReplyType, StrongOwnerType>) ||
            (!s_result_is_void && std::is_invocable_v<ReplyType, ResultType, StrongOwnerType>),
        "Either the task must return a non-void type and the reply must be invocable with that "
        "type and a strong pointer to its owner, or the task must return void and the reply must "
        "be invocable with only a strong pointer to its owner");

    TaskHolder<TaskType> task_holder {std::forward<TaskType>(task)};
    TaskHolder<ReplyType> reply_holder {std::forward<ReplyType>(reply)};

    return
        [weak_owner = std::move(weak_owner),
         task_holder = std::move(task_holder),
         reply_holder = std::move(reply_holder)](TaskRunner *runner, TaskLocation location) mutable
    {
        if (StrongOwnerType owner = weak_owner.lock(); owner)
        {
            if constexpr (s_result_is_void)
            {
                std::invoke(std::move(task_holder.m_task), std::move(owner));

                runner->post_task(
                    std::move(location),
                    std::move(weak_owner),
                    std::move(reply_holder.m_task));
            }
            else
            {
                auto result = std::invoke(std::move(task_holder.m_task), std::move(owner));

                runner->post_task(
                    std::move(location),
                    std::move(weak_owner),
                    std::bind(
                        std::move(reply_holder.m_task),
                        std::move(result),
                        std::placeholders::_1));
            }
        }
    };
}

} // namespace fly::task
