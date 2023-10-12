// -*- mode: c++; -*-

#include <defs.hh>
#include "graphics/software/FontManager.hh"
#include "graphics/software/VFNTFont.hh"
#include "graphics/software/NVGFont.hh"
#include "graphics/software/font.hh"
#include "graphics/software/font_internal.hh"
#include "graphics/paths/PathRenderer.hh"
#include <bmpman/bmpman.hh>
#include <cfile/cfile.hh>
#include <assert/assert.hh>
#include <log/log.hh>

namespace font {

std::map< std::string, TrueTypeFontData > FontManager::allocatedData;
std::map< std::string, std::unique_ptr< font > > FontManager::vfntFontData;
std::vector< std::unique_ptr< FSFont > > FontManager::fonts;

FSFont* FontManager::currentFont = NULL;

FSFont* FontManager::getFont (const std::string& name) {
    for (std::vector< std::unique_ptr< FSFont > >::iterator iter =
             fonts.begin ();
         iter != fonts.end (); iter++) {
        if ((*iter)->getName () == name) return iter->get ();
    }

    return NULL;
}

FSFont* FontManager::getCurrentFont () { return currentFont; }

int FontManager::getCurrentFontIndex () {
    if (!FontManager::isReady ()) return -1;

    return FontManager::getFontIndex (currentFont);
}

int FontManager::getFontIndex (const std::string& name) {
    int index = 0;

    for (std::vector< std::unique_ptr< FSFont > >::iterator iter =
             fonts.begin ();
         iter != fonts.end (); iter++, index++) {
        if ((*iter)->getName () == name) return index;
    }

    return -1;
}

int FontManager::getFontIndex (FSFont* font) {
    if (font == NULL) return -1;

    int index = 0;

    for (std::vector< std::unique_ptr< FSFont > >::iterator iter =
             fonts.begin ();
         iter != fonts.end (); iter++, index++) {
        if (iter->get () == font) return index;
    }

    return -1;
}

int FontManager::numberOfFonts () { return (int)fonts.size (); }

bool FontManager::isReady () { return currentFont != NULL; }

bool FontManager::isFontNumberValid (int id) {
    return id >= 0 && id < (int)fonts.size ();
}

void FontManager::setCurrentFont (FSFont* font) {
    ASSERTX (font != NULL, "New font pointer may not be NULL!");
    currentFont = font;
}

font* FontManager::loadFontOld (const std::string& typeface) {
    if (vfntFontData.find (typeface) != vfntFontData.end ()) {
        font* data = vfntFontData[typeface].get ();

        ASSERT (data != NULL);

        return data;
    }

    bool localize = true;

    CFILE* fp =
        cfopen (typeface.c_str (), "rb", CFILE_NORMAL, CF_TYPE_ANY, localize);
    if (fp == NULL) {
        WARNINGF (LOCATION, "Unable to find font file \"%s\".", typeface.c_str ());
        return NULL;
    }

    std::unique_ptr< font > fnt (new font ());
    if (!fnt) {
        WARNINGF (LOCATION, "Unable to allocate memory for \"%s\"", typeface.c_str ());
        return NULL;
    }

    strcpy (fnt->filename, typeface.c_str ());
    cfread (&fnt->id, 4, 1, fp);
    cfread (&fnt->version, sizeof (int), 1, fp);
    cfread (&fnt->num_chars, sizeof (int), 1, fp);
    cfread (&fnt->first_ascii, sizeof (int), 1, fp);
    cfread (&fnt->w, sizeof (int), 1, fp);
    cfread (&fnt->h, sizeof (int), 1, fp);
    cfread (&fnt->num_kern_pairs, sizeof (int), 1, fp);
    cfread (&fnt->kern_data_size, sizeof (int), 1, fp);
    cfread (&fnt->char_data_size, sizeof (int), 1, fp);
    cfread (&fnt->pixel_data_size, sizeof (int), 1, fp);

    if (fnt->kern_data_size) {
        fnt->kern_data = (font_kernpair*)malloc (fnt->kern_data_size);
        ASSERT (fnt->kern_data != NULL);
        cfread (fnt->kern_data, fnt->kern_data_size, 1, fp);
    }
    else {
        fnt->kern_data = NULL;
    }
    if (fnt->char_data_size) {
        fnt->char_data = (font_char*)malloc (fnt->char_data_size);
        ASSERT (fnt->char_data != NULL);
        cfread (fnt->char_data, fnt->char_data_size, 1, fp);
    }
    else {
        fnt->char_data = NULL;
    }
    if (fnt->pixel_data_size) {
        fnt->pixel_data = (ubyte*)malloc (fnt->pixel_data_size);
        ASSERT (fnt->pixel_data != NULL);
        cfread (fnt->pixel_data, fnt->pixel_data_size, 1, fp);
    }
    else {
        fnt->pixel_data = NULL;
    }
    cfclose (fp);

    // Create a bitmap for hardware cards.
    // JAS:  Try to squeeze this into the smallest square power of two texture.
    // This should probably be done at font generation time, not here.
    int w, h;
    if (fnt->pixel_data_size * 4 < 64 * 64) { w = h = 64; }
    else if (fnt->pixel_data_size * 4 < 128 * 128) {
        w = h = 128;
    }
    else if (fnt->pixel_data_size * 4 < 256 * 256) {
        w = h = 256;
    }
    else if (fnt->pixel_data_size * 4 < 512 * 512) {
        w = h = 512;
    }
    else {
        w = h = 1024;
    }

    fnt->bm_w = w;
    fnt->bm_h = h;
    fnt->bm_data = (ubyte*)malloc (fnt->bm_w * fnt->bm_h);
    fnt->bm_u = (int*)malloc (sizeof (int) * fnt->num_chars);
    fnt->bm_v = (int*)malloc (sizeof (int) * fnt->num_chars);

    memset (fnt->bm_data, 0, fnt->bm_w * fnt->bm_h);

    int i, x, y;
    x = y = 0;
    for (i = 0; i < fnt->num_chars; i++) {
        ubyte* ubp;
        int x1, y1;
        ubp = &fnt->pixel_data[fnt->char_data[i].offset];
        if (x + fnt->char_data[i].byte_width >= fnt->bm_w) {
            x = 0;
            y += fnt->h + 2;
            if (y + fnt->h > fnt->bm_h) {
                ASSERTX (0, "Font too big!\n");
            }
        }
        fnt->bm_u[i] = x;
        fnt->bm_v[i] = y;

        for (y1 = 0; y1 < fnt->h; y1++) {
            for (x1 = 0; x1 < fnt->char_data[i].byte_width; x1++) {
                uint c = *ubp++;
                if (c > 14) c = 14;
                // The font pixels only have ~4 bits of information in them
                // (since the value is at maximum 14) but the bmpman code
                // expects 8 bits of pixel information. To fix that we simply
                // rescale this value to fit into the [0, 255] range (15 * 17
                // is 255). This was adapted from the previous version where
                // the graphics code used an internal array for converting
                // these values
                fnt->bm_data[(x + x1) + (y + y1) * fnt->bm_w] =
                    (unsigned char)(c * 17);
            }
        }
        x += fnt->char_data[i].byte_width + 2;
    }

    fnt->bitmap_id =
        bm_create (8, fnt->bm_w, fnt->bm_h, fnt->bm_data, BMP_AABITMAP);

    auto ptr = fnt.get ();

    vfntFontData[typeface] = std::move (fnt);

    return ptr;
}

VFNTFont* FontManager::loadVFNTFont (const std::string& name) {
    font* font = FontManager::loadFontOld (name);

    if (font == NULL) { return NULL; }
    else {
        std::unique_ptr< VFNTFont > vfnt (new VFNTFont (font));

        auto ptr = vfnt.get ();

        fonts.push_back (std::move (vfnt));

        return ptr;
    }
}

NVGFont*
FontManager::loadNVGFont (const std::string& fileName, float fontSize) {
    if (allocatedData.find (fileName) == allocatedData.end ()) {
        CFILE* fontFile = cfopen (
            const_cast< char* > (fileName.c_str ()), "rb", CFILE_NORMAL,
            CF_TYPE_ANY);

        if (fontFile == NULL) {
            WARNINGF (LOCATION, "Couldn't open font file \"%s\"", fileName.c_str ());
            return NULL;
        }

        size_t size = static_cast< size_t > (cfilelength (fontFile));

        std::unique_ptr< ubyte[] > fontData (new ubyte[size]);

        if (!fontData) {
            WARNINGF (LOCATION,"Couldn't allocate %zu bytes for reading font file \"%s\"!",size, fileName.c_str ());
            cfclose (fontFile);
            return NULL;
        }

        if (!cfread (fontData.get (), (int)size, 1, fontFile)) {
            ERRORF (LOCATION, "Error while reading font data from \"%s\"", fileName.c_str ());
            cfclose (fontFile);
            return NULL;
        }

        cfclose (fontFile);

        TrueTypeFontData newData;

        newData.size = size;
        std::swap (newData.data, fontData);

        allocatedData.insert (std::make_pair (fileName, std::move (newData)));
    }

    auto data = &allocatedData.find (fileName)->second;

    auto path = graphics::paths::PathRenderer::instance ();

    int handle = path->createFontMem (
        fileName.c_str (), data->data.get (), (int)data->size, 0);

    if (handle < 0) {
        WARNINGF (LOCATION, "Couldn't couldn't create font for file \"%s\"", fileName.c_str ());
        return NULL;
    }

    std::unique_ptr< NVGFont > nvgFont (new NVGFont ());
    nvgFont->setHandle (handle);
    nvgFont->setSize (fontSize);

    auto ptr = nvgFont.get ();

    fonts.push_back (std::move (nvgFont));

    return ptr;
}

void FontManager::init () {}

void FontManager::close () {
    allocatedData.clear ();
    vfntFontData.clear ();
    fonts.clear ();

    currentFont = NULL;
}
} // namespace font
