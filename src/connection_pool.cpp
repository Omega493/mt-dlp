-/*
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

#include "connection_pool.hpp"

ConnectionPool::ConnectionPool(size_t pool_size) {
  for (size_t i = 0; i < pool_size; ++i) {
    CURL* handle = curl_easy_init();
    if (handle) {
      available_handles.push(handle);
    }
  }
}

ConnectionPool::~ConnectionPool() {
  std::scoped_lock lock(mtx);
  while (!available_handles.empty()) {
    CURL* handle = available_handles.front();
    available_handles.pop();
    curl_easy_cleanup(handle);
  }
}

CURL* ConnectionPool::acquire() {
  std::unique_lock lock(mtx);
  cv.wait(lock, [this]() { return !available_handles.empty(); });
  
  CURL* handle = available_handles.front();
  available_handles.pop();
  return handle;
}

void ConnectionPool::release(CURL* handle) {
  {
    std::scoped_lock lock(mtx);
    available_handles.push(handle);
  }
  cv.notify_one();
}
