#include "platform/plat_fs.h"

#include <cstring>
#include <cstdio>  // rename()
#include <sys/stat.h>
#include <unistd.h>

#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "plat_fs";

LittleFsWrapper LittleFS;

// ---------------------------------------------------------------- FsFile ---

FsFile::~FsFile() { close(); }

FsFile::FsFile(FsFile &&o) noexcept : f_(o.f_), d_(o.d_), path_(o.path_) {
  o.f_ = nullptr;
  o.d_ = nullptr;
}

FsFile &FsFile::operator=(FsFile &&o) noexcept {
  if (this != &o) {
    close();
    f_ = o.f_;
    d_ = o.d_;
    path_ = o.path_;
    o.f_ = nullptr;
    o.d_ = nullptr;
  }
  return *this;
}

void FsFile::close() {
  if (f_ != nullptr) {
    fclose(f_);
    f_ = nullptr;
  }
  if (d_ != nullptr) {
    closedir(d_);
    d_ = nullptr;
  }
}

size_t FsFile::write(uint8_t b) { return write(&b, 1); }

size_t FsFile::write(const uint8_t *buf, size_t len) {
  if (f_ == nullptr || buf == nullptr) {
    return 0;
  }
  return fwrite(buf, 1, len, f_);
}

int FsFile::read() {
  if (f_ == nullptr) {
    return -1;
  }
  return fgetc(f_); // devuelve EOF (-1) al final, igual que Arduino
}

size_t FsFile::read(uint8_t *buf, size_t len) {
  if (f_ == nullptr || buf == nullptr) {
    return 0;
  }
  return fread(buf, 1, len, f_);
}

size_t FsFile::readBytes(char *buf, size_t len) {
  return read(reinterpret_cast<uint8_t *>(buf), len);
}

String FsFile::readStringUntil(char terminator) {
  // Arduino devuelve lo leido SIN el terminador, y cadena vacia al final del
  // fichero. Se replica.
  String out;
  if (f_ == nullptr) {
    return out;
  }
  int c;
  std::string acc;
  while ((c = fgetc(f_)) != EOF) {
    if (static_cast<char>(c) == terminator) {
      break;
    }
    acc.push_back(static_cast<char>(c));
  }
  return String(acc);
}

size_t FsFile::print(const char *s) {
  if (f_ == nullptr || s == nullptr) {
    return 0;
  }
  const size_t n = strlen(s);
  return fwrite(s, 1, n, f_);
}

size_t FsFile::print(const String &s) { return print(s.c_str()); }
size_t FsFile::print(int v) { return print(String(v)); }
size_t FsFile::print(unsigned int v) { return print(String(v)); }
size_t FsFile::print(long v) { return print(String(v)); }
size_t FsFile::print(unsigned long v) { return print(String(v)); }
size_t FsFile::print(double v, int decimals) {
  return print(String(v, static_cast<unsigned int>(decimals)));
}

size_t FsFile::println(const char *s) { return print(s) + print("\r\n"); }
size_t FsFile::println(const String &s) { return println(s.c_str()); }
size_t FsFile::println() { return print("\r\n"); }

int FsFile::printf(const char *fmt, ...) {
  if (f_ == nullptr) {
    return 0;
  }
  va_list args;
  va_start(args, fmt);
  const int n = vfprintf(f_, fmt, args);
  va_end(args);
  return n;
}

bool FsFile::seek(uint32_t pos) {
  return f_ != nullptr && fseek(f_, static_cast<long>(pos), SEEK_SET) == 0;
}

uint32_t FsFile::position() {
  if (f_ == nullptr) {
    return 0;
  }
  const long p = ftell(f_);
  return p < 0 ? 0 : static_cast<uint32_t>(p);
}

uint32_t FsFile::size() {
  if (f_ == nullptr) {
    return 0;
  }
  const long cur = ftell(f_);
  fseek(f_, 0, SEEK_END);
  const long end = ftell(f_);
  fseek(f_, cur, SEEK_SET);
  return end < 0 ? 0 : static_cast<uint32_t>(end);
}

int FsFile::available() {
  if (f_ == nullptr) {
    return 0;
  }
  const long cur = ftell(f_);
  fseek(f_, 0, SEEK_END);
  const long end = ftell(f_);
  fseek(f_, cur, SEEK_SET);
  const long left = end - cur;
  return left < 0 ? 0 : static_cast<int>(left);
}

void FsFile::flush() {
  if (f_ != nullptr) {
    fflush(f_);
  }
}

FsFile FsFile::openNextFile() {
  if (d_ == nullptr) {
    return FsFile();
  }
  // Arduino se salta "." y ".."; readdir() del VFS tambien los omite en
  // LittleFS, pero se filtran igualmente por si acaso.
  struct dirent *e = nullptr;
  while ((e = readdir(d_)) != nullptr) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
      continue;
    }
    const String child = path_ + "/" + String(e->d_name);
    FILE *f = fopen(child.c_str(), "rb");
    if (f == nullptr) {
      continue;
    }
    return FsFile(f, child);
  }
  return FsFile();
}

// ------------------------------------------------------- LittleFsWrapper ---

String LittleFsWrapper::fullPath(const char *path) const {
  if (path == nullptr) {
    return base_;
  }
  if (path[0] == '/') {
    return base_ + path;
  }
  return base_ + "/" + path;
}

bool LittleFsWrapper::begin(bool formatOnFail, const char *basePath,
                            uint8_t maxOpenFiles, const char *partitionLabel) {
  if (mounted_) {
    return true;
  }
  base_ = basePath;
  label_ = partitionLabel;

  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = basePath;
  conf.partition_label = partitionLabel;
  conf.format_if_mount_failed = formatOnFail;
  conf.dont_mount = false;

  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "no se pudo montar LittleFS en '%s' (particion '%s'): %s",
             basePath, partitionLabel, esp_err_to_name(err));
    return false;
  }
  (void)maxOpenFiles; // el VFS de IDF no lo limita por sistema de ficheros
  mounted_ = true;
  return true;
}

void LittleFsWrapper::end() {
  if (mounted_) {
    esp_vfs_littlefs_unregister(label_);
    mounted_ = false;
  }
}

FsFile LittleFsWrapper::open(const char *path, const char *mode, bool create) {
  const String full = fullPath(path);

  // Un directorio se abre con opendir(); Arduino distinguia solo por lo que
  // hubiera en la ruta, asi que se mira primero si es un directorio.
  struct stat st;
  if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    DIR *d = opendir(full.c_str());
    return d != nullptr ? FsFile(d, full) : FsFile();
  }

  // `create` de Arduino significa "crea los directorios intermedios".
  if (create) {
    std::string p = full.str();
    for (size_t i = 1; i < p.size(); i++) {
      if (p[i] == '/') {
        p[i] = '\0';
        ::mkdir(p.c_str(), 0777);
        p[i] = '/';
      }
    }
  }

  FILE *f = fopen(full.c_str(), mode);
  return f != nullptr ? FsFile(f, full) : FsFile();
}

FsFile LittleFsWrapper::open(const String &path, const char *mode,
                             bool create) {
  return open(path.c_str(), mode, create);
}

bool LittleFsWrapper::exists(const char *path) {
  struct stat st;
  return stat(fullPath(path).c_str(), &st) == 0;
}
bool LittleFsWrapper::exists(const String &path) { return exists(path.c_str()); }

bool LittleFsWrapper::remove(const char *path) {
  return unlink(fullPath(path).c_str()) == 0;
}
bool LittleFsWrapper::remove(const String &path) { return remove(path.c_str()); }

bool LittleFsWrapper::rename(const char *from, const char *to) {
  return ::rename(fullPath(from).c_str(), fullPath(to).c_str()) == 0;
}

bool LittleFsWrapper::rename(const String &from, const String &to) {
  return rename(from.c_str(), to.c_str());
}

bool LittleFsWrapper::mkdir(const char *path) {
  return ::mkdir(fullPath(path).c_str(), 0777) == 0;
}

bool LittleFsWrapper::rmdir(const char *path) {
  return ::rmdir(fullPath(path).c_str()) == 0;
}

size_t LittleFsWrapper::totalBytes() {
  size_t total = 0, used = 0;
  if (esp_littlefs_info(label_, &total, &used) != ESP_OK) {
    return 0;
  }
  return total;
}

size_t LittleFsWrapper::usedBytes() {
  size_t total = 0, used = 0;
  if (esp_littlefs_info(label_, &total, &used) != ESP_OK) {
    return 0;
  }
  return used;
}
