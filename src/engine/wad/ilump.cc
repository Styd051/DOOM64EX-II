#include "image/image.hh"
#include "utility/convert.hh"

#include "ilump.hh"

using namespace imp::wad;

String ILump::read_bytes()
{
    auto is = stream();
    is->seekg(0, is->end);
    auto end_pos = is->tellg();
    is->seekg(0, is->beg);
    auto size = to_size(end_pos - is->tellg());

    String bytes(size, '\0');
    is->read(&bytes[0], size);

    return bytes;
}

char* ILump::read_bytes_ccompat(size_t *size_out = nullptr)
{
    auto is = stream();
    is->seekg(0, is->end);
    auto end_pos = is->tellg();
    is->seekg(0, is->beg);
    auto size = to_size(end_pos - is->tellg());

    auto bytes = new char[size];
    is->read(&bytes[0], size);

    if (size_out)
        *size_out = size;

    return bytes;
}

Optional<Image> ILump::read_image()
{
    auto is = stream();
    // Qualified: ADL on std::istream also finds std::make_optional under C++17.
    return imp::make_optional<Image>(*is);
}

Optional<Palette> ILump::read_palette()
{
    auto is = stream();
    RgbaPalette pal(256);
    for (auto& c : pal) {
        c.red = is->get();
        c.green = is->get();
        c.blue = is->get();
        c.alpha = 255;
    }
    pal[0].alpha = 0;
    return make_optional<Palette>(pal);
}
