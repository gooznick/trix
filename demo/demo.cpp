/*
 * trix demo — multithreaded image tracking
 *
 * Generates a rich synthetic reference image, then in a 50 Hz loop:
 *   1. Warps the previous frame by a small random transform (camera motion + noise).
 *   2. Divides the image into a grid of reference patches.
 *   3. Worker threads search for each patch in the new frame using normalised
 *      cross-correlation (cv::matchTemplate TM_CCOEFF_NORMED).
 *   4. Estimates the inter-frame translation from the high-quality patch offsets.
 *
 * Every phase is instrumented with trix so any backend (ftrace, perf, itt, etw)
 * gives a structured, multithreaded trace of the pipeline.
 *
 * Build / run:  see demo/README.md
 */

#include <trix/trix.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <vector>

#include <opencv2/imgproc.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int    IMG_W          = 640;
static constexpr int    IMG_H          = 480;
static constexpr int    NUM_FRAMES     = 200;
static constexpr double TARGET_HZ      = 50.0;
static constexpr int    NUM_WORKERS    = 4;
static constexpr int    PATCH_SIZE     = 32;   // template side length (pixels)
static constexpr int    SEARCH_RADIUS  = 8;    // ±pixels to search around reference
static constexpr int    GRID_STRIDE    = 64;   // spacing between patch centres
static constexpr float  MAX_TRANSLATE  = 4.0f; // pixels per frame
static constexpr float  MAX_ANGLE_DEG  = 0.5f; // degrees per frame
static constexpr float  NOISE_SIGMA    = 0.01f;
static constexpr float  MIN_SCORE      = 0.5f; // NCC threshold for good matches

// ─────────────────────────────────────────────────────────────────────────────
//  Thread pool
// ─────────────────────────────────────────────────────────────────────────────

class ThreadPool {
public:
    explicit ThreadPool(int n) {
        for (int i = 0; i < n; ++i)
            workers_.emplace_back([this] { run(); });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lk(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    template<class F>
    std::future<void> push(F&& f) {
        auto task = std::make_shared<std::packaged_task<void()>>(std::forward<F>(f));
        auto fut  = task->get_future();
        {
            std::unique_lock<std::mutex> lk(mu_);
            queue_.push([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

private:
    void run() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
            }
            task();
        }
    }

    std::vector<std::thread>           workers_;
    std::queue<std::function<void()>>  queue_;
    std::mutex                         mu_;
    std::condition_variable            cv_;
    bool                               stop_ = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Image utilities
// ─────────────────────────────────────────────────────────────────────────────

// Generate a rich synthetic texture (layered sinusoids) that works well with NCC.
static cv::Mat make_reference_image()
{
    cv::Mat img(IMG_H, IMG_W, CV_32F);

    for (int y = 0; y < IMG_H; ++y) {
        float* row = img.ptr<float>(y);
        for (int x = 0; x < IMG_W; ++x) {
            float v = 0.50f
                    + 0.20f * std::sin(x * 0.050f) * std::cos(y * 0.070f)
                    + 0.15f * std::sin(x * 0.130f + y * 0.090f)
                    + 0.10f * std::cos(x * 0.030f - y * 0.040f)
                    + 0.08f * std::sin(x * 0.230f) * std::sin(y * 0.190f)
                    + 0.05f * std::cos(x * 0.071f + y * 0.113f);
            row[x] = std::clamp(v, 0.f, 1.f);
        }
    }
    return img;
}

// Apply a small random affine warp and add Gaussian noise.
static cv::Mat apply_warp(const cv::Mat& src, std::mt19937& rng)
{
    std::uniform_real_distribution<float> td(-MAX_TRANSLATE, MAX_TRANSLATE);
    std::uniform_real_distribution<float> ad(-MAX_ANGLE_DEG, MAX_ANGLE_DEG);

    const cv::Point2f centre(IMG_W * 0.5f, IMG_H * 0.5f);
    cv::Mat M = cv::getRotationMatrix2D(centre, static_cast<double>(ad(rng)), 1.0);
    M.at<double>(0, 2) += td(rng);
    M.at<double>(1, 2) += td(rng);

    cv::Mat warped;
    cv::warpAffine(src, warped, M, src.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT);

    cv::Mat noise(src.size(), CV_32F);
    cv::randn(noise, 0.f, NOISE_SIGMA);
    warped += noise;
    cv::threshold(warped, warped, 1.f, 1.f, cv::THRESH_TRUNC);
    cv::threshold(warped, warped, 0.f, 0.f, cv::THRESH_TOZERO);

    return warped;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Patch grid
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<cv::Point> make_patch_grid()
{
    std::vector<cv::Point> pts;
    const int margin = PATCH_SIZE / 2 + SEARCH_RADIUS;
    for (int y = margin; y < IMG_H - margin; y += GRID_STRIDE)
        for (int x = margin; x < IMG_W - margin; x += GRID_STRIDE)
            pts.emplace_back(x, y);
    return pts;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Patch correlation
// ─────────────────────────────────────────────────────────────────────────────

struct PatchResult {
    cv::Point2f offset;
    float       score;
};

// Find the best match for one reference patch using normalised cross-correlation.
// Called concurrently from multiple worker threads.
static PatchResult search_patch(const cv::Mat& prev,
                                const cv::Mat& curr,
                                cv::Point      ref_centre)
{
    TRIX_ALGO_SCOPE("correlate");

    const int half = PATCH_SIZE / 2;
    const int R    = SEARCH_RADIUS;

    cv::Rect tmpl_rect(ref_centre.x - half, ref_centre.y - half,
                       PATCH_SIZE, PATCH_SIZE);
    cv::Rect srch_rect(ref_centre.x - half - R, ref_centre.y - half - R,
                       PATCH_SIZE + 2 * R, PATCH_SIZE + 2 * R);

    // Both rects were constructed to be in-bounds by make_patch_grid's margin.
    if (srch_rect.x < 0 || srch_rect.y < 0 ||
        srch_rect.br().x > curr.cols || srch_rect.br().y > curr.rows)
        return {{0.f, 0.f}, 0.f};

    cv::Mat ncc_map;
    cv::matchTemplate(curr(srch_rect), prev(tmpl_rect), ncc_map, cv::TM_CCOEFF_NORMED);

    double   maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(ncc_map, nullptr, &maxVal, nullptr, &maxLoc);

    // maxLoc is the top-left of the best match inside srch_rect.
    // Displacement relative to the reference centre:
    //   (srch_rect.x + maxLoc.x + half) - ref_centre.x  =  maxLoc.x - R
    return {
        { static_cast<float>(maxLoc.x - R), static_cast<float>(maxLoc.y - R) },
        static_cast<float>(maxVal)
    };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Translation estimation
// ─────────────────────────────────────────────────────────────────────────────

// Median of offsets from patches that exceeded MIN_SCORE.
static cv::Point2f estimate_translation(std::vector<PatchResult>& results)
{
    TRIX_ALGO_SCOPE("estimate");

    std::vector<float> xs, ys;
    xs.reserve(results.size());
    ys.reserve(results.size());

    for (const auto& r : results) {
        if (r.score >= MIN_SCORE) {
            xs.push_back(r.offset.x);
            ys.push_back(r.offset.y);
        }
    }

    if (xs.empty()) return {0.f, 0.f};

    auto median = [](std::vector<float>& v) {
        auto mid = v.begin() + v.size() / 2;
        std::nth_element(v.begin(), mid, v.end());
        return *mid;
    };

    return { median(xs), median(ys) };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    using Clock = std::chrono::steady_clock;
    const auto frame_period = std::chrono::duration<double>(1.0 / TARGET_HZ);

    const cv::Mat              base_image  = make_reference_image();
    const std::vector<cv::Point> patch_grid = make_patch_grid();

    std::printf("trix demo  —  image tracking pipeline\n");
    std::printf("  %dx%d image, %zu patches, %d workers, %.0f Hz, %d frames\n\n",
                IMG_W, IMG_H, patch_grid.size(), NUM_WORKERS, TARGET_HZ, NUM_FRAMES);

    ThreadPool pool(NUM_WORKERS);
    std::mt19937 rng(42);

    cv::Mat prev = base_image.clone();
    cv::Mat curr;

    std::vector<PatchResult> results(patch_grid.size());
    std::vector<std::future<void>> futures;
    futures.reserve(patch_grid.size());

    double   sum_tx = 0, sum_ty = 0;
    auto     loop_start = Clock::now();

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {

        auto frame_start = Clock::now();
        TRIX_FRAME_SCOPE(static_cast<uint64_t>(frame));

        // ── 1. Generate next frame (main thread) ─────────────────────────
        {
            TRIX_ALGO_SCOPE("generate");
            curr = apply_warp(prev, rng);
        }

        // ── 2. Dispatch patch-search tasks to the thread pool ─────────────
        {
            TRIX_ALGO_SCOPE("dispatch");
            futures.clear();
            for (std::size_t i = 0; i < patch_grid.size(); ++i) {
                futures.push_back(pool.push([&, i] {
                    results[i] = search_patch(prev, curr, patch_grid[i]);
                }));
            }
        }

        // ── 3. Wait for all workers ───────────────────────────────────────
        {
            TRIX_ALGO_SCOPE("wait");
            for (auto& f : futures) f.wait();
        }

        // ── 4. Estimate inter-frame translation ───────────────────────────
        cv::Point2f est = estimate_translation(results);

        trix_data_float("est_tx", est.x);
        trix_data_float("est_ty", est.y);

        sum_tx += est.x;
        sum_ty += est.y;

        if (frame % 50 == 0)
            std::printf("  frame %4d  est_t = (%+.2f, %+.2f) px\n",
                        frame, est.x, est.y);

        // ── 5. Advance ────────────────────────────────────────────────────
        prev = std::move(curr);

        // ── 6. Rate limit to TARGET_HZ ────────────────────────────────────
        auto deadline = frame_start + frame_period;
        if (Clock::now() < deadline)
            std::this_thread::sleep_until(deadline);
    }

    double wall = std::chrono::duration<double>(Clock::now() - loop_start).count();
    std::printf("\nProcessed %d frames in %.2f s  (%.1f Hz avg)\n",
                NUM_FRAMES, wall, NUM_FRAMES / wall);
    std::printf("Mean estimated translation: (%.3f, %.3f) px\n",
                sum_tx / NUM_FRAMES, sum_ty / NUM_FRAMES);
    return 0;
}
