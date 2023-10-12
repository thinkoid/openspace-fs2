// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_SOFTWARE_NVGFONT_HH
#define FREESPACE2_GRAPHICS_SOFTWARE_NVGFONT_HH

#include <defs.hh>

#include "graphics/software/FSFont.hh"

namespace font {
struct font;
class NVGFont : public FSFont {
private:
    int m_handle;
    float m_letterSpacing;
    float m_size;
    float m_tabWidth;

    font* m_specialCharacters;

    float m_lineHeight = 0.0f;

public:
    NVGFont ();
    ~NVGFont () override;

    int getHandle () const { return m_handle; }
    float getSize () const { return m_size; }
    float getLetterSpacing () const { return m_letterSpacing; }
    float getTabWidth () const { return m_tabWidth; }
    font* getSpecialCharacterFont () const { return m_specialCharacters; }

    void setHandle (int handle);
    void setSize (float size);
    void setLetterSpacing (float spacing);
    void setTabWidth (float tabWidth);
    void setSpecialCharacterFont (font* fontData);

    FontType getType () const override { return NVG_FONT; };

    float getTextHeight () const override;

    void getStringSize (
        const char* text, size_t textLen, int resize_mode, float* width,
        float* height) const override;

    void computeFontMetrics () override;

    static size_t getTokenLength (const char* string, size_t maxLength);
};
} // namespace font

#endif // FREESPACE2_GRAPHICS_SOFTWARE_NVGFONT_HH
