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

#include "include/pch.hpp"

#define SELENA_ARENA_OVERLOAD_NEW_DELETE
#define SELENA_ARENA_OVERLOAD_NOTHROW_NEW_DELETE
#define SELENA_ARENA_OVERLOAD_ALIGNED_NEW_DELETE
#include "selena/cpp/allocators/static_arena_alloc.hpp"

#include "src/core/downloader/downloader.hpp"

// #define MT_DLP_REGEX_TEST
// #define MT_DLP_TEST_LOCAL_DOWNLOAD

#ifndef MT_DLP_REGEX_TEST
#include "src/core/ui/ui.hpp"
#endif // ^^^ MT_DLP_REGEX_TEST ^^^

#ifdef MT_DLP_REGEX_TEST
# define I_D_LIKE_TO_TEST_MT_DLP
# include <fstream>
# include "selena/cpp/containers/unordered_set.hpp"
# include "tests/tests.hpp"
#endif // ^^^ MT_DLP_REGEX_TEST

#ifdef _WIN32
# define getpid _getpid
#endif // ^^^ _WIN32 ^^^

int main(const int argc, const char* const* const argv) {
  std::signal(SIGINT, [](const int signum_) {
    char buf_[80]{};
    const auto x_{
      std::snprintf(buf_, sizeof(buf_), "\nProcess ID: %d\nSIGINT (external interrupt) sent to program.\n",
        getpid()
    ) };
    stderr_write(buf_);
    std::_Exit(signum_);
  });

  std::signal(SIGTERM, [](const int signum_) {
    char buf_[80]{};
    const auto x_{
      std::snprintf(buf_, sizeof(buf_), "\nProcess ID: %d\nSIGTERM (termination request) sent to program.\n",
        getpid()
    ) };
    stderr_write(buf_);
    std::_Exit(signum_);
  });

#ifdef _WIN32
# undef getpid
  LPTOP_LEVEL_EXCEPTION_FILTER prev_handler_{ SetUnhandledExceptionFilter(panic_handler) };
#elifdef __linux__ // ^^^ _WIN32 / __linux__ vvv
  struct sigaction signal_action_{};
  signal_action_.sa_sigaction = panic_handler;
  signal_action_.sa_flags = SA_SIGINFO;
  sigemptyset(&signal_action_.sa_mask);
  sigaction(SIGABRT, &signal_action_, nullptr);
  sigaction(SIGFPE, &signal_action_, nullptr);
  sigaction(SIGILL, &signal_action_, nullptr);
  sigaction(SIGSEGV, &signal_action_, nullptr);
  // Ignore SIGINT and SIGTERM
  // SIGINT is initiated by user
  // SIGTERM is sent to the program
  // Neither of the two are caused by the program itself
#endif // ^^^ _WIN32 ^^^

  try {
#ifndef MT_DLP_REGEX_TEST
    if (argc < 2) {
      std::println(
        stderr,
        "Usage:\n"
        "  mt-dlp <URL>"
      );
      return 1;
    }

    const std::chrono::steady_clock::time_point start{ std::chrono::steady_clock::now() };

    const std::string url{ argv[1] };
#endif // ^^^ MT_DLP_REGEX_TEST ^^^
   
    static const std::regex url_regex{ mt_dlp::downloader::kUrlRegex };

#ifndef MT_DLP_REGEX_TEST
    if (!std::regex_match(url, url_regex)) {
      std::println(
        stderr,
        SELENA_STR_COLORS_RED_LIGHT
        "[mt-dlp] It looks like you provided an invalid URL."
        SELENA_STR_COLORS_RESET
      );
      return 1;
    }
#endif // ^^^ MT_DLP_REGEX_TEST ^^^

#ifdef MT_DLP_REGEX_TEST
    selena::unordered_set<std::string_view, sizeof(mt_dlp_tests::kTestUrls) / sizeof(std::string_view), 0.75L> regex_test_set;

    std::ofstream file{ "mt-dlp-regex-tests.txt" };

    if (!file.is_open()) {
      std::println(
        stderr,
        SELENA_STR_COLORS_RED_LIGHT
        "[mt-dlp tests] Unable to open tests file."
        SELENA_STR_COLORS_RESET
      );

      return 1;
    }

    int num_true{}, num_false{};

    for (const std::string_view& view : mt_dlp_tests::kTestUrls) {
      if (regex_test_set.contains(view)) {
        std::println(
          stderr,
          SELENA_STR_COLORS_RED_LIGHT
          "[mt-dlp tests] Assertion failed - URL {} was provided twice.\n"
          "[mt-dlp tests] Assertion was that every URL must be unique."
          SELENA_STR_COLORS_RESET,
          view
        );

        return 1;
      }

      regex_test_set.insert(view);

      const bool res{ std::regex_match(std::string{ view }, url_regex) };

      file << std::format("{} {}\n", view, res);

      if (res) {
        ++num_true;
      } else {
        ++num_false;
      }
    }

    std::println("[mt-dlp tests] Num true: {}, Num false: {}", num_true, num_false);

    return 0;
#endif // ^^^ MT_DLP_REGEX_TEST ^^^

#ifndef MT_DLP_REGEX_TEST

    mt_dlp::file_info_t file_info;

    try {
      file_info = mt_dlp::downloader::get_file_metadata(url);
    } catch (const std::exception& e) {
      std::println(
        stderr,
        SELENA_STR_COLORS_RED_LIGHT
        "[mt-dlp] Initialization error: {}"
        SELENA_STR_COLORS_RESET,
        e.what()
      );
      return 1;
    }

    // An immutable view of the actual struct instant.
    const mt_dlp::file_info_t& info{ file_info };

    const std::string filename{ info.filename_ };

    std::FILE* fp{ nullptr };

#ifdef _WIN32
    // As per MSVC, fopen_s is more secure than fopen.
    if (fopen_s(&fp, filename.c_str(), "wb")) {
      fp = nullptr;
    }
#else // ^^^ _WIN32 / !_WIN32 vvv
    fp = std::fopen(filename.c_str(), "wb");
#endif // ^^^ _WIN32 ^^^

    if (!fp) {
      std::println(
        stderr,
        SELENA_STR_COLORS_RED_LIGHT
        "[mt-dlp] File access error."
        SELENA_STR_COLORS_RESET
      );
      return 1;
    }

    std::uint32_t num_chunks{ 1 };
    std::string init_status;

    if (!info.supports_ranges_) {
      init_status = "Ranges not supported. Using single connection.";
    } else if (info.size_ < mt_dlp::downloader::kMinMultipartLimit) {
      init_status = "File size <15 MB. Parallel chunk-based download disabled.";
    } else {
      num_chunks = static_cast<std::uint32_t>((info.size_ + mt_dlp::downloader::kChunkSplitSize - 1) / mt_dlp::downloader::kChunkSplitSize);
      init_status = "Parallel chunk-based download using 4 MiB chunks.";
    }

    mt_dlp::downloader dl_client{};

    if (info.size_ > 0 && num_chunks > 1) {
      for (std::uint32_t i{}; i < num_chunks; ++i) {
        const std::int64_t start{ static_cast<std::int64_t>(i) * mt_dlp::downloader::kChunkSplitSize };
        const std::int64_t end{ static_cast<std::int64_t>((i == num_chunks - 1) ? (info.size_ - 1) : (start + mt_dlp::downloader::kChunkSplitSize - 1)) };

        dl_client.enqueue_chunk(start, end);
      }
    } else {
      dl_client.enqueue_chunk(0, (info.size_ > 0) ? (info.size_ - 1) : -1);
    }

    mt_dlp::ui_mgr ui{ filename, info.resolved_url_ };
    ui.start();
    dl_client.start(info.resolved_url_, fp);

    while (true) {
      dl_client.poll();
      const mt_dlp::download_stats_t stats{ dl_client.get_stats() };

      double progress{};

      if (info.size_ > 0) {
        progress = static_cast<double>(stats.total_bytes_) / static_cast<double>(info.size_);
      }

      const std::string speed_str{ mt_dlp::downloader::format_size(stats.curr_speed_) + "/s" };
      const std::string downloaded_str{
        mt_dlp::downloader::format_size(static_cast<double>(stats.total_bytes_)) + " / " + mt_dlp::downloader::format_size(static_cast<double>(info.size_))
      };

      std::string eta_str{ "ETA: --:--:--" };

      if (stats.curr_speed_ > 0.0 && info.size_ > 0) {
        const std::int64_t remaining_bytes{ info.size_ - stats.total_bytes_ };
        const std::int64_t eta_sec{ static_cast<std::int64_t>(static_cast<double>(remaining_bytes) / stats.curr_speed_) };
        eta_str = "ETA: " + mt_dlp::downloader::format_time(eta_sec);
      }

      std::string current_status{ init_status };

      if (stats.is_done_) {
        if (!stats.error_msg_.empty()) {
          current_status = "Error: " + stats.error_msg_;
        } else {
          const std::chrono::steady_clock::time_point end{ std::chrono::steady_clock::now() };

          const std::chrono::hh_mm_ss hms{ end - start };

          std::string time_str;
          time_str.reserve(17);

          const auto hours{ hms.hours().count() };
          const auto mins{ hms.minutes().count() };
          const auto secs{ hms.seconds().count() };
          const auto ms{ std::chrono::duration_cast<std::chrono::milliseconds>(hms.subseconds()).count() };

          if (hours) { time_str += std::format("{}h ", hours); }
          if (mins) { time_str += std::format("{}min ", mins); }

          if (secs) {
            time_str += std::format("{}", secs);
          }

          if (time_str.empty() || !secs) {
            time_str += std::format("0.{:03}s", ms);
          } else {
            time_str += std::format(".{:03}s", ms);
          }

          current_status = std::format("Download Complete! Took {}.", time_str);
          progress = 1.0;
        }
      }

      ui.update(progress, speed_str, eta_str, current_status, downloaded_str);

#ifdef MT_DLP_TEST_LOCAL_DOWNLOAD
      static bool run_once{ false };

      if (!run_once) {
        std::this_thread::sleep_for(std::chrono::seconds{ 2 });
        run_once = true;
      }
#endif // ^^^ MT_DLP_TEST_LOCAL_DOWNLOAD ^^^

      if (stats.is_done_) {
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds{ 2 });
    ui.stop();

    if (fp) {
      std::fclose(fp);
    }

    return 0;

#endif // ^^^ MT_DLP_REGEX_TEST ^^^
  } catch (const std::exception& e) {
    std::println(
      stderr,
      SELENA_STR_COLORS_RED_LIGHT
      "[mt-dlp] standard exception thrown: {}"
      SELENA_STR_COLORS_RESET,
      e.what()
    );
    
    return 1;
  } catch (...) {
    std::println(
      stderr,
      SELENA_STR_COLORS_RED_LIGHT
      "[mt-dlp] unknown exception thrown"
      SELENA_STR_COLORS_RESET
    );
    return 1;
  }
}
