#include "RichText.h"
#include "duilib/Animation/AnimationManager.h"
#include "duilib/Animation/AnimationPlayer.h"
#include "duilib/Core/WindowBuilder.h"
#include "duilib/Core/GlobalManager.h"
#include "duilib/Utils/StringUtil.h"
#include "duilib/Utils/AttributeUtil.h"
#include "duilib/Core/Window.h"

namespace ui 
{

RichText::RichText(Window* pWindow) :
    Control(pWindow),
    m_uTextStyle(TEXT_LEFT | TEXT_TOP),
    m_fRowSpacingMul(1.0f),
    m_bLinkUnderlineFont(true),
    m_nTextDataDPI(0)
{
}

RichText::~RichText()
{
}

std::wstring RichText::GetType() const { return DUI_CTR_RICHTEXT; }

void RichText::SetAttribute(const std::wstring& strName, const std::wstring& strValue)
{
    if (strName == L"text_align") {
        if (strValue.find(L"left") != std::wstring::npos) {
            m_uTextStyle &= ~(TEXT_CENTER | TEXT_RIGHT);
            m_uTextStyle |= TEXT_LEFT;
        }
        if (strValue.find(L"hcenter") != std::wstring::npos) {
            m_uTextStyle &= ~(TEXT_LEFT | TEXT_RIGHT);
            m_uTextStyle |= TEXT_CENTER;
        }
        if (strValue.find(L"right") != std::wstring::npos) {
            m_uTextStyle &= ~(TEXT_LEFT | TEXT_CENTER);
            m_uTextStyle |= TEXT_RIGHT;
        }
        if (strValue.find(L"top") != std::wstring::npos) {
            m_uTextStyle &= ~(TEXT_BOTTOM | TEXT_VCENTER);
            m_uTextStyle |= TEXT_TOP;
        }
        if (strValue.find(L"vcenter") != std::wstring::npos) {
            m_uTextStyle &= ~(TEXT_TOP | TEXT_BOTTOM);
            m_uTextStyle |= TEXT_VCENTER;
        }
        if (strValue.find(L"bottom") != std::wstring::npos) {
            m_uTextStyle &= ~(TEXT_TOP | TEXT_VCENTER);
            m_uTextStyle |= TEXT_BOTTOM;
        }
        m_textData.clear();
    }    
    else if (strName == L"font") {
        SetFontId(strValue);
    }
    else if (strName == L"text_color") {
        SetTextColor(strValue);
    }    
    else if ((strName == L"text_padding") || (strName == L"textpadding")) {
        UiPadding rcTextPadding;
        AttributeUtil::ParsePaddingValue(strValue.c_str(), rcTextPadding);
        SetTextPadding(rcTextPadding);
    }
    else if (strName == L"row_spacing_mul") {
        SetRowSpacingMul(wcstof(strValue.c_str(), nullptr));
    }
    else if (strName == L"default_link_font_color") {
        //超级链接：常规文本颜色值
        m_linkNormalTextColor = strValue;
    }
    else if (strName == L"hover_link_font_color") {
        //超级链接：Hover状态文本颜色值
        m_linkHoverTextColor = strValue;
    }
    else if (strName == L"mouse_down_link_font_color") {
        //超级链接：鼠标按下状态文本颜色值
        m_linkMouseDownTextColor = strValue;
    }
    else if (strName == L"link_font_underline") {
        //超级链接：是否使用带下划线的字体
        m_bLinkUnderlineFont = (strValue == L"true");
    }
    else if (strName == L"text") {
        //允许使用'{'代替'<'，'}'代替'>'
        if (((strValue.find(L'<') == std::wstring::npos) && (strValue.find(L'>') == std::wstring::npos)) &&
            ((strValue.find(L'{') != std::wstring::npos) && (strValue.find(L'}') != std::wstring::npos))) {
            std::wstring richText(strValue);
            StringHelper::ReplaceAll(L"{", L"<", richText);
            StringHelper::ReplaceAll(L"}", L">", richText);
            SetText(richText);
        }
        else {
            SetText(strValue);
        }        
    }
    else if ((strName == L"text_id") || (strName == L"textid")) {
        SetTextId(strValue);
    }
    else if (strName == L"trim_policy") {
        if (strValue == L"all") {
            m_trimPolicy = TrimPolicy::kAll;
        }
        else if (strValue == L"none") {
            m_trimPolicy = TrimPolicy::kNone;
        }
        else if (strValue == L"keep_one") {
            m_trimPolicy = TrimPolicy::kKeepOne;
        }
        else {
            m_trimPolicy = TrimPolicy::kAll;
        }
    }
    else {
        __super::SetAttribute(strName, strValue);
    }
}

void RichText::ChangeDpiScale(uint32_t nOldDpiScale, uint32_t nNewDpiScale)
{
    ASSERT(nNewDpiScale == Dpi().GetScale());
    if (nNewDpiScale != Dpi().GetScale()) {
        return;
    }
    UiPadding rcTextPadding = GetTextPadding();
    rcTextPadding = Dpi().GetScalePadding(rcTextPadding, nOldDpiScale);
    SetTextPadding(rcTextPadding, false);

    __super::ChangeDpiScale(nOldDpiScale, nNewDpiScale);
}

void RichText::CalcDestRect(IRender* pRender, UiRect rc, UiRect& rect)
{
    rect.Clear();
    if (!m_textData.empty()) {
        std::vector<RichTextData> richTextData;
        richTextData.reserve(m_textData.size());
        for (const RichTextData& textData : m_textData) {
            richTextData.push_back(textData);
        }
        pRender->DrawRichText(rc, richTextData, m_uTextStyle, true, (uint8_t)GetAlpha());
        for (size_t index = 0; index < richTextData.size(); ++index) {
            m_textData[index].m_textRects = richTextData[index].m_textRects;
        }
    }
    for (const RichTextData& textData : m_textData) {
        for (const UiRect& textRect : textData.m_textRects) {
            if (rect.IsZero()) {
                rect = textRect;
            }
            else {
                rect.Union(textRect);
            }
        }
    }
}

UiSize RichText::EstimateText(UiSize szAvailable)
{
    UiSize fixedSize;
    IRender* pRender = nullptr;
    if (GetWindow() != nullptr) {
        pRender = GetWindow()->GetRender();
    }
    if (pRender == nullptr) {
        return fixedSize;
    }

    int32_t nWidth = szAvailable.cx;
    if (GetFixedWidth().IsStretch()) {
        //如果是拉伸类型，使用外部宽度
        nWidth = CalcStretchValue(GetFixedWidth(), szAvailable.cx);
    }
    else if (GetFixedWidth().IsInt32()) {
        nWidth = GetFixedWidth().GetInt32();
    }

    //最大高度，不限制
    int32_t nHeight = INT_MAX;

    UiRect rc;
    rc.left = 0;
    rc.right = rc.left + nWidth;
    rc.top = 0;
    rc.bottom = rc.top + nHeight;
    rc.Deflate(GetControlPadding());
    rc.Deflate(GetTextPadding());

    if (rc.IsEmpty()) {
        return fixedSize;
    }

    //检查并更新文本
    CheckParseText();

    //计算绘制所占的区域大小
    UiRect rect;
    CalcDestRect(pRender, rc, rect);

    UiPadding rcTextPadding = GetTextPadding();
    UiPadding rcPadding = GetControlPadding();
    if (GetFixedWidth().IsAuto()) {
        fixedSize.cx = rect.Width() + rcTextPadding.left + rcTextPadding.right;
        fixedSize.cx += (rcPadding.left + rcPadding.right);
    }
    if (GetFixedHeight().IsAuto()) {
        fixedSize.cy = rect.Height() + rcTextPadding.top + rcTextPadding.bottom;
        fixedSize.cy += (rcPadding.top + rcPadding.bottom);
    }
    return fixedSize;
}

void RichText::PaintText(IRender* pRender)
{
    if (pRender == nullptr) {
        return;
    }
    UiRect rc = GetRect();
    rc.Deflate(GetControlPadding());
    rc.Deflate(GetTextPadding());

    //检查并更新文本
    CheckParseText();

    //如果设置了对齐方式，需要评估绘制位置
    if ((m_uTextStyle & (TEXT_CENTER | TEXT_RIGHT | TEXT_VCENTER | TEXT_BOTTOM))) {
        //计算绘制所占的区域大小
        UiRect rect;
        CalcDestRect(pRender, rc, rect);
        if ((rect.Width() < rc.Width()) && (m_uTextStyle & (TEXT_CENTER | TEXT_RIGHT))) {            
            //水平方向
            int32_t diff = rc.Width() - rect.Width();
            if (m_uTextStyle & TEXT_CENTER) {
                rc.Offset(diff / 2, 0);
            }
            else if (m_uTextStyle & TEXT_RIGHT) {
                rc.Offset(diff, 0);
            }
        }
        if ((rect.Height() < rc.Height()) && (m_uTextStyle & (TEXT_VCENTER | TEXT_BOTTOM))) {
            //垂直方向
            int32_t diff = rc.Height() - rect.Height();
            if (m_uTextStyle & TEXT_VCENTER) {
                rc.Offset(0, diff / 2);
            }
            else if (m_uTextStyle & TEXT_BOTTOM) {
                rc.Offset(0, diff);
            }
        }
    }

    if (!m_textData.empty()) {
        UiColor normalLinkTextColor;
        if (!m_linkNormalTextColor.empty()) {
            normalLinkTextColor = GetUiColor(m_linkNormalTextColor.c_str());
        }
        UiColor mouseDownLinkTextColor;
        if (!m_linkMouseDownTextColor.empty()) {
            mouseDownLinkTextColor = GetUiColor(m_linkMouseDownTextColor.c_str());
        }
        UiColor linkHoverTextColor;
        if (!m_linkHoverTextColor.empty()) {
            linkHoverTextColor = GetUiColor(m_linkHoverTextColor.c_str());
        }

        std::vector<RichTextData> richTextData;
        richTextData.reserve(m_textData.size());
        for (const RichTextDataEx& textDataEx : m_textData) {
            if (!textDataEx.m_linkUrl.empty()) {
                //对于超级链接，设置默认文本格式
                RichTextData textData = textDataEx;
                if (textDataEx.m_bMouseDown || textDataEx.m_bMouseHover) {
                    textData.m_fontInfo.m_bUnderline = m_bLinkUnderlineFont;//是否显示下划线字体
                }
                if (!m_linkNormalTextColor.empty()) {                    
                    if (!normalLinkTextColor.IsEmpty()) {
                        textData.m_textColor = normalLinkTextColor;//标准状态的字体颜色
                    }
                }
                if (textDataEx.m_bMouseDown && !m_linkMouseDownTextColor.empty()) {                    
                    if (!mouseDownLinkTextColor.IsEmpty()) {
                        textData.m_textColor = mouseDownLinkTextColor;//鼠标按下时的字体颜色
                    }
                }
                else if (textDataEx.m_bMouseHover && !m_linkHoverTextColor.empty()) {                    
                    if (!linkHoverTextColor.IsEmpty()) {
                        textData.m_textColor = linkHoverTextColor;//鼠标Hover时的字体颜色
                    }
                }
                richTextData.push_back(textData);
            }
            else {
                richTextData.push_back(textDataEx);
            }
        }
        pRender->DrawRichText(rc, richTextData, m_uTextStyle, false, (uint8_t)GetAlpha());
        for (size_t index = 0; index < richTextData.size(); ++index) {
            m_textData[index].m_textRects = richTextData[index].m_textRects;
        }
    }
}

void RichText::CheckParseText()
{
    if (!m_richTextId.empty() && (m_langFileName != GlobalManager::Instance().GetLanguageFileName())) {
        //多语言版：当语言发生变化时，更新文本内容
        DoSetText(GlobalManager::Instance().Lang().GetStringViaID(m_richTextId.c_str()));
        m_langFileName = GlobalManager::Instance().GetLanguageFileName();
    }

    //当DPI变化时，需要重新解析文本，更新字体大小
    if (m_nTextDataDPI != Dpi().GetDPI()) {
        m_textData.clear();
    }

    if (m_textData.empty()) {
        ParseText(m_textData);
        m_nTextDataDPI = Dpi().GetDPI();
    }
}

bool RichText::ParseText(std::vector<RichTextDataEx>& outTextData) const
{
    //默认字体
    std::wstring sFontId = GetFontId();
    IFont* pFont = GlobalManager::Instance().Font().GetIFont(sFontId, Dpi());
    ASSERT(pFont != nullptr);
    if (pFont == nullptr) {
        return false;
    }

    RichTextDataEx parentTextData;
    //默认文本颜色
    parentTextData.m_textColor = GetUiColor(GetTextColor());
    if (parentTextData.m_textColor.IsEmpty()) {
        parentTextData.m_textColor = UiColor(UiColors::Black);
    }
    parentTextData.m_fontInfo.m_fontName = pFont->FontName();
    parentTextData.m_fontInfo.m_fontSize = pFont->FontSize();
    parentTextData.m_fontInfo.m_bBold = pFont->IsBold();
    parentTextData.m_fontInfo.m_bUnderline = pFont->IsUnderline();
    parentTextData.m_fontInfo.m_bItalic = pFont->IsItalic();
    parentTextData.m_fontInfo.m_bStrikeOut = pFont->IsStrikeOut();
    parentTextData.m_fRowSpacingMul = m_fRowSpacingMul;

    std::vector<RichTextDataEx> textData;

    for (const RichTextSlice& textSlice : m_textSlice) {
        if (!ParseTextSlice(textSlice, parentTextData, textData)) {
            return false;
        }
    }
    outTextData.swap(textData);
    return true;
}

bool RichText::ParseTextSlice(const RichTextSlice& textSlice, 
                              const RichTextDataEx& parentTextData, 
                              std::vector<RichTextDataEx>& textData) const
{
    //当前节点
    RichTextDataEx currentTextData;
    currentTextData.m_fRowSpacingMul = m_fRowSpacingMul;
    currentTextData.m_text = textSlice.m_text;
    currentTextData.m_linkUrl = textSlice.m_linkUrl;
    if (!textSlice.m_textColor.empty()) {
        currentTextData.m_textColor = GetUiColor(textSlice.m_textColor.c_str());
    }
    if (textSlice.m_textColor.empty()) {
        currentTextData.m_textColor = parentTextData.m_textColor;
    }

    if (!textSlice.m_bgColor.empty()) {
        currentTextData.m_bgColor = GetUiColor(textSlice.m_bgColor.c_str());
    }
    else {
        currentTextData.m_bgColor = parentTextData.m_bgColor;
    }

    currentTextData.m_fontInfo = parentTextData.m_fontInfo;
    if (!textSlice.m_fontInfo.m_fontName.empty()) {
        currentTextData.m_fontInfo.m_fontName = textSlice.m_fontInfo.m_fontName;
    }
    if (textSlice.m_fontInfo.m_fontSize > 0) {
        currentTextData.m_fontInfo.m_fontSize = Dpi().GetScaleInt(textSlice.m_fontInfo.m_fontSize); //字体大小，需要DPI缩放
    }
    if (textSlice.m_fontInfo.m_bBold) {
        currentTextData.m_fontInfo.m_bBold = textSlice.m_fontInfo.m_bBold;
    }
    if (textSlice.m_fontInfo.m_bUnderline) {
        currentTextData.m_fontInfo.m_bUnderline = textSlice.m_fontInfo.m_bUnderline;
    }
    if (textSlice.m_fontInfo.m_bItalic) {
        currentTextData.m_fontInfo.m_bItalic = textSlice.m_fontInfo.m_bItalic;
    }
    if (textSlice.m_fontInfo.m_bStrikeOut) {
        currentTextData.m_fontInfo.m_bStrikeOut = textSlice.m_fontInfo.m_bStrikeOut;
    }
    if (!currentTextData.m_text.empty() || !currentTextData.m_linkUrl.empty()) {
        textData.push_back(currentTextData);
    }
        
    if (!textSlice.m_childs.empty()) {
        //子节点
        for (const RichTextSlice& childSlice : textSlice.m_childs) {
            if (!ParseTextSlice(childSlice, currentTextData, textData)) {
                return false;
            }
        }
    }
    return true;
}

UiPadding RichText::GetTextPadding() const
{
    return UiPadding(m_rcTextPadding.left, m_rcTextPadding.top, m_rcTextPadding.right, m_rcTextPadding.bottom);
}

void RichText::SetTextPadding(UiPadding padding, bool bNeedDpiScale)
{
    ASSERT((padding.left >= 0) && (padding.top >= 0) && (padding.right >= 0) && (padding.bottom >= 0));
    if ((padding.left < 0) || (padding.top < 0) ||
        (padding.right < 0) || (padding.bottom < 0)) {
        return;
    }
    if (bNeedDpiScale) {
        Dpi().ScalePadding(padding);
    }
    if (!GetTextPadding().Equals(padding)) {
        m_rcTextPadding.left = TruncateToUInt16(padding.left);
        m_rcTextPadding.top = TruncateToUInt16(padding.top);
        m_rcTextPadding.right = TruncateToUInt16(padding.right);
        m_rcTextPadding.bottom = TruncateToUInt16(padding.bottom);
        RelayoutOrRedraw();
    }
}

const std::wstring& RichText::TrimText(std::wstring& text)
{
    if (m_trimPolicy == TrimPolicy::kNone) {
        //不处理
    }
    else if (m_trimPolicy == TrimPolicy::kKeepOne) {
        //只保留一个空格
        if (!text.empty()) {
            bool bFirst = (text.front() == L' ');
            bool bLast = text[text.size() - 1] == L' ';
            StringHelper::Trim(text);
            if (text.empty()) {
                text = L" ";
            }
            else {
                if (bFirst) {
                    text = L" " + text;
                }
                else if (bLast) {
                    text += L" ";
                }
            }
        }
    }
    else {
        //去掉所有空格
        StringHelper::Trim(text);
    }    
    return text;
}

std::wstring RichText::TrimText(const wchar_t* text)
{
    if (m_trimPolicy == TrimPolicy::kNone) {
        //不处理
        std::wstring retText;
        if (text != nullptr) {
            retText = text;
        }
        return retText;
    }
    else if (m_trimPolicy == TrimPolicy::kKeepOne) {
        //只保留一个空格
        std::wstring retText;
        if (text != nullptr) {
            retText = text;
        }
        TrimText(retText);
        return retText;
    }
    else {
        //去掉所有空格
        return StringHelper::Trim(text);
    }    
}

bool RichText::DoSetText(const std::wstring& richText)
{
    Clear();
    //XML解析的内容，全部封装在WindowBuilder这个类中，以避免到处使用XML解析器，从而降低代码维护复杂度
    bool bResult = true;
    if (!richText.empty()) {
        if (richText.find(L"<RichText") == std::wstring::npos) {
            std::wstring formatedText = L"<RichText>" + richText + L"</RichText>";
            bResult = WindowBuilder::ParseRichTextXmlText(formatedText, this);
        }
        else {
            bResult = WindowBuilder::ParseRichTextXmlText(richText, this);
        }
    }
    return bResult;
}

bool RichText::SetText(const std::wstring& richText)
{
    bool bResult = DoSetText(richText);
    if (bResult) {
        RelayoutOrRedraw();
    }
    return bResult;
}

bool RichText::SetTextId(const std::wstring& richTextId)
{
    bool bRet = SetText(GlobalManager::Instance().Lang().GetStringViaID(richTextId));
    m_richTextId = richTextId;
    if (!m_richTextId.empty()) {
        m_langFileName = GlobalManager::Instance().GetLanguageFileName();
    }
    else {
        m_langFileName.clear();
    }
    return bRet;
}

void RichText::Clear()
{
    m_textData.clear();
    if (!m_textSlice.empty()) {
        m_textSlice.clear();
        Invalidate();
    }
}

std::wstring RichText::GetFontId() const
{
    return m_sFontId.c_str();
}

void RichText::SetFontId(const std::wstring& strFontId)
{
    if (m_sFontId != strFontId) {
        m_sFontId = strFontId;
        m_textData.clear();
        Invalidate();
    }
}

std::wstring RichText::GetTextColor() const
{
    return m_sTextColor.c_str();
}

void RichText::SetTextColor(const std::wstring& sTextColor)
{
    if (m_sTextColor != sTextColor) {
        m_sTextColor = sTextColor;
        m_textData.clear();
        Invalidate();
    }
}

float RichText::GetRowSpacingMul() const
{
    return m_fRowSpacingMul;
}

void RichText::SetRowSpacingMul(float fRowSpacingMul)
{
    if (m_fRowSpacingMul != fRowSpacingMul) {
        m_fRowSpacingMul = fRowSpacingMul;
        if (m_fRowSpacingMul <= 0.01f) {
            m_fRowSpacingMul = 1.0f;
        }
        m_textData.clear();
        Invalidate();
    }
}

void RichText::AppendTextSlice(const RichTextSlice&& textSlice)
{
    m_textSlice.emplace_back(textSlice);
    m_textData.clear();
}

void RichText::AppendTextSlice(const RichTextSlice& textSlice)
{
    m_textSlice.emplace_back(textSlice);
    m_textData.clear();
}

std::wstring RichText::ToString() const
{
    const std::wstring indentValue = L"    ";
    const std::wstring lineBreak = L"\r\n";
    std::wstring richText = L"<RichText>";
    richText += lineBreak;
    for (const RichTextSlice& textSlice : m_textSlice) {
        richText += ToString(textSlice, indentValue);
    }
    richText += L"</RichText>";
    return richText;
}

std::wstring RichText::ToString(const RichTextSlice& textSlice, const std::wstring& indent) const
{
    // 支持的标签列表(兼容HTML的标签):
    // 
    // 超级链接：    <a href="URL">文本</a>
    // 粗体字:      <b> </b>
    // 斜体字:      <i> </i>
    // 删除字:      <s> </s> 或 <del> </del> 或者 <strike> </strike>
    // 下划线字:    <u> </u>
    // 设置背景色:  <bgcolor color="#000000"> </bgcolor>
    // 设置字体:    <font face="宋体" size="12" color="#000000">
    // 换行标签：   <br/>
    const std::wstring indentValue = L"    ";
    const std::wstring lineBreak = L"\r\n";
    std::wstring richText;
    if (textSlice.m_nodeName.empty()) {
        if (!textSlice.m_text.empty()) {
            richText += indent;
            richText += textSlice.m_text.c_str();
            richText += lineBreak;
        }
        //没有节点名称的情况下，就没有属性和子节点，直接返回
        return richText;
    }

    //生成属性列表
    std::wstring attrList;    
    if (!textSlice.m_linkUrl.empty()) {
        attrList += L"href=\"";
        std::wstring url = textSlice.m_linkUrl.c_str();
        attrList += textSlice.m_linkUrl.c_str();
        attrList += L"\"";
    }
    if (!textSlice.m_bgColor.empty()) {
        attrList += L"color=\"";
        attrList += textSlice.m_bgColor.c_str();
        attrList += L"\"";
    }
    if (!textSlice.m_textColor.empty()) {
        attrList += L"color=\"";
        attrList += textSlice.m_textColor.c_str();
        attrList += L"\"";
    }
    if (!textSlice.m_fontInfo.m_fontName.empty()) {
        attrList += L"face=\"";
        attrList += textSlice.m_fontInfo.m_fontName;
        attrList += L"\"";
    }
    if (textSlice.m_fontInfo.m_fontSize != 0) {
        attrList += L"size=\"";
        attrList += StringHelper::Printf(L"%d", textSlice.m_fontInfo.m_fontSize);
        attrList += L"\"";
    }

    if(!textSlice.m_childs.empty()) {
        //有子节点：节点开始
        richText += indent;
        richText += L"<";
        richText += textSlice.m_nodeName.c_str();
        if (!attrList.empty()) {
            richText += L" ";
            richText += attrList;
        }
        richText += L">";
        richText += lineBreak;

        //添加子节点
        for (const RichTextSlice& childSlice : textSlice.m_childs) {
            richText += ToString(childSlice, indent + indentValue);
        }

        //节点结束
        richText += indent;
        richText += L"</";
        richText += textSlice.m_nodeName.c_str();
        richText += L">";
        richText += lineBreak;
    }
    else if (!textSlice.m_linkUrl.empty()) {
        //超级链接节点：需要特殊处理
        richText += indent;
        richText += L"<";
        richText += textSlice.m_nodeName.c_str();
        if (!attrList.empty()) {
            richText += L" ";
            richText += attrList;
        }
        richText += L">";

        //添加超链接的文本
        richText += textSlice.m_text.c_str();

        //节点结束
        richText += L"</";
        richText += textSlice.m_nodeName.c_str();
        richText += L">";
        richText += lineBreak;
    }
    else {
        //没有子节点：放一行中表示
        richText += indent;
        richText += L"<";
        richText += textSlice.m_nodeName.c_str();
        richText += L" ";
        if (!attrList.empty()) {            
            richText += attrList;
        }
        richText += L"/>";
        richText += lineBreak;
    }
    return richText;
}

bool RichText::ButtonDown(const EventArgs& msg)
{
    bool bRet = __super::ButtonDown(msg);
    for (size_t index = 0; index < m_textData.size(); ++index) {
        RichTextDataEx& textData = m_textData[index];
        textData.m_bMouseDown = false;
        if (textData.m_linkUrl.empty()) {
            continue;
        }        
        for (const UiRect& textRect : textData.m_textRects) {
            if (textRect.ContainsPt(msg.ptMouse)) {
                //在超级链接上
                textData.m_bMouseDown = true;
                Invalidate();
            }
        }
    }
    return bRet;
}

bool RichText::ButtonUp(const EventArgs& msg)
{
    bool bRet = __super::ButtonUp(msg);
    for (size_t index = 0; index < m_textData.size(); ++index) {
        RichTextDataEx& textData = m_textData[index];
        if (textData.m_linkUrl.empty()) {
            continue;
        }
        for (const UiRect& textRect : textData.m_textRects) {
            if (textRect.ContainsPt(msg.ptMouse)) {
                //在超级链接上, 并且与按下鼠标时的相同，则触发点击事件
                if (textData.m_bMouseDown) {
                    textData.m_bMouseDown = false;
                    Invalidate();
                    std::wstring url = textData.m_linkUrl.c_str();
                    SendEvent(kEventLinkClick, (WPARAM)url.c_str());
                    return bRet;
                }                
            }
        }
    }
    bool bInvalidate = false;
    for (RichTextDataEx& textData : m_textData) {
        if (textData.m_linkUrl.empty()) {
            continue;
        }
        if (textData.m_bMouseDown) {
            textData.m_bMouseDown = false;
            bInvalidate = true;
        }
    }
    if (bInvalidate) {
        Invalidate();
    }
    return bRet;
}

bool RichText::MouseMove(const EventArgs& msg)
{
    bool bRet = __super::MouseMove(msg);
    bool bOnLinkUrl = false;
    for (RichTextDataEx& textData : m_textData) {
        textData.m_bMouseHover = false;
        if (textData.m_linkUrl.empty()) {
            continue;
        }
        for (const UiRect& textRect : textData.m_textRects) {
            if (textRect.ContainsPt(msg.ptMouse)) {
                //在超级链接上
                textData.m_bMouseHover = true;
                Invalidate();
                if (textData.m_linkUrl == GetToolTipText()) {
                    bOnLinkUrl = true;
                }
            }
        }
    }
    if (!bOnLinkUrl) {
        SetToolTipText(L"");
    }
    return bRet;
}

bool RichText::MouseHover(const EventArgs& msg)
{
    bool bRet = __super::MouseHover(msg);
    bool hasHover = false;
    for (RichTextDataEx& textData : m_textData) {
        if (textData.m_linkUrl.empty()) {
            continue;
        }
        for (const UiRect& textRect : textData.m_textRects) {
            if (textRect.ContainsPt(msg.ptMouse)) {
                //超级链接, 显示ToolTip
                SetToolTipText(textData.m_linkUrl.c_str());
                hasHover = true;
            }
        }        
    }
    if (!hasHover) {
        SetToolTipText(L"");
    }
    return bRet;
}

bool RichText::MouseLeave(const EventArgs& msg)
{
    bool bInvalidate = false;
    for (RichTextDataEx& textData : m_textData) {
        if (textData.m_linkUrl.empty()) {
            continue;
        }
        if (textData.m_bMouseHover) {
            textData.m_bMouseHover = false;
            bInvalidate = true;
        }
        if (textData.m_bMouseDown) {
            textData.m_bMouseDown = false;
            bInvalidate = true;
        }
    }
    if (bInvalidate) {
        Invalidate();
    }
    return __super::MouseLeave(msg);
}

bool RichText::OnSetCursor(const EventArgs& msg)
{
    for (const RichTextDataEx& textData : m_textData) {
        if (textData.m_linkUrl.empty()) {
            continue;
        }
        for (const UiRect& textRect : textData.m_textRects) {
            if (textRect.ContainsPt(msg.ptMouse)) {
                //超级链接，光标变成手型
                SetCursor(kCursorHand);
                return true;
            }
        }
    }
    return __super::OnSetCursor(msg);
}

} // namespace ui

