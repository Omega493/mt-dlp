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

#ifndef CONNECTION_POOL_HPP
#define CONNECTION_POOL_HPP

#include <curl/curl.h>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "selena/base.hpp"

class ConnectionPool {
public:
  explicit ConnectionPool(size_t pool_size);
  ~ConnectionPool();

  NO_COPY_MOVE(ConnectionPool);

  CURL* acquire();
  void release(CURL* handle);

private:
  std::queue<CURL*> available_handles;
  std::mutex mtx;
  std::condition_variable cv;
};

#endif // CONNECTION_POOL_HPP
