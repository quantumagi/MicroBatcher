#include <iostream>
#include <vector>
#include <queue>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <functional>

// Interface for BatchProcessor
template<typename TJob, typename TJobResult>
class IBatchProcessor {
public:
    virtual std::vector<TJobResult> ProcessBatch(const std::vector<TJob>& jobs) = 0;
    virtual ~IBatchProcessor() = default;
};

// MicroBatcher class
template<typename TJob, typename TJobResult>
class MicroBatcher {
public:
    // Constructor
    MicroBatcher(std::shared_ptr<IBatchProcessor<TJob, TJobResult>> batchProcessor, int batchSize, std::chrono::milliseconds batchFrequency, int maxAsyncBatches = 1)
        : batchProcessor(batchProcessor), batchSize(batchSize), batchFrequency(batchFrequency), maxAsyncBatches(maxAsyncBatches), shutdownRequested(false), processing(false) {
        // Validate parameters
        if (!batchProcessor) throw std::invalid_argument("batchProcessor is null");
        if (batchSize <= 0) throw std::invalid_argument("Batch size must be greater than 0");
        if (batchFrequency <= std::chrono::milliseconds(0)) throw std::invalid_argument("Batch frequency must be greater than 0");
        if (maxAsyncBatches < 1) throw std::invalid_argument("Maximum asynchronous batches must be at least 1");
    }

    // Destructor
    ~MicroBatcher() {
        Shutdown();
    }

    // Initializes and starts the micro-batching process
    void Startup() {
        std::lock_guard<std::mutex> lock(mutex);
        // Check if processing is already started
        if (processing) return;
        shutdownRequested = false;
        processing = true;
        // Start the processing thread
        processingThread = std::thread([this] { ProcessLoop(); });
    }

    // Submits a job to be processed asynchronously
    std::future<TJobResult> SubmitJob(TJob job) {
        std::lock_guard<std::mutex> lock(mutex);
        // Check if shutdown is requested
        if (shutdownRequested) throw std::runtime_error("MicroBatcher is shutting down");
        auto promise = std::make_shared<std::promise<TJobResult>>();
        // Enqueue the job along with its promise
        jobQueue.push({ std::move(job), promise });
        queueCondition.notify_one();
        return promise->get_future();
    }

    // Asynchronously shuts down the micro-batching process
    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (shutdownRequested) return;
            shutdownRequested = true;
            queueCondition.notify_all();
        }
        if (processingThread.joinable()) {
            processingThread.join();
        }
    }

private:
    // Struct representing a job item
    struct JobItem {
        TJob job;
        std::shared_ptr<std::promise<TJobResult>> promise;
    };

    // This is the asynchronous thread used to process a batch of jobs
    void ProcessLoop() {
        while (true) {
            std::vector<JobItem> jobsToProcess;
            {
                std::unique_lock<std::mutex> lock(mutex);
                // Wait for either the queue to have jobs or shutdown is requested
                queueCondition.wait_for(lock, batchFrequency, [this] { return !jobQueue.empty() || shutdownRequested; });

                // Check if shutdown is requested and the queue is empty
                if (shutdownRequested && jobQueue.empty()) break;

                // Pop jobs from the queue to process
                while (!jobQueue.empty() && jobsToProcess.size() < batchSize) {
                    jobsToProcess.push_back(std::move(jobQueue.front()));
                    jobQueue.pop();
                }
            }

            if (!jobsToProcess.empty()) {
                try {
                    std::vector<TJob> jobs;
                    // Extract jobs and promises
                    for (auto& item : jobsToProcess) {
                        jobs.push_back(std::move(item.job));
                    }

                    // Process the batch of jobs
                    auto results = batchProcessor->ProcessBatch(jobs);

                    // Set results or exceptions for each job
                    for (size_t i = 0; i < jobsToProcess.size(); ++i) {
                        if (i < results.size()) {
                            jobsToProcess[i].promise->set_value(std::move(results[i]));
                        }
                        else {
                            jobsToProcess[i].promise->set_exception(std::make_exception_ptr(std::runtime_error("Batch processor did not return a result for the job")));
                        }
                    }
                }
                catch (...) {
                    // Set exception for each job if an error occurs
                    for (auto& item : jobsToProcess) {
                        item.promise->set_exception(std::current_exception());
                    }
                }
            }
        }
    }

    std::shared_ptr<IBatchProcessor<TJob, TJobResult>> batchProcessor;
    int batchSize;
    std::chrono::milliseconds batchFrequency;
    int maxAsyncBatches;

    std::queue<JobItem> jobQueue;
    std::mutex mutex;
    std::condition_variable queueCondition;
    std::atomic<bool> shutdownRequested;
    std::atomic<bool> processing;
    std::thread processingThread;
};
