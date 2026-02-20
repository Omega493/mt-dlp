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

#ifndef DOWNLOADER_HPP
#define DOWNLOADER_HPP

#include <algorithm>
#include <chrono>
#include <format>  // A rather heavy header only to be used for two trivial functions
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
// <charconv> may also be used

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/system.h>

// The next include is for the sole purpose of "NO_COPY_MOVE" macro.
// Given how curl is a C library and I am to use raw ptrs,
// i find it necessary to delete copy/move operations.
// I could probably use smart ptrs, but why make this unnecessarily complex
// when disabling copy/move works just fine?
// Also, don't panic if "function definition for 'NO_COPY_MOVE' not found" occurs.
// Copy/move is blocked when this is used.
#include "selena/base.hpp"
#include "selena/utils.hpp"

struct FileInfo {
  std::string resolved_url{};
  int64_t size{};
  bool supports_ranges{ false };
};

struct DownloadState {
  std::string err_msg{};
  std::mutex mtx;
  int64_t bytes_downloaded{};
  double curr_speed{ 0.0 };
  bool is_done{ false };
  bool should_retry{ false };

  DownloadState() = default;
  ~DownloadState() = default;

  NO_COPY_MOVE(DownloadState);
};

struct WorkerStats {
  std::string speed;
  std::string status;
  double progress{};
  int id{};
  bool is_done{ false };
};

struct GlobalStats {
  std::string filename;
  std::string total_speed;
  std::string eta;
  std::string downloaded_str;
  std::string status_text;
  std::vector<WorkerStats> workers;
  double total_progress;
};

class Downloader {
public:
  Downloader() { static CurlGlobal curl_global_obj{}; }
  ~Downloader() = default;
  NO_COPY_MOVE(Downloader);

  static std::string get_filename(const std::string& url);
  static std::string format_size(double bytes);

  static std::string format_time(int64_t sec);

  static FileInfo file_info(const std::string& url);

  int download(const std::string& url,
               DownloadState& state,
               std::FILE* const fp,
               std::mutex& fp_mtx,
               const int64_t range_start = -1,
               const int64_t range_end = -1);

private:
  struct CurlGlobal {
    CurlGlobal() { (void)curl_global_init(CURL_GLOBAL_ALL); }
    ~CurlGlobal() { (void)curl_global_cleanup(); }

    NO_COPY_MOVE(CurlGlobal);
  };

  struct CurlOptRes {
    const std::string desc;
    const CURLcode code;
  };

  struct Context {
    DownloadState* state;

    // Buffer to reduce disk IO
    std::vector<char> buffer;

    std::FILE* fp{ nullptr };
    std::mutex* fp_mtx;
    int64_t range_start_offset;

    /*
     * Some sites respect range requests, but don't enforce a "stop" boundary.
     * This will make threads go rogue and write into other threads' territory,
     * effectively wasting bandwidth (the output file will be bit-perfect though).
     * So, we must enforce a hard boundary. The following var specifies that.
     */
    int64_t limit_bytes;
    // Track logic progress (what we accepted) vs disk progress (what we flushed)
    int64_t bytes_processed{ 0 };
    int64_t bytes_flushed{ 0 };

    std::chrono::steady_clock::time_point last_time;
    curl_off_t last_byte_count{ 0 };
    double avg_speed{ 0.0 };

    static constexpr size_t BUFFER_THRESHOLD{ 4 * 1024 * 1024 };  // 4 MiB

    Context(DownloadState* const st,
            std::FILE* const f,
            std::mutex* const m,
            const int64_t start,
            const int64_t limit)
        : state{ st },
          fp{ f },
          fp_mtx{ m },
          range_start_offset{ ((start < 0) ? 0 : start) },
          limit_bytes{ limit },
          last_time{ std::chrono::steady_clock::now() } {
      buffer.reserve(BUFFER_THRESHOLD + 16384);
    }

    ~Context() = default;
    NO_COPY_MOVE(Context);
  };

  static size_t header_callback(const char* const buffer,
                                const size_t size,
                                const size_t nitems,
                                void* const userdata);

  // 3/5 of the arguments are unused. They are still in the function definition because curl
  // function ptr :staregePepe:
  static int progress_callback(void* const clientp,
                               const curl_off_t,
                               const curl_off_t dlnow,
                               const curl_off_t,
                               const curl_off_t);

  static size_t write_callback(const void* const contents,
                               const size_t size,
                               const size_t nmemb,
                               void* const userp);
};

#endif  // DOWNLOADER_HPP
