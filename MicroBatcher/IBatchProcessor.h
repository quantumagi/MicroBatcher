#include <vector>
#include <future>

namespace MicroBatcher {
    /// <summary>
    /// Processes a batch of jobs.
    /// </summary>
    template<typename TJob, typename TJobResult>
    class IBatchProcessor {
    public:
        /// <summary>
        /// Processes a batch of jobs asynchronously.
        /// </summary>
        /// <param name="jobs">A vector of jobs.</param>
        /// <returns>A future containing a vector of job results corresponding 1-to-1 with the jobs.</returns>
        virtual std::future<std::vector<TJobResult>> ProcessBatchAsync(const std::vector<TJob>& jobs) = 0;

        // Virtual destructor to ensure proper cleanup of derived classes
        virtual ~IBatchProcessor() = default;
    };
}
