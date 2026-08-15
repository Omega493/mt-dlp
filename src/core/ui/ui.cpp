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

#include "src/core/ui/ui.hpp"

namespace mt_dlp {

ui_mgr::ui_mgr(
  SELENA_FUNCTION_IN const std::string& pFileName_,
  SELENA_FUNCTION_IN const std::string& pUrl_
) : filename_{ pFileName_ }, url_{ pUrl_ }, screen_{ ftxui::ScreenInteractive::TerminalOutput() }
{
  renderer_ = ftxui::Renderer([this]() -> ftxui::Element {
    std::scoped_lock lock{ mtx_ };

    return ftxui::window(
      ftxui::text(" mt-dlp ") | ftxui::bold | ftxui::center,
      ftxui::vbox({
        ftxui::text("File: " + filename_) | ftxui::bold,
        ftxui::text("From: " + url_) | ftxui::bold,
        ftxui::text(status_) | (progress_ >= 1.0 ? ftxui::color(ftxui::Color::Green) : ftxui::color(ftxui::Color::Cyan)),
        ftxui::separator(),
        ftxui::hbox({
          ftxui::text("Progress: "),
          ftxui::text(downloaded_),
          (progress_ >= 1.0 ? ftxui::text(" [COMPLETE]") | ftxui::color(ftxui::Color::Green) : ftxui::text("")),
          ftxui::filler(),
          (progress_ >= 1.0 ? ftxui::text("") : ftxui::hbox({
            ftxui::text(speed_) | ftxui::color(ftxui::Color::Cyan),
            ftxui::text(" | "),
            ftxui::text(eta_) | ftxui::color(ftxui::Color::Cyan)
          }))
        }),
        ftxui::gauge(progress_) | ftxui::color(ftxui::Color::GreenLight)
      })
    );
  });
}

void ui_mgr::start() {
  ui_thread_ = std::jthread{ [this]() -> void {
    screen_.Loop(renderer_);
  } };
}

void ui_mgr::stop() {
  screen_.Exit();
}

void ui_mgr::update(
  const double pProgress_,
  SELENA_FUNCTION_IN const std::string& pSpeed_,
  SELENA_FUNCTION_IN const std::string& pEta_,
  SELENA_FUNCTION_IN const std::string& pStatus_,
  SELENA_FUNCTION_IN const std::string& pDownloaded_
) {
  {
    std::scoped_lock lock{ mtx_ };
    progress_ = pProgress_;
    speed_ = pSpeed_;
    eta_ = pEta_;
    status_ = pStatus_;
    downloaded_ = pDownloaded_;
  }
  
  screen_.Post(ftxui::Event::Custom);
}

} // namespace mt_dlp
