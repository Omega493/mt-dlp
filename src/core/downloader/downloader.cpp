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

#include "src/core/downloader/downloader.hpp"

#include "selena/cpp/string_utils.hpp"

namespace mt_dlp {

downloader::downloader() : multi_handle_{ multi_handle_ = curl_multi_init() }, last_time_{ std::chrono::steady_clock::now() } {}

downloader::~downloader() {
  while (!handle_pool_.empty()) {
    curl_easy_cleanup(handle_pool_.back());
    handle_pool_.pop_back();
  }

  if (multi_handle_) {
    curl_multi_cleanup(multi_handle_);
  }
}

// Helper for header_callback.
static constexpr int hex_to_int(
  const char pC_
) noexcept {
  if (pC_ >= '0' && pC_ <= '9') { return pC_ - '0'; }
  if (pC_ >= 'a' && pC_ <= 'f') { return pC_ - 'a' + 10; }
  if (pC_ >= 'A' && pC_ <= 'F') { return pC_ - 'A' + 10; }
  return 0;
}

// Helps replace elements such as "%20" to " " (space).
[[nodiscard]] std::string downloader::url_decode(
  SELENA_FUNCTION_IN const std::string_view pView_
) {
  std::string decoded_name;

  // The decoded name can go at most to the view's length.
  decoded_name.reserve(pView_.length());

  for (std::size_t i{}; i < pView_.length(); ++i) {

    // If the current character is a % and the view has at least 2 chars remaining...
    if (pView_[i] == '%' && (i + 2) < pView_.length()) {

      // ... we extract the next two characters. That is, %20 is read all at once.
      // The hex_to_int function is passed the value '2' and '0'. It returns 2 and 0 respectively.
      // Now, (2 << 4) | 0 is performed. This returns the integer 32.
      // Integer 32 corresponds to the space character in ASCII. Append.
      decoded_name += static_cast<char>((hex_to_int(pView_[i + 1]) << 4) | hex_to_int(pView_[i + 2]));
      i += 2;
    } else {
      // If not %, we directly append.
      decoded_name += pView_[i];
    }
  }

  return decoded_name;
}

[[nodiscard]] file_info_t downloader::get_file_metadata(
  SELENA_FUNCTION_IN const std::string& pUrl_
) {
  file_info_t info{
    .resolved_url_ = pUrl_
  };

  CURL* const curl{ curl_easy_init() };

  if (!curl) {
    throw std::runtime_error{ "Failed to initialize curl handle." };
  }

  const curl_opt_res_t arr[]{
    { "CURLOPT_USERAGENT", curl_easy_setopt(curl, CURLoption::CURLOPT_USERAGENT, "mt-dlp") },
    { "CURLOPT_URL", curl_easy_setopt(curl, CURLoption::CURLOPT_URL, pUrl_.c_str()) },
    { "CURLOPT_NOBODY", curl_easy_setopt(curl, CURLoption::CURLOPT_NOBODY, 1L) },
    { "CURLOPT_FOLLOWLOCATION", curl_easy_setopt(curl, CURLoption::CURLOPT_FOLLOWLOCATION, 1L) },
    { "CURLOPT_HEADERFUNCTION", curl_easy_setopt(curl, CURLoption::CURLOPT_HEADERFUNCTION, header_callback) },
    { "CURLOPT_HEADERDATA", curl_easy_setopt(curl, CURLoption::CURLOPT_HEADERDATA, &info) },
    { "CURLOPT_FILETIME", curl_easy_setopt(curl, CURLoption::CURLOPT_FILETIME, 1L) }
  };

  for (const curl_opt_res_t& res : arr) {
    if (res.code_ != CURLcode::CURLE_OK) {
      curl_easy_cleanup(curl);

      throw std::runtime_error{
        std::format(
          "Failed to set curl option {}. Curl code: {} ({})",
          res.desc_,
          static_cast<int>(res.code_),
          curl_easy_strerror(res.code_)
        )
      };
    }
  }

  if (const CURLcode res{ curl_easy_perform(curl) }; res != CURLcode::CURLE_OK) {
    curl_easy_cleanup(curl);

    throw std::runtime_error{
      std::format(
        "Failed to perform header callback. Curl code: {} ({})",
        static_cast<int>(res),
        curl_easy_strerror(res)
      )
    };
  }

  int res_code{};

  if (const CURLcode res{ curl_easy_getinfo(curl, CURLINFO::CURLINFO_RESPONSE_CODE, &res_code) }; res != CURLcode::CURLE_OK) {
    curl_easy_cleanup(curl);

    throw std::runtime_error{
      std::format(
        "Failed to perform getinfo callback. Curl code: {} ({})",
        static_cast<int>(res),
        curl_easy_strerror(res)
      )
    };
  }

  if (res_code == 429 || res_code == 503) {
    curl_easy_cleanup(curl);

    throw std::runtime_error{
      std::format("Server throttled connection. HTTP response code: {}", res_code)
    };
  }

  if (res_code >= 400) {
    curl_easy_cleanup(curl);

    throw std::runtime_error{
      std::format("HTTP response failed with code: {}", res_code)
    };
  }

  curl_off_t size_dl{};

  if ((curl_easy_getinfo(curl, CURLINFO::CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size_dl) == CURLcode::CURLE_OK) && (size_dl > 0)) {
    info.size_ = static_cast<std::int64_t>(size_dl);
  } else {
    curl_easy_cleanup(curl);

    throw std::runtime_error{ "Failed to get file size." };
  }

  char* effective_url{ nullptr };

  if ((curl_easy_getinfo(curl, CURLINFO::CURLINFO_EFFECTIVE_URL, &effective_url) == CURLcode::CURLE_OK) && effective_url) {
    info.resolved_url_ = effective_url;
  } else {
    curl_easy_cleanup(curl);

    throw std::runtime_error{ "Failed to get effective URL." };
  }

  // Fallback if the server didn't respond with a file name.
  if (info.filename_.empty()) {
    const std::size_t pos{ info.resolved_url_.find_last_of('/') };
    const std::string name{ (pos == std::string::npos) ? "download.bin" : info.resolved_url_.substr(pos + 1) };
    const std::size_t q_pos{ name.find('?') };

    // Triggers a move.
    info.filename_ = url_decode((q_pos == std::string::npos) ? name : name.substr(0, q_pos));

    std::println(
      SELENA_STR_COLORS_YELLOW_LIGHT
      "\n[mt-dlp] Server didn't respond with a filename. Defaulting to '{}'.\n"
      SELENA_STR_COLORS_RESET,
      info.filename_
    );
  }

  curl_easy_cleanup(curl);

  return info;
}

[[nodiscard]] std::string downloader::format_size(
  const double pBytes_
) {
  std::size_t s{};
  double bytes{ pBytes_ };

  while ((bytes >= 1024.0) && (s < 3)) {
    bytes /= 1024.0;
    ++s;
  }

  return std::format("{:.2f} {}", bytes, kSuffixes[s]);
}

[[nodiscard]] std::string downloader::format_time(
  const std::int64_t pSec_
) {
  if (pSec_ < 0) {
    return "--:--:--";
  }

  const std::chrono::seconds dur{ pSec_ };
  const std::chrono::hh_mm_ss<std::chrono::seconds> hms{ dur };

  const auto h{ hms.hours().count() };
  const auto m{ hms.minutes().count() };
  const auto s{ hms.seconds().count() };

  if (h > 0) {
    return std::format("{}h{}m{}s", h, m, s);
  }

  if (m > 0) {
    return std::format("{}m{}s", m, s);
  }

  return std::format("{}s", s);
}

void downloader::enqueue_chunk(
  const std::int64_t pStart_,
  const std::int64_t pEnd_
) {
  chunk_queue_.push(chunk_info_t{
    .start_offs_ = pStart_,
    .end_offs_ = pEnd_
  });
}

void downloader::start(
  SELENA_FUNCTION_IN const std::string& pUrl_,
  SELENA_FUNCTION_IN_MODIFIED std::FILE* pFp_
) {
  url_ = pUrl_;
  file_ptr_ = pFp_;
  stats_.is_done_ = false;

  const std::size_t init_workers{ std::min(kMaxConcurrentChunks, chunk_queue_.size()) };

  for (std::size_t i{}; i < init_workers; ++i) {
    handle_pool_.push_back(curl_easy_init());

    // Artificial sleeping to not spam the server.
    std::this_thread::sleep_for(std::chrono::milliseconds{ 50 });
  }

  for (std::size_t i{}; i < init_workers; ++i) {
    add_worker();
  }
}

void downloader::poll() {
  if (stats_.is_done_) {
    return;
  }

  std::int32_t running_handles{};
  curl_multi_perform(multi_handle_, &running_handles);

  std::int32_t numfds{};
  curl_multi_poll(multi_handle_, nullptr, 0, 100, &numfds);

  std::int32_t msgs_in_queue{};
  CURLMsg* msg{};

  while ((msg = curl_multi_info_read(multi_handle_, &msgs_in_queue))) {
    if (msg->msg == CURLMSG_DONE) {
      CURL* const easy_handle{ msg->easy_handle };
      chunk_ctx_t* ctx{};
      curl_easy_getinfo(easy_handle, CURLINFO::CURLINFO_PRIVATE, &ctx);

      std::int32_t res_code{};
      curl_easy_getinfo(easy_handle, CURLINFO::CURLINFO_RESPONSE_CODE, &res_code);

      if (res_code == 429 || res_code == 503) {
        stats_.error_msg_ = "Server throttled connection.";
        stats_.is_done_ = true;
      }

      flush_buffer(ctx);
      curl_multi_remove_handle(multi_handle_, easy_handle);

      // Recycle the handle into the pool instead of destroying it.
      handle_pool_.push_back(easy_handle);

      active_contexts_.erase(easy_handle);
      --active_handles_;

      if (!stats_.is_done_ && !chunk_queue_.empty()) {
        add_worker();
      }
    }
  }

  const std::chrono::steady_clock::time_point now{ std::chrono::steady_clock::now() };
  const std::int64_t dur_ms{ std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time_).count() };

  if (dur_ms >= 100) {
    const double secs{ static_cast<double>(dur_ms) / 1000.0 };
    const std::int64_t bytes_diff{ stats_.total_bytes_ - last_bytes_downloaded_ };
    const double curr_speed{ static_cast<double>(bytes_diff) / secs };

    stats_.curr_speed_ = (stats_.curr_speed_ == 0.0) ? curr_speed : (curr_speed * kSmoothingConstant + stats_.curr_speed_ * (1 - kSmoothingConstant));

    last_time_ = now;
    last_bytes_downloaded_ = stats_.total_bytes_;
  }

  if (!active_handles_ && chunk_queue_.empty()) {
    stats_.is_done_ = true;
  }
}

[[nodiscard]] download_stats_t downloader::get_stats() const {
  return stats_;
}

[[nodiscard]] std::size_t downloader::header_callback(
  SELENA_FUNCTION_IN const char* const pBuffer_,
  const std::size_t pSize_,
  const std::size_t pNitems_,
  SELENA_FUNCTION_IN_MODIFIED void* const pUserdata_
) {
  const std::size_t num_bytes{ pSize_ * pNitems_ };
  const std::string_view header_line{ pBuffer_, num_bytes };

  file_info_t* const info{ static_cast<file_info_t*>(pUserdata_) };

  if (selena::icontains(header_line, "accept-ranges: bytes")) {
    info->supports_ranges_ = true;
  }
  
  // For more details on Content-Dispotion,
  // Go: https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Content-Disposition
  if (selena::icontains(header_line, "content-disposition:")) {
    // Let header have Content-Disposition: attachment; filename*=UTF-8''file%20name.jpg.
    // it_star searches for "filename*=".
    const std::string_view::const_iterator it_star{ std::search(
      header_line.begin(), header_line.end(),
      kFnStarKey.begin(), kFnStarKey.end(),
      selena::iequal
    ) };

    if (it_star != header_line.end()) {
      // start will now point at the U of UTF-8.
      const std::size_t start{ static_cast<std::size_t>(std::distance(header_line.begin(), it_star)) + kFnStarKey.length() };

      std::size_t end{ header_line.length() };

      // Look for semicolor from the 'start' index to find end of filename.
      const std::size_t semi_pos{ header_line.find(';', start) };
      
      if (semi_pos != std::string_view::npos) {
        end = semi_pos;
      }

      // name_view is created from 'start' to 'end.
      // This view now contains UTF-8''file%20name.jpg\r\n
      std::string_view name_view{ header_line.substr(start, end - start) };
      
      // This loop removes any training spaces, line feeds or carriage returns.
      while (!name_view.empty() && (name_view.back() == '\r' || name_view.back() == '\n' || name_view.back() == ' ')) {
        name_view.remove_suffix(1);
      }

      // RFC 5987 specifies that the value begins with a charset, a single quote, an optional language tag,
      // and another single quote, followed by the actual URL-encoded filename.
      // This searches for the first single quote. It is right after the UTF-8.
      const std::size_t quote1{ name_view.find('\'') };
      
      if (quote1 != std::string_view::npos) {
        // Now search for the second single quote immediately after the first quote.
        const std::size_t quote2{ name_view.find('\'', quote1 + 1) };
      
        if (quote2 != std::string_view::npos) {
          // Now slice everything upto the second single quote.
          // The view now holds just file%20name.jpg
          name_view.remove_prefix(quote2 + 1);
        }
      }

      std::string decoded_name{ url_decode(name_view) };

      // At this stage, the decoded name shoud be "file name.jpg".
      if (!decoded_name.empty()) {
        // info->filename_ = decoded_name; makes a copy. Force a move instead.
        info->filename_ = std::move(decoded_name);
      }
    } else {
      // The else block is triggered if filename*= search fails.
      // it searches for "filename=".
      const std::string_view::const_iterator it{ std::search(
        header_line.begin(), header_line.end(),
        kFnKey.begin(), kFnKey.end(),
        selena::iequal
      ) };

      if (it != header_line.end()) {
        // start ptr now points at '"'.
        std::size_t start{ static_cast<std::size_t>(std::distance(header_line.begin(), it)) + kFnKey.length() };
        std::size_t end{ header_line.length() };

        if (start < end && header_line[start] == '"') {
          // We are extracting the file name. So, if the first character is '"',
          // we move one position forward.
          ++start;

          // This finds the position of the next '"'.
          const std::size_t quote_pos{ header_line.find('"', start) };
          
          // If found, the file name's end is marked as that position.
          if (quote_pos != std::string_view::npos) {
            end = quote_pos;
          }
        } else {
          // Look for semicolon to find the end of filename.
          const std::size_t semi_pos{ header_line.find(';', start) };
          
          // If found, make end ptr as the semi colon's position.
          if (semi_pos != std::string_view::npos) {
            end = semi_pos;
          }
        }
        // name_view is created from 'start' to 'end.
        // This view now contains file name.jpg
        std::string_view name_view{ header_line.substr(start, end - start) };

        // This loop removes any training spaces, line feeds or carriage returns.
        while (!name_view.empty() && (name_view.back() == '\r' || name_view.back() == '\n' || name_view.back() == ' ')) {
          name_view.remove_suffix(1);
        }

        if (!name_view.empty()) {
          // This one here will trigger a move.
          info->filename_ = url_decode(name_view);
        }
      }
    }
  }

  return num_bytes;
}

[[nodiscard]] std::size_t downloader::write_callback(
  SELENA_FUNCTION_IN const void* pContents_,
  const std::size_t pSize_,
  const std::size_t pNmemb_,
  SELENA_FUNCTION_IN_MODIFIED void* pUserPtr_
) {
  const std::size_t total_bytes{ pSize_ * pNmemb_ };
  chunk_ctx_t* ctx{ static_cast<chunk_ctx_t*>(pUserPtr_) };
  const char* data_ptr{ static_cast<const char*>(pContents_) };

  ctx->buf_.insert(ctx->buf_.end(), data_ptr, data_ptr + total_bytes);
  ctx->bytes_processed_ += static_cast<std::int64_t>(total_bytes);
  ctx->parent_->stats_.total_bytes_ += static_cast<std::int64_t>(total_bytes);

  if (ctx->buf_.size() >= kBufferSize) {
    if (ctx->file_ptr_) {
#ifdef _WIN32
      if (_fseeki64(ctx->file_ptr_, static_cast<long long>(ctx->start_offs_ + ctx->bytes_flushed_), SEEK_SET)) { return 0; }
#else // ^^^ _WIN32 / !_WIN32 vvv
      if (std::fseek(ctx->file_ptr_, static_cast<long>(ctx->start_offs_ + ctx->bytes_flushed_), SEEK_SET)) { return 0; }
#endif // ^^^ _WIN32 ^^^

      if (std::fwrite(ctx->buf_.data(), 1, ctx->buf_.size(), ctx->file_ptr_) != ctx->buf_.size()) {
        return 0;
      }

      ctx->bytes_flushed_ += static_cast<std::int64_t>(ctx->buf_.size());
      ctx->buf_.clear();
    }
  }

  return total_bytes;
}

void downloader::add_worker() {
  if (chunk_queue_.empty() || handle_pool_.empty()) {
    return;
  }

  const chunk_info_t chunk{ chunk_queue_.front() };
  chunk_queue_.pop();

  std::unique_ptr<chunk_ctx_t> ctx_ptr{ std::make_unique<chunk_ctx_t>() };

  chunk_ctx_t* const ctx{ ctx_ptr.get() };
  ctx->buf_.reserve(kBufferSize);
  ctx->start_offs_ = chunk.start_offs_;
  ctx->file_ptr_ = file_ptr_;
  ctx->parent_ = this;

  CURL* const curl_handle{ handle_pool_.back() };
  handle_pool_.pop_back();

  const std::string range_str{ std::format("{}-{}", chunk.start_offs_, chunk.end_offs_) };

  const curl_opt_res_t arr[]{
    { "CURLOPT_USERAGENT", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_USERAGENT, "mt-dlp") },
    { "CURLOPT_URL", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_URL, url_.c_str()) },
    { "CURLOPT_FOLLOWLOCATION", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_FOLLOWLOCATION, 1L) },
    { "CURLOPT_WRITEFUNCTION", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_WRITEFUNCTION, write_callback) },
    { "CURLOPT_WRITEDATA", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_WRITEDATA, ctx) },
    { "CURLOPT_PRIVATE", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_PRIVATE, ctx) },
    { "CURLOPT_RANGE", curl_easy_setopt(curl_handle, CURLoption::CURLOPT_RANGE, range_str.c_str()) }
  };

  for (const curl_opt_res_t& res : arr) {
    if (res.code_ != CURLcode::CURLE_OK) {
      curl_easy_cleanup(curl_handle);

      throw std::runtime_error{
        std::format("Failed to set curl option {}. Curl code: {} ({})", res.desc_, (int)(res.code_), curl_easy_strerror(res.code_))
      };
    }
  }

  curl_multi_add_handle(multi_handle_, curl_handle);
  active_contexts_[curl_handle] = std::move(ctx_ptr);
  ++active_handles_;
}

void downloader::flush_buffer(
  SELENA_FUNCTION_IN_MODIFIED chunk_ctx_t* pCtx_
) {
  if (!pCtx_->buf_.empty() && pCtx_->file_ptr_) {
#ifdef _WIN32
    if (_fseeki64(pCtx_->file_ptr_, static_cast<long long>(pCtx_->start_offs_ + pCtx_->bytes_flushed_), SEEK_SET)) { return; }
#else // ^^^ _WIN32 / !_WIN432 vvv
    if (std::fseek(pCtx_->file_ptr_, static_cast<long>(pCtx_->start_offs_ + pCtx_->bytes_flushed_), SEEK_SET)) { return; }
#endif // ^^^ !_WIN32 ^^^
    
    std::fwrite(pCtx_->buf_.data(), 1, pCtx_->buf_.size(), pCtx_->file_ptr_);
    pCtx_->bytes_flushed_ += static_cast<std::int64_t>(pCtx_->buf_.size());
    pCtx_->buf_.clear();
  }
}

} // namespace mt_dlp
