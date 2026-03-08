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

// TODO: Refactor this absolute monolith of a file

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <memory>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "selena/base.hpp"
#include "selena/utils.hpp"
#include "src/downloader.hpp"
#include "src/connection_pool.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: mt-dlp <url>" << std::endl;
    return 1;
  }

  const std::string url{ argv[1] };
  if (!selena::is_valid_url(url)) {
    std::cerr << "Given URL does not seem to be a valid URL." << std::endl;
    return 1;
  }

  constexpr int64_t MIN_MULTIPART_LIM{ 50 * 1024 * 1024 }; // 50 MB

  uint32_t curr_threads{ 0 };

  while (true) {
    FileInfo file_info{};
    try { file_info = Downloader::file_info(url); }
    catch (const std::exception& e) {
      std::cerr << "Initialization Error: " << e.what() << std::endl;
      return 1;
    }

    const std::string filename{ Downloader::get_filename(file_info.resolved_url) };
    std::FILE* const fp{ std::fopen(filename.c_str(), "wb") };
    if (!fp) { std::perror("File access error"); return 1; }

    if (!curr_threads)
      curr_threads = [&file_info]() -> uint32_t {
        if (!file_info.supports_ranges) return 1;
        if (file_info.size < MIN_MULTIPART_LIM) return 1;
        return (std::thread::hardware_concurrency() > 8) ? 8 : 6;
      }();

    std::vector<std::jthread> workers{};

    std::string init_status{};
    if (!file_info.supports_ranges) init_status = "Ranges not supported. Using single connection.";
    else if (file_info.size < MIN_MULTIPART_LIM) init_status = "File size <50 MB. Optimization disabled.";
    else init_status = "Parallel Download: " + std::to_string(curr_threads) + " threads.";

    GlobalStats ui_stats{
    .filename = filename,
    .status_text = init_status
    };
    ui_stats.workers.resize(curr_threads);
    std::mutex ui_mtx{};

    ftxui::ScreenInteractive screen{ ftxui::ScreenInteractive::TerminalOutput() };

    // Normal 'bool' may also be used. Let me just use atomic though because why not.
    std::atomic_bool all_done{ true };

    std::shared_ptr<ftxui::ComponentBase> renderer{ ftxui::Renderer([&] {
      std::scoped_lock lock{ ui_mtx };

      std::vector<ftxui::Element> worker_elements{};
      for (const WorkerStats& w : ui_stats.workers) {
        /*
        * For individual threads, we define a style of progress bars:
        * 1. If the download is single-threaded, it gets a solid block of a progress bar
        * 2. If multi-threaded download:
        *  a. Even threads get blocks
        *  b. Odd ones get dimmed out
        */
        const ftxui::Decorator gauge_style{ ((ui_stats.workers.size() == 1) || !(w.id & 1)) ?
          (ftxui::color(ftxui::Color::White)) : (ftxui::color(ftxui::Color::Grey100) | ftxui::dim)};
  
        const ftxui::Element status_elem{ [&w]() -> ftxui::Element {
          if (w.is_done) return ftxui::text(" [DONE]") | ftxui::color(ftxui::Color::Green);
          else return ftxui::text(" " + w.speed) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15);
        }() };

        worker_elements.push_back(
          ftxui::hbox({
            ftxui::text(" T-" + std::to_string(w.id)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 6) | ftxui::color(ftxui::Color::Gold1),
            ftxui::text(" "),
            ftxui::gauge(w.progress) | ftxui::flex | gauge_style,
            status_elem
          })
        );
      }

      return ftxui::window(
        ftxui::text(" mt-dlp ") | ftxui::bold | ftxui::center,
        ftxui::vbox({
          ftxui::text("File: " + ui_stats.filename) | ftxui::bold,
          ftxui::text(ui_stats.status_text) |
            (all_done.load() ? ftxui::color(ftxui::Color::Green) : ftxui::color(ftxui::Color::Cyan)),
          ftxui::separator(),
          ftxui::vbox({
            ftxui::hbox({
              ftxui::text("Total Progress: "),
              ftxui::text(ui_stats.downloaded_str),
              all_done.load() ? ftxui::text(" [COMPLETE]") | ftxui::color(ftxui::Color::Green) : ftxui::text(""),
              ftxui::filler(),
              all_done.load() ? ftxui::text("") : ftxui::hbox({
                ftxui::text(ui_stats.total_speed) | ftxui::color(ftxui::Color::Cyan),
                ftxui::text(" | "),
                ftxui::text(ui_stats.eta) | ftxui::color(ftxui::Color::Cyan)
              })
            }),
            ftxui::gauge(ui_stats.total_progress) | ftxui::color(ftxui::Color::GreenLight)
          }) | ftxui::border,
        ftxui::text("Workers:") | ftxui::bold,
        ftxui::vbox(std::move(worker_elements)) | ftxui::borderEmpty
        })
      );
    }) }; // 'renderer' lambda

    const std::chrono::steady_clock::time_point start_time{ std::chrono::steady_clock::now() };  // Download start time
    
    const int64_t chunk_size{ file_info.size / curr_threads };
    std::vector<std::unique_ptr<DownloadState>> worker_states{};
    std::mutex file_mtx{};
    
    // Calculate number of connections to use (approximately half the number of threads)
    const size_t num_connections{ (curr_threads + 1) / 2 };
    ConnectionPool connection_pool{ num_connections };

    for (uint32_t i{ 0 }; i < curr_threads; ++i) {
      worker_states.push_back(std::make_unique<DownloadState>());
      const int64_t start{ i * chunk_size };
      const int64_t end{ (i == curr_threads - 1) ? file_info.size - 1 : (start + chunk_size - 1) };
      workers.emplace_back([&, i, start, end] {
        Downloader client{};
        client.download(file_info.resolved_url, *worker_states[i], fp, file_mtx, &connection_pool, start, end);
      });
      std::this_thread::sleep_for(std::chrono::milliseconds{ 50 }); // Small delay to not flood the source server
    }

    bool retry_requested{ false };

    std::jthread watcher{ [&] {
      double bps{}; // For smoothing purposes

      while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 100 }); // 10 FPS

        all_done.store(true); // Reset 'all_done' to true at the start of every check cycle

        int64_t total_bytes{};
        double total_bps{};

        GlobalStats temp_stats{
          .filename = filename,
          .status_text = ui_stats.status_text
        };

        for (uint32_t i{ 0 }; i < curr_threads; ++i) {
          std::scoped_lock lock{ worker_states[i]->mtx };

          if (worker_states[i]->should_retry) {
            retry_requested = true;
            temp_stats.status_text = "Server Error (503/429). Throttling detected.";
            {
              std::scoped_lock ui_lock{ ui_mtx };
              ui_stats = std::move(temp_stats);
            }
            screen.Post(ftxui::Event::Custom);
            screen.Exit();
            return;
          }

          WorkerStats w_stat{};
          w_stat.id = i + 1;
          w_stat.is_done = worker_states[i]->is_done;
          w_stat.speed = Downloader::format_size(worker_states[i]->curr_speed) + "/s";

          const int64_t w_chunk_size{ (i == curr_threads - 1) ? (file_info.size - (i * chunk_size)) : chunk_size };
          w_stat.progress = (chunk_size > 0) ? static_cast<double>(worker_states[i]->bytes_downloaded) / w_chunk_size : 0.0;

          temp_stats.workers.push_back(w_stat);
          total_bytes += worker_states[i]->bytes_downloaded;
          if (!worker_states[i]->is_done) {
            total_bps += worker_states[i]->curr_speed;
            all_done.store(false);
          }
        }

        temp_stats.total_progress = (file_info.size > 0) ? static_cast<double>(total_bytes) / file_info.size : 0.0;
        bps = (bps == 0.0) ? total_bps : (total_bps * 0.3 + bps * 0.7);
        temp_stats.total_speed = Downloader::format_size(total_bps) + "/s";
        temp_stats.downloaded_str = Downloader::format_size(static_cast<double>(total_bytes))
          + " / " + Downloader::format_size(static_cast<double>(file_info.size));

        const int64_t remaining{ file_info.size - total_bytes };
        const int64_t eta_sec{ (total_bps > 0) ? static_cast<long long>(remaining / bps) : 0 };
        temp_stats.eta = "ETA: " + Downloader::format_time(eta_sec);

        // We might have a small rounding error at the end.
        // To make the UI look clean, we must snap the UI to fill the blocks and all.
        // This shouldn't cause any issues to the file being downloaded though - after all,
        // if 'all_done' is true, the file download succeeded.
        // Aka, this one here is just a cosmetic / UX fix.
        if (all_done.load()) {
          const std::chrono::steady_clock::time_point end_time{ std::chrono::steady_clock::now() };
          const int64_t dur{ std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() };
          temp_stats.status_text = "Download Complete! Took " + Downloader::format_time(dur);
          temp_stats.downloaded_str = Downloader::format_size(static_cast<double>(file_info.size))
            + " / " + Downloader::format_size(static_cast<double>(file_info.size));
          temp_stats.total_progress = 1.0;
        }

        {
          std::scoped_lock lock{ ui_mtx };
          ui_stats = std::move(temp_stats);
        }

        screen.Post(ftxui::Event::Custom);
        if (all_done.load()) break;
      }

      std::this_thread::sleep_for(std::chrono::seconds{ 3 });
      screen.Exit();
    } }; // 'watcher' thread

    screen.Loop(renderer);
    if (fp) std::fclose(fp);
    workers.clear();

    if (retry_requested) {
      if (curr_threads <= 1) {
        std::cerr << "\n[Error] Download failed: Server is rejecting connections even with 1 thread." << std::endl;
        return 1;
      }

      curr_threads /= 2;
      if (curr_threads < 1) curr_threads = 1;

      std::cout << "\n[Info] Rate limit detected. Retrying with " << curr_threads << " threads in 3 seconds." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds{ 3 });
      continue;
    }

    break;
  }

  return 0;
}
