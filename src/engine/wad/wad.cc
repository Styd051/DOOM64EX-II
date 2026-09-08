#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __linux__
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <fstream>
#include <cstring>
#include <platform/app.hh>
#include "native_ui/native_ui.hh"
#include "wad.hh"
#include "wad_loaders.hh"

extern String data_dir;

namespace {
  wad::Iwad iwad_kind_ {};

  //
  // Which of the two a file is, read from the file rather than from its name.
  //
  // The name cannot be trusted: the ROM-select dialog copies whatever the user
  // picked to "doom64.rom", so a remaster WAD chosen there arrives under the
  // cartridge's name. add_device would still load it -- the loaders match on
  // content -- and everything downstream would then be told the wrong thing.
  //
  wad::Iwad probe_iwad_(StringView path)
  {
      std::ifstream f(path.to_string(), std::ios::binary);
      char id[4] {};

      if (!f.read(id, 4))
          return wad::Iwad::none;

      if (memcmp(id, "IWAD", 4) == 0 || memcmp(id, "PWAD", 4) == 0)
          return wad::Iwad::wad;

      return wad::Iwad::rom;
  }

  bool add_iwad_(StringView path)
  {
      if (!wad::add_device(path))
          return false;

      iwad_kind_ = probe_iwad_(path);

      log::info("IWAD: {} ({})", path,
                iwad_kind_ == wad::Iwad::wad ? "2020 remaster" : "N64 cartridge");

      return true;
  }
}

wad::Iwad wad::iwad_kind()
{ return iwad_kind_; }

void wad::init()
{
    Optional<String> path;
    bool iwad_loaded {};

    // Add device loaders
    wad::add_device_loader(zip_loader);
    wad::add_device_loader(doom_loader);
    wad::add_device_loader(rom_loader);

    /* Find and add the Doom 64 IWAD */
    while (!iwad_loaded) {
        //
        // Either file will do, and the engine runs from whichever is present.
        //
        // The cartridge is tried first because it is what this engine was
        // written against and what it reproduces most faithfully -- its audio
        // is the N64's own, where the remaster's WAD carries WAV and MIDI that
        // need an instrument bank we do not ship.
        //
        if ((path = app::find_data_file("doom64.rom"))) {
            iwad_loaded = add_iwad_(*path);
        }

        if (!iwad_loaded && (path = app::find_data_file("doom64.wad"))) {
            iwad_loaded = add_iwad_(*path);
        }

#ifdef _WIN32
        if (!iwad_loaded) {
            char cd[MAX_PATH];
            GetCurrentDirectory(MAX_PATH, cd);

            auto str = g_native_ui->rom_select();

            SetCurrentDirectory(cd);

            if (!str) {
                std::exit(0);
            }

            CopyFile(str->c_str(), "doom64.rom", FALSE);
        }
#elif __linux__
        if (!iwad_loaded) {
            auto str = g_native_ui->rom_select();

            if (!str) {
                log::fatal("Couldn't find 'doom64.rom'");
            }

            auto path = fmt::format("{}/doom64.rom", data_dir);
            int srcfd = ::open(str->c_str(), O_RDONLY, 0);
            int dstfd = ::open(path.c_str(), O_WRONLY | O_CREAT, 0644);

            struct stat srcstat;
            fstat(srcfd, &srcstat);

            sendfile(dstfd, srcfd, 0, srcstat.st_size);

            close(srcfd);
            close(dstfd);
        }
#endif
    }

    // Find and add 'doom64ex.pk3'
    if (auto engine_data_path = app::find_data_file("doom64ex.pk3")) {
        wad::add_device(*engine_data_path);
    } else {
        log::fatal("Couldn't find 'doom64ex.pk3'");
    }

    wad::merge();
}
