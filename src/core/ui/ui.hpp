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

#ifndef MT_DLP_UI_HPP
#define MT_DLP_UI_HPP

#include "include/pch.hpp"

namespace mt_dlp {

class ui_mgr {
public:
  explicit ui_mgr(
    SELENA_FUNCTION_IN const std::string& pFileName_,
    SELENA_FUNCTION_IN const std::string& pUrl_
  );
  
  NO_COPY_MOVE(ui_mgr);

  void start();
  void stop();
  void update(
    const double pProgress_,
    SELENA_FUNCTION_IN const std::string& pSpeed_,
    SELENA_FUNCTION_IN const std::string& pEta_,
    SELENA_FUNCTION_IN const std::string& pStatus_,
    SELENA_FUNCTION_IN const std::string& pDownloaded_
  );

private:
  ftxui::ScreenInteractive screen_;
  std::mutex mtx_;
  std::string filename_, url_, speed_, eta_, status_, downloaded_;
  std::jthread ui_thread_;
  std::shared_ptr<ftxui::ComponentBase> renderer_;
  double progress_{};
}; // class ui_mgr

} // namespace mt_dlp

#endif // MT_DLP_UI_HPP
