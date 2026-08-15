/*
 * Copyright (C) 2026 Omega493 and contributors

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MT_DLP_DOWNLOADER_HPP
#define MT_DLP_DOWNLOADER_HPP

#include "include/pch.hpp"

namespace mt_dlp {

struct download_stats_t {
  std::string error_msg_;
  double curr_speed_{};
  std::int64_t total_bytes_{};
  bool is_done_{};
};

struct file_info_t {
  std::string resolved_url_;
  std::string filename_;
  std::int64_t size_{};
  bool supports_ranges_{};
};

struct chunk_info_t {
  std::int64_t start_offs_{};
  std::int64_t end_offs_{};
};

struct curl_opt_res_t {
  std::string desc_;
  CURLcode code_{};
};

class downloader {
public:
  downloader();
  ~downloader();
  NO_COPY_MOVE(downloader);

  // Helps retrieve the file metadata, such as the file size, file name and whether the server
  // supports ranges.
  [[nodiscard]] static file_info_t get_file_metadata(
    SELENA_FUNCTION_IN const std::string& pUrl_
  );

  [[nodiscard]] static std::string format_size(const double pBytes_);

  [[nodiscard]] static std::string format_time(const std::int64_t pSec_);

  // This helper populates the download chunk_queue_ queue with the start and end offsets.
  // It must be called before calling start.
  void enqueue_chunk(
    const std::int64_t pStart_,
    const std::int64_t pEnd_
  );

  // This starts the actual downloading. It begins by pulling chunks from the queue
  // by repeatedly calling add_worker.
  void start(
    SELENA_FUNCTION_IN const std::string& pUrl_,
    SELENA_FUNCTION_IN_MODIFIED std::FILE* pFilePtr_
  );

  // This function serves as the main loop for the non-blocking network I/O.
  // It performs the following:
  // 1. Advances the state of all active network transfers via the underlying multi-handle.
  // 2. Checks for completed transfers. If a chunk finishes downloading, it checks for HTTP errors like
  // rate limiting, flushes any remaining data in memory to disk, and removes the worker handle.
  // 3. Recycles or cleans up the finished handle and decrements the active worker count.
  // 4. Pops the next chunk from the queue and assigns it to a new worker if there is still data left to download.
  // 5. Calculates the current download speed and updates the global statistics.
  void poll();
  
  [[nodiscard]] download_stats_t get_stats() const;

  static constexpr inline std::size_t kMaxConcurrentChunks{ 8 };

  // 4 MiB
  static constexpr inline std::size_t kBufferSize{ 4 * (1ULL << 20) };

  // 4 MiB
  static constexpr inline std::size_t kChunkSplitSize{ 4 * (1ULL << 20) };

  static constexpr inline std::string_view kSuffixes[]{ "B", "KiB", "MiB", "GiB" };

  // 15 MiB
  // TODO: Decide on a "good enough" limit.
  static constexpr inline std::int64_t kMinMultipartLimit{ 15 * (1ULL << 20) };

  // Don't ask me what this regex is, or how it works. The only thing I know is that it blocks
  // anything that doesn't start with a http/s. Everything else is only known by God.
  // Runnning by defining the macro MT_DLP_REGEX_TEST would generate the "mt-dlp-regex-tests.txt".
  // It should give a basic idea as to what this program considers as a "valid URL".
  static constexpr inline const char kUrlRegex[]{
    R"(^https?:\/\/(?:[^:@\s]+(?::[^:@\s]+)?@)?)"
    R"((?:(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}|\d{1,3}(?:\.\d{1,3}){3}|\[[a-fA-F0-9:]+\]))"
    R"((?::\d+)?(?:\/[-a-zA-Z0-9._~!$&'()*+,;=:@%/?# ]*)?$)"
  };

private:
  struct chunk_ctx_t {
    std::vector<char> buf_;
    std::int64_t start_offs_{};
    std::int64_t bytes_processed_{};
    std::int64_t bytes_flushed_{};
    std::FILE* file_ptr_{ nullptr };
    downloader* parent_{ nullptr };
  };
  
  // Helper for header_callback.
  static constexpr inline std::string_view kFnKey{ "filename=" };

  // Helper for header_callback.
  static constexpr inline std::string_view kFnStarKey{ "filename*=" };

  // Used in poll().
  static constexpr inline double kSmoothingConstant{ 0.225 };

  // This allows for linking a curl easy handle to its corresponding chunk context. When a write cback
  // is triggered or a worker finishes download, this allows the retrieval of whichever handle fired the task.
  std::unordered_map<CURL*, std::unique_ptr<chunk_ctx_t>> active_contexts_;
  
  // This aggregates the global state of the download.
  download_stats_t stats_;

  // It serves as a log of all the byte ranges that need to be fetched.
  // Whenever a worker is available, it pops the next available chunk.
  std::queue<chunk_info_t> chunk_queue_;
  
  std::string url_;

  // A vector acting as a LIFO stack, containing the initialized easy handles.
  // When a chunk starts downloading, it pops off of the back of the vector.
  // When a chunk finishes its download, it pushes to the back of the stack.
  // This allows for connection reuse.
  std::vector<CURL*> handle_pool_;
  
  std::chrono::steady_clock::time_point last_time_;
  std::int64_t last_bytes_downloaded_{};

  // Serves as the primary curl multi handle. This is what manages the execution of
  // multiple concurrent easy handles.
  CURLM* multi_handle_{ nullptr };

  std::FILE* file_ptr_{ nullptr };
  std::int32_t active_handles_{};

  [[nodiscard]] static std::size_t header_callback(
    SELENA_FUNCTION_IN const char* pBuffer_,
    const std::size_t pSize_,
    const std::size_t pNitems_,
    SELENA_FUNCTION_IN_MODIFIED void* pUserdata_
  );

  [[nodiscard]] static std::size_t write_callback(
    SELENA_FUNCTION_IN const void* pContents_,
    const std::size_t pSize_,
    const std::size_t pNmemb_,
    SELENA_FUNCTION_IN_MODIFIED void* pUserPtr_
  );

  [[nodiscard]] static std::string url_decode(
    SELENA_FUNCTION_IN const std::string_view pView_
  );

  // This configures an individual worker to download a specific chunk of the file.
  // It pops the next available chunk off the queue, sets up a easy handle and adds it to the
  // main multi handle.
  void add_worker();

  // Handles writing data to the disk.
  void flush_buffer(
    SELENA_FUNCTION_IN_MODIFIED chunk_ctx_t* pCtx_
  );
};

} // namespace mt_dlp

#endif // MT_DLP_DOWNLOADER_HPP
