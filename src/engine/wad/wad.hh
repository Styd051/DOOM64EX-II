#ifndef __WAD__60795258
#define __WAD__60795258

#include "idevice.hh"
#include "lump.hh"

namespace imp {
  namespace wad {
    void init();

    /*!
     * Add a device loader callback
     * @param device_loader
     */
    bool add_device_loader(IDeviceLoader& device_loader);

    /*!
     *
     * @param device
     */
    bool add_device(IDevicePtr device);

    bool add_device(StringView path);

    void merge();

    Optional<Lump> open(Section section, StringView path);

    /*!
     * Open a Lump by its section index
     * @param section
     * @param path
     * @return
     */
    Optional<Lump> open(Section section, size_t index);

    Optional<Lump> open(size_t index);

    /*!
     * Open a Lump by the path it has inside its device, rather than by its
     * eight-character lump name.
     *
     * Lump names are normalised and truncated, so several files can end up
     * sharing one -- "progs/common.inc" and "progs/common_glsl.inc" both become
     * "COMMON", and only the last one added survives. Anything laid out in
     * directories, shader sources in particular, has to be addressed this way.
     *
     * Matching is case-insensitive and treats '\\' as '/'. Only devices that
     * fill in ILump::real_name() can be found here; the ROM and WAD devices
     * leave it empty.
     *
     * @param path Path within the device, e.g. "progs/common_glsl.inc"
     * @return The lump, or nullopt if no device holds that path
     */
    Optional<Lump> open_path(StringView path);

    inline bool exists_path(StringView path)
    { return static_cast<bool>(open_path(path)); }

    inline Optional<Lump> open(StringView path)
    { return open(Section::normal, path); }

    inline bool exists(Section section, StringView path)
    { return static_cast<bool>(open(section, path)); }

    inline bool exists(StringView path)
    { return exists(Section::normal, path); }

    /*!
     * Every device path known to open_path, in no particular order.
     *
     * @param prefix Only return paths starting with this, case-insensitively
     * @return Matching paths, as stored by the device
     */
    Vector<String> list_paths(StringView prefix = ""_sv);

    ArrayView<ILumpPtr> list_section(Section);
  }
}

#endif //__WAD__60795258
