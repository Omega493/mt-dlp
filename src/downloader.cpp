#include "downloader.hpp"

std::string Downloader::get_filename(const std::string& url) {
  const std::string name{ url.substr(url.find_last_of('/') + 1) };
  return name.empty() ? "download.bin" : name.substr(0, name.find('?'));
}
std::string Downloader::format_size(double bytes) {
  const char* const suffixes[4]{ "B", "KiB", "MiB", "GiB" };
  size_t s{ 0 };
  while (bytes >= 1024.0 && s < 3) {
    bytes /= 1024.0;
    ++s;
  }

  return std::format("{:.2f} {}", bytes, suffixes[s]);
}

std::string Downloader::format_time(int64_t sec) {
  if (sec < 0 || sec > 360000) return "--:--:--";

  const int64_t h{ sec / 3600 };
  const int64_t m{ (sec % 3600) / 60 };
  const int64_t s{ sec % 60 };

  if (h > 0) return std::format("{}h{}m{}s", h, m, s);
  else if (m > 0)
    return std::format("{}m{}s", m, s);

  return std::format("{}s", s);
}

FileInfo Downloader::file_info(const std::string& url) {
  FileInfo info{ .resolved_url = url };

  CURL* const curl{ curl_easy_init() };
  if (!curl) throw std::runtime_error("Failed to initialize curl handle");

  const std::vector<CurlOptRes> opts{
    { "CURLOPT_USERAGENT", curl_easy_setopt(curl, CURLOPT_USERAGENT, "mt-dlp") },
    { "CURLOPT_URL", curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) },
    { "CURLOPT_NOBODY", curl_easy_setopt(curl, CURLOPT_NOBODY, 1L) },
    { "CURLOPT_FOLLOWLOCATION", curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) },
    { "CURLOPT_HEADERFUNCTION", curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback) },
    { "CURLOPT_HEADERDATA", curl_easy_setopt(curl, CURLOPT_HEADERDATA, &info.supports_ranges) },
    { "CURLOPT_FILETIME", curl_easy_setopt(curl, CURLOPT_FILETIME, 1L) }
  };

  for (const CurlOptRes& opt : opts)
    if (opt.code != CURLE_OK) {
      (void)curl_easy_cleanup(curl);
      throw std::runtime_error("[curl] Error while setting curl option '" +
                               std::string{ opt.desc } + "': " + curl_easy_strerror(opt.code));
    }

  const CURLcode res{ curl_easy_perform(curl) };

  if (res != CURLE_OK) {
    (void)curl_easy_cleanup(curl);
    throw std::runtime_error("[curl] Network error: " + std::string{ curl_easy_strerror(res) });
  }

  int32_t res_code{};
  const std::pair<std::string, CURLcode> curl_res_code{
    "CURLINFO_RESPONSE_CODE", curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code)
  };
  if (curl_res_code.second != CURLE_OK) {
    (void)curl_easy_cleanup(curl);
    throw std::runtime_error("Error while fetching curl's response code: " + curl_res_code.first +
                             ": " + std::string{ curl_easy_strerror(curl_res_code.second) });
  }

  if ((res_code != 429) && (res_code != 503) && (res_code >= 400)) {
    (void)curl_easy_cleanup(curl);
    throw std::runtime_error("HTTP Error: " + std::to_string(res_code));
  }

  curl_off_t size_dl{};
  if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size_dl) == CURLE_OK)
    if (size_dl > 0) info.size = static_cast<int64_t>(size_dl);

  char* const effective_url{ nullptr };
  if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url) == CURLE_OK && effective_url)
    info.resolved_url = std::string{ effective_url };

  (void)curl_easy_cleanup(curl);
  return info;
}

int Downloader::download(const std::string& url,
                         DownloadState& state,
                         std::FILE* const fp,
                         std::mutex& fp_mtx,
                         const int64_t range_start,
                         const int64_t range_end) {
  CURL* const curl{ curl_easy_init() };
  if (!curl) throw std::runtime_error("Failed to create curl handle");

  const int64_t expected_size{ (range_end > 0 && range_start >= 0) ? (range_end - range_start + 1)
                                                                   : -1 };
  Context ctx{ &state, fp, &fp_mtx, range_start, expected_size };

  std::string range_str{};
  if (range_start >= 0) {
    range_str = std::to_string(range_start) + '-';
    if (range_end > 0) range_str += std::to_string(range_end);
  }

  const std::vector<CurlOptRes> opts{
    { "CURLOPT_USERAGENT", curl_easy_setopt(curl, CURLOPT_USERAGENT, "mt-dlp") },
    { "CURLOPT_URL", curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) },
    { "CURLOPT_FOLLOWLOCATION", curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) },
    { "CURLOPT_WRITEFUNCTION", curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) },
    { "CURLOPT_WRITEDATA", curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx) },
    { "CURLOPT_NOPROGRESS", curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L) },
    { "CURLOPT_XFERINFOFUNCTION",
      curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback) },
    { "CURLOPT_XFERINFODATA", curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx) },
    { "CURLOPT_RANGE",
      range_str.empty() ? CURLE_OK : curl_easy_setopt(curl, CURLOPT_RANGE, range_str.c_str()) }
  };

  for (const CurlOptRes& opt : opts)
    if (opt.code != CURLE_OK) {
      {
        std::scoped_lock lock{ state.mtx };
        state.err_msg = "Configuration error: " + opt.desc;
        state.is_done = true;
      }
      (void)curl_easy_cleanup(curl);
      return opt.code;
    }

  CURLcode res{ curl_easy_perform(curl) };

  int32_t res_code{};
  (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);

  if (res_code == 503 || res_code == 429) {  // 503 = service unavailable, 429 = too many requests
    {
      std::scoped_lock lock{ state.mtx };
      state.should_retry = true;
      state.is_done = true;
      state.err_msg = "Server throttled connection (HTTP " + std::to_string(res_code) + ")";
    }
    (void)curl_easy_cleanup(curl);
    return -1;
  }

  (void)curl_easy_cleanup(curl);

  // If we manually aborted because we finished, treat it as ok
  if ((res == CURLE_WRITE_ERROR) && (ctx.limit_bytes > 0) &&
      (ctx.bytes_processed >= ctx.limit_bytes))
    res = CURLE_OK;

  if (!ctx.buffer.empty() && ctx.fp) {
    std::scoped_lock lock{ fp_mtx };

#ifdef _WIN32
    if (_fseeki64(ctx.fp,
                  static_cast<__int64>(ctx.range_start_offset + ctx.bytes_flushed),
                  SEEK_SET) != 0)
#else
    if (std::fseek(
            ctx.fp, static_cast<long>(ctx.range_start_offset + ctx.bytes_flushed), SEEK_SET) != 0)
#endif
      res = CURLE_WRITE_ERROR;  // Seek failed
    else {
      if (std::fwrite(ctx.buffer.data(), 1, ctx.buffer.size(), ctx.fp) != ctx.buffer.size())
        res = CURLE_WRITE_ERROR;
      else
        ctx.bytes_flushed += ctx.buffer.size();
      ctx.buffer.clear();
    }
  }

  {
    std::scoped_lock lock{ state.mtx };
    if (!state.should_retry) {  // Only mark done if we aren't retrying
      state.is_done = true;
      if (res != CURLE_OK) state.err_msg = curl_easy_strerror(res);
      else {
        state.bytes_downloaded = ctx.bytes_processed;
        state.curr_speed = 0.0;
      }
    }
  }
  return res;
}

size_t Downloader::header_callback(const char* const buffer,
                                   const size_t size,
                                   const size_t nitems,
                                   void* const userdata) {
  const size_t num_bytes{ size * nitems };
  const std::string_view header_line{ buffer, num_bytes };
  constexpr std::string_view target{ "accept-ranges: bytes" };

  bool* const supports_ranges{ static_cast<bool*>(userdata) };
  if (selena::icontains(header_line, target)) *supports_ranges = true;
  return num_bytes;
}

int Downloader::progress_callback(void* const clientp,
                                  const curl_off_t,
                                  const curl_off_t dlnow,
                                  const curl_off_t,
                                  const curl_off_t) {
  Context* const ctx{ static_cast<Context*>(clientp) };

  const std::chrono::steady_clock::time_point now{ std::chrono::steady_clock::now() };
  const int64_t dur{
    std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx->last_time).count()
  };
  if (dur < 100 && dlnow > 0) return 0;

  const double sec{ static_cast<double>(dur) / 1000.0 };
  const double curr_speed{ [&]() -> double {
    if (sec > 0) return static_cast<double>(dlnow - ctx->last_byte_count) / sec;
    return 0.0;
  }() };

  ctx->avg_speed = (ctx->avg_speed == 0) ? curr_speed : (curr_speed * 0.3 + ctx->avg_speed * 0.7);

  ctx->last_time = now;
  ctx->last_byte_count = dlnow;

  {
    std::scoped_lock lock{ ctx->state->mtx };
    ctx->state->curr_speed = ctx->avg_speed;
  }

  return 0;
}

size_t Downloader::write_callback(const void* const contents,
                                  const size_t size,
                                  const size_t nmemb,
                                  void* const userp) {
  Context* const ctx{ static_cast<Context*>(userp) };
  size_t total_bytes{ size * nmemb };

  // We check against bytes_processed (what we have accepted into RAM/Disk so far),
  // not just what has been flushed to disk.
  if (ctx->limit_bytes > 0) {
    if (ctx->bytes_processed >= ctx->limit_bytes) return 0;  // Abort, we are done
    if ((ctx->bytes_processed + total_bytes) > ctx->limit_bytes)
      // Trim the excess so we don't write into the next chunk
      total_bytes = static_cast<size_t>(ctx->limit_bytes - ctx->bytes_processed);
  }

  const char* const data_ptr{ static_cast<const char*>(contents) };
  ctx->buffer.insert(ctx->buffer.end(), data_ptr, data_ptr + total_bytes);

  ctx->bytes_processed += total_bytes;
  {
    std::scoped_lock lock{ ctx->state->mtx };
    ctx->state->bytes_downloaded = ctx->bytes_processed;
  }

  if (ctx->buffer.size() >= Context::BUFFER_THRESHOLD) {
    if (ctx->fp) {
      std::scoped_lock lock{ *ctx->fp_mtx };

      // 'long' is 32-bit in Windows. So fseek won't be able to handle files
      // larger than 2 GiB. If we try downloading a file >2 GiB, the seek offset
      // will cause int overflow and corrupt the file.
#ifdef _WIN32
      if (_fseeki64(ctx->fp,
                    static_cast<long long>(ctx->range_start_offset + ctx->bytes_flushed),
                    SEEK_SET) != 0)
#else
      if (std::fseek(ctx->fp,
                     static_cast<long>(ctx->range_start_offset + ctx->bytes_flushed),
                     SEEK_SET) != 0)
#endif
        return 0;  // Seek error

      if (std::fwrite(ctx->buffer.data(), 1, ctx->buffer.size(), ctx->fp) != ctx->buffer.size())
        return 0;  // Write error

      ctx->bytes_flushed += ctx->buffer.size();
      ctx->buffer.clear();
    }
  }

  return total_bytes;
}
