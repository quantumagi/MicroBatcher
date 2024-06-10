#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <chrono>
#include <memory>
#include <future>
#include <stdexcept>
#include <queue>
#include "../MicroBatcher/MicroBatcher.cpp"

// Mock class for IBatchProcessor
template<typename TJob, typename TJobResult>
class MockBatchProcessor : public IBatchProcessor<TJob, TJobResult> {
public:
    MOCK_METHOD(std::vector<TJobResult>, ProcessBatch, (const std::vector<TJob>&), (override));
};

// JobResult struct for results
template<typename T>
struct JobResult {
    T result;
    explicit JobResult(T result) : result(result) {}
};

// Test cases for MicroBatcher
class MicroBatcherTests : public ::testing::TestWithParam<std::tuple<int, int, int>> {
};

INSTANTIATE_TEST_SUITE_P(
    MicroBatcherParamTests,
    MicroBatcherTests,
    ::testing::Values(
        std::make_tuple(1, 10, 3),
        std::make_tuple(10, 10, 1),
        std::make_tuple(23, 10, 3),
        std::make_tuple(3, 1, 3),
        std::make_tuple(1, 1, 1)
    )
);

TEST_P(MicroBatcherTests, SubmitJobAsync_WithGoodBatchProcessor_ProcessesJobs) {
    auto [numJobs, batchSize, maxAsyncBatches] = GetParam();

    // Create a mock batch processor.
    auto batchProcessorMock = std::make_shared<MockBatchProcessor<int, JobResult<int>>>();
    EXPECT_CALL(*batchProcessorMock, ProcessBatch(testing::_))
        .WillRepeatedly([numJobs](const std::vector<int>& jobs) {
            EXPECT_LE(jobs.size(), numJobs);
            std::vector<JobResult<int>> res;
            for (const auto& job : jobs) {
                res.emplace_back(job * 2);
            }
            return res;
        });

    // Create and start the batcher.
    auto batcher = std::make_unique<MicroBatcher<int, JobResult<int>>>(batchProcessorMock, batchSize, std::chrono::milliseconds(1000), maxAsyncBatches);
    batcher->Startup();

    // Submit the jobs.
    std::vector<std::future<JobResult<int>>> results;
    for (int i = 1; i <= numJobs; ++i) {
        results.push_back(batcher->SubmitJob(i));
    }

    // Check that the processor multiplied the input by 2.
    for (int i = 0; i < numJobs; ++i) {
        EXPECT_EQ(results[i].get().result, (i + 1) * 2);
    }

    batcher->Shutdown();
}

TEST(MicroBatcherTests, SubmitJobAsync_WithErrorInBatchProcessor_FailsJobsWithError) {
    // Create a mock batch processor that throws an error.
    auto batchProcessorMock = std::make_shared<MockBatchProcessor<int, int>>();
    EXPECT_CALL(*batchProcessorMock, ProcessBatch(testing::_))
        .WillRepeatedly(testing::Throw(std::runtime_error("Batch processor error")));

    // Create and start the batcher.
    auto batcher = std::make_unique<MicroBatcher<int, int>>(batchProcessorMock, 10, std::chrono::milliseconds(1000));
    batcher->Startup();

    // Submit a job and check that it throws an error.
    auto future = batcher->SubmitJob(1);
    try {
        future.get(); // Wait for the future to complete and throw if there's an error.
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Batch processor error");
    }

    batcher->Shutdown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
