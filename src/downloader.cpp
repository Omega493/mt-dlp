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
