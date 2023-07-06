#ifndef UI_RENDER_FONT_GDI_H_
#define UI_RENDER_FONT_GDI_H_

#pragma once

#include "duilib/Render/IFont.h"

namespace ui 
{

/** GDI字体接口的实现
*/
class UILIB_API Font_GDI: public IFont
{
public:
    Font_GDI() : 
        m_logFont{ 0, }, 
        m_hFont(nullptr) 
    {
    }

    explicit Font_GDI(const LOGFONT& logFont): 
        m_logFont(logFont)
    {
        m_hFont = ::CreateFontIndirect(&m_logFont);
        ASSERT(m_hFont != nullptr);
    }

    virtual ~Font_GDI()
    {
        if (m_hFont != nullptr) {
            ::DeleteObject(m_hFont);
            m_hFont = nullptr;
        }
    }

    Font_GDI(const Font_GDI&) = delete;
    Font_GDI& operator=(const Font_GDI&) = delete;

    /**@brief 获取字体名
     */
    virtual const wchar_t* FontName() const override { return m_logFont.lfFaceName; }

    /**@brief 获取字体大小
     */
    virtual int FontSize() const override { return -m_logFont.lfHeight; }

    /**@brief 获取字体宽度
     */
    virtual int FontWidth() const override { return m_logFont.lfWidth; }

    /**@brief 获取字体Weight值
     */
    virtual int FontWeight() const override { return m_logFont.lfWeight; }

    /**@brief 是否为粗体
     */
    virtual bool IsBold() const override { return m_logFont.lfWeight >= FW_BOLD; }

    /**@brief 字体下划线状态
     */
    virtual bool IsUnderline() const override { return m_logFont.lfUnderline; }

    /**@brief 字体的斜体状态
     */
    virtual bool IsItalic() const override { return m_logFont.lfItalic; }

    /**@brief 字体的删除线状态
     */
    virtual bool IsStrikeOut() const override { return m_logFont.lfStrikeOut; }

    /** 获取字体句柄
    */
    HFONT GetFontHandle() const { return m_hFont; }

private:
    //字体信息
    LOGFONT m_logFont;

    //字体句柄
    HFONT m_hFont;
};

} // namespace ui

#endif // UI_RENDER_FONT_GDI_H_
