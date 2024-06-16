#include "StringUtil.h"
#include "duilib/third_party/convert_utf/ConvertUTF.h"
#include <filesystem>

using namespace llvm; //for ConvertUTF.h

namespace ui
{

#define GG_VA_COPY(a, b) (a = b)
#define COUNT_OF(array)            (sizeof(array)/sizeof(array[0]))

namespace
{

template<typename CharType>
int StringTokenizeT(const std::basic_string<CharType> &input,
                    const std::basic_string<CharType> &delimitor,
                    std::list<std::basic_string<CharType> > &output)
{
    size_t token_begin;
    size_t token_end;

    output.clear();

    for (token_begin = token_end = 0; token_end != std::basic_string<CharType>::npos;)
    {
        token_begin = input.find_first_not_of(delimitor, token_begin);
        if (token_begin == std::basic_string<CharType>::npos)
            break;
        token_end = input.find_first_of(delimitor, token_begin + 1);
        output.push_back(input.substr(token_begin, token_end - token_begin));
        token_begin = token_end + 1;
    }

    return static_cast<int>(output.size());
}

template<typename CharType>
size_t StringReplaceAllT(const std::basic_string<CharType> &find,
                         const std::basic_string<CharType> &replace,
                         std::basic_string<CharType> &output)
{
    size_t find_length = find.size();
    size_t replace_length = replace.size();
    size_t offset = 0, endpos = 0;
    size_t target = 0, found_pos = 0;
    size_t replaced = 0;
    CharType *data_ptr = nullptr;

    if (find.empty())
        return 0;

    /*
     * to avoid extra memory reallocating,
     * we use two passes to finish the task in the case that replace.size() is greater find.size()
     */

    if (find_length < replace_length)
    {
        /* the first pass, count all available 'find' to be replaced  */
        for (;;)
        {
            offset = output.find(find, offset);
            if (offset == std::basic_string<CharType>::npos)
                break;
            replaced++;
            offset += find_length;
        }

        if (replaced == 0)
            return 0;

        size_t newsize = output.size() + replaced * (replace_length - find_length);

        /* we apply for more memory to hold the content to be replaced */
        endpos = newsize;
        offset = newsize - output.size();
        output.resize(newsize);
        data_ptr = &output[0];

        memmove((void*)(data_ptr + offset),
                (void*)data_ptr,
                (output.size() - offset) * sizeof(CharType));
    }
    else
    {
        endpos = output.size();
        offset = 0;
        data_ptr = (CharType*)(&output[0]);
    }

    /* the second pass,  the replacement */
    while (offset < endpos)
    {
        found_pos = output.find(find, offset);
        if (found_pos != std::basic_string<CharType>::npos)
        {
            /* move the content between two targets */
            if (target != found_pos)
                memmove((void*)(data_ptr + target),
                        (void*)(data_ptr + offset),
                        (found_pos - offset) * sizeof(CharType));

            target += found_pos - offset;

            /* replace */
            memcpy(data_ptr + target,
                   replace.data(),
                   replace_length * sizeof(CharType));

            target += replace_length;
            offset = find_length + found_pos;
            replaced++;
        }
        else
        {
            /* ending work  */
            if (target != offset)
                memcpy((void*)(data_ptr + target),
                       (void*)(data_ptr + offset),
                       (endpos - offset) * sizeof(CharType));
            break;
        }
    }

    if (replace_length < find_length)
        output.resize(output.size() - replaced * (find_length - replace_length));

    return replaced;
}

inline int vsnprintfT(char *dst, size_t count, const char *format, va_list ap)
{
    return vsnprintf(dst, count, format, ap);
}

inline int vsnprintfT(wchar_t *dst, size_t count, const wchar_t *format, va_list ap)
{
    return _vsnwprintf_s(dst, count, count, format, ap);
}

template<typename CharType>
void StringAppendVT(const CharType *format, va_list ap, std::basic_string<CharType> &output)
{
    CharType stack_buffer[1024] = {0, };

    /* first, we try to finish the task using a fixed-size buffer in the stack */
    va_list ap_copy;
    GG_VA_COPY(ap_copy, ap);

    int result = vsnprintfT(stack_buffer, COUNT_OF(stack_buffer), format, ap_copy);
    va_end(ap_copy);
    if (result >= 0 && result < static_cast<int>(COUNT_OF(stack_buffer)))
    {
        /* It fits */
        output.append(stack_buffer, result);
        return;
    }

    /* then, we have to repeatedly increase buffer size until it fits. */
    int buffer_size = COUNT_OF(stack_buffer);
    std::basic_string<CharType> heap_buffer;
    for (;;)
    {
        if (result != -1)
        {
            ASSERT(0);
            return; /* not expected, result should be -1 here */
        }
        buffer_size <<= 1; /* try doubling the buffer size */
        if (buffer_size > 32 * 1024 * 1024)
        {
            ASSERT(0);
            return;    /* too long */
        }
        /* resize */
        heap_buffer.resize(buffer_size);
        /*
         * NOTE: You can only use a va_list once.  Since we're in a while loop, we
         * need to make a new copy each time so we don't use up the original.
         */
        GG_VA_COPY(ap_copy, ap);
        result = vsnprintfT(&heap_buffer[0], buffer_size, format, ap_copy);
        va_end(ap_copy);

        if ((result >= 0) && (result < buffer_size)) {
            /* It fits */
            output.append(&heap_buffer[0], result);
            return;
        }
    }
}

#define NOT_SPACE(x) ((x) != 0x20 && ((x) < 0 || 0x1d < (x)))

template<typename CharType>
void StringTrimT(std::basic_string<CharType> &output)
{
    if (output.empty()) {
        return;
    }
    size_t bound1 = 0;
    size_t bound2 = output.length();
    const CharType *src = output.data();
    if (src == nullptr) {
        return;
    }

    for (; bound2 > 0; bound2--)
        if (NOT_SPACE(src[bound2-1]))
            break;

    for (; bound1 < bound2; bound1++)
        if (NOT_SPACE(src[bound1]))
            break;

    if (bound1 < bound2) {
        memmove((void *)src,
            src + bound1,
            sizeof(CharType) * (bound2 - bound1));
    }

    output.resize(bound2 - bound1);
}

template<typename CharType>
void StringTrimLeftT(std::basic_string<CharType> &output)
{
    size_t check = 0;
    size_t length = output.length();
    const CharType *src = output.data();
    if (src == nullptr) {
        return;
    }

    for (; check < length; check++)
        if (NOT_SPACE(src[check]))
            break;

    output.erase(0, check);
}

template<typename CharType>
void StringTrimRightT(std::basic_string<CharType> &output)
{
    size_t length = output.length();
    const CharType *src = output.data();
    if (src == nullptr) {
        return;
    }

    for (; length > 0; length--)
        if (NOT_SPACE(src[length-1]))
            break;

    output.resize(length);
}

} // anonymous namespace


std::wstring StringUtil::Printf(const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    std::wstring output;
    StringAppendVT<wchar_t>(format, args, output);
    va_end(args);
    return output;
}

std::string StringUtil::Printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string output;
    StringAppendVT<char>(format, args, output);
    va_end(args);
    return output;
}

size_t StringUtil::ReplaceAll(const std::wstring& find, const std::wstring& replace, std::wstring& output)
{
    if (output.empty()) {
        return 0;
    }
    return StringReplaceAllT<wchar_t>(find, replace, output);
}

size_t StringUtil::ReplaceAll(const std::string& find, const std::string& replace, std::string& output)
{
    if (output.empty())    {
        return 0;
    }
    return StringReplaceAllT<char>(find, replace, output);
}

void StringUtil::LowerString(std::string& str)
{
    if (str.empty()) {
        return;
    }
    char* start = &str[0];
    char* end = start + str.size();
    for (; start < end; start++) {
        if (*start >= 'A' && *start <= 'Z') {
            *start += 'a' - 'A';
        }
    }
}

void StringUtil::LowerString(std::wstring& str)
{
    if (str.empty()) {
        return;
    }
    wchar_t* start = &str[0];
    wchar_t* end = start + str.size();
    for (; start < end; start++) {
        if (*start >= L'A' && *start <= L'Z') {
            *start += L'a' - L'A';
        }
    }
}

void StringUtil::UpperString(std::string& str)
{
    if (str.empty()) {
        return;
    }
    char* start = &str[0];
    char* end = start + str.size();
    for (; start < end; start++) {
        if (*start >= 'a' && *start <= 'z') {
            *start -= 'a' - 'A';
        }
    }
}

void StringUtil::UpperString(std::wstring& str)
{
    if (str.empty()) {
        return;
    }
    wchar_t* start = &str[0];
    wchar_t* end = start + str.size();
    for (; start < end; start++) {
        if (*start >= L'a' && *start <= L'z') {
            *start -= L'a' - L'A';
        }
    }
}

std::wstring StringUtil::MakeLowerString(const std::wstring&str)
{
    std::wstring resStr = str;
    if (resStr.empty()) {
        return L"";
    }
    wchar_t *start = &resStr[0];
    wchar_t *end = start + resStr.size();
    for (; start < end; start++) {
        if (*start >= L'A' && *start <= L'Z') {
            *start += L'a' - L'A';
        }
    }    
    return resStr;
}

std::string StringUtil::MakeLowerString(const std::string& str)
{
    std::string resStr = str;
    if (resStr.empty()) {
        return "";
    }
    char* start = &resStr[0];
    char* end = start + resStr.size();
    for (; start < end; start++) {
        if (*start >= 'A' && *start <= 'Z') {
            *start += 'a' - 'A';
        }
    }
    return resStr;
}

std::wstring StringUtil::MakeUpperString(const std::wstring &str)
{
    DString resStr = str;
    if (resStr.empty()) {
        return L"";
    }
    wchar_t *start = &resStr[0];
    wchar_t *end = start + resStr.size();
    for (; start < end; ++start) {
        if (*start >= L'a' && *start <= L'z') {
            *start -= L'a' - L'A';
        }
    }
    return resStr;
}

std::string StringUtil::MakeUpperString(const std::string& str)
{
    std::string resStr = str;
    if (resStr.empty()) {
        return "";
    }
    char* start = &resStr[0];
    char* end = start + resStr.size();
    for (; start < end; ++start) {
        if (*start >= 'a' && *start <= 'z') {
            *start -= 'a' - 'A';
        }
    }
    return resStr;
}

std::wstring StringUtil::UTF8ToUTF16(const UTF8Char* utf8, size_t length)
{
    std::vector<UTF16Char> data;
    data.resize(8192);
    UTF16Char* output = &data[0];
    const UTF8* src_begin = reinterpret_cast<const UTF8*>(utf8);
    const UTF8* src_end = src_begin + length;
    UTF16* dst_begin = reinterpret_cast<UTF16*>(output);

    std::wstring utf16;
    while (src_begin < src_end)
    {
        ConversionResult result = ConvertUTF8toUTF16(&src_begin,
            src_end,
            &dst_begin,
            dst_begin + data.size(),
            lenientConversion);

        utf16.append(output, dst_begin - reinterpret_cast<UTF16*>(output));
        dst_begin = reinterpret_cast<UTF16*>(output);
        if (result == sourceIllegal || result == sourceExhausted)
        {
            utf16.clear();
            break;
        }
    }

    return utf16;
}

std::string StringUtil::UTF16ToUTF8(const UTF16Char* utf16, size_t length)
{
    std::vector<UTF8Char> data;
    data.resize(8192);
    UTF8Char* output = &data[0];
    const UTF16* src_begin = reinterpret_cast<const UTF16*>(utf16);
    const UTF16* src_end = src_begin + length;
    UTF8* dst_begin = reinterpret_cast<UTF8*>(output);

    std::string utf8;
    while (src_begin < src_end)
    {
        ConversionResult result = ConvertUTF16toUTF8(&src_begin,
            src_end,
            &dst_begin,
            dst_begin + data.size(),
            lenientConversion);

        utf8.append(output, dst_begin - reinterpret_cast<UTF8*>(output));
        dst_begin = reinterpret_cast<UTF8*>(output);
        if (result == sourceIllegal || result == sourceExhausted)
        {
            utf8.clear();
            break;
        }
    }

    return utf8;
}

std::basic_string<UTF32Char> StringUtil::UTF8ToUTF32(const UTF8Char* utf8, size_t length)
{
    std::vector<UTF32Char> data;
    data.resize(8192);
    UTF32Char* output = &data[0];
    const UTF8* src_begin = reinterpret_cast<const UTF8*>(utf8);
    const UTF8* src_end = src_begin + length;
    UTF32* dst_begin = reinterpret_cast<UTF32*>(output);

    std::basic_string<UTF32Char> utf32;
    while (src_begin < src_end)
    {
        ConversionResult result = ConvertUTF8toUTF32(&src_begin,
            src_end,
            &dst_begin,
            dst_begin + data.size(),
            lenientConversion);

        utf32.append(output, dst_begin - reinterpret_cast<UTF32*>(output));
        dst_begin = reinterpret_cast<UTF32*>(output);
        if (result == sourceIllegal || result == sourceExhausted)
        {
            utf32.clear();
            break;
        }
    }

    return utf32;
}

std::string StringUtil::UTF32ToUTF8(const UTF32Char* utf32, size_t length)
{
    std::vector<UTF8Char> data;
    data.resize(8192);
    UTF8Char* output = &data[0];
    const UTF32* src_begin = reinterpret_cast<const UTF32*>(utf32);
    const UTF32* src_end = src_begin + length;
    UTF8* dst_begin = reinterpret_cast<UTF8*>(output);

    std::string utf8;
    while (src_begin < src_end)
    {
        ConversionResult result = ConvertUTF32toUTF8(&src_begin,
            src_end,
            &dst_begin,
            dst_begin + data.size(),
            lenientConversion);

        utf8.append(output, dst_begin - reinterpret_cast<UTF8*>(output));
        dst_begin = reinterpret_cast<UTF8*>(output);
        if (result == sourceIllegal || result == sourceExhausted)
        {
            utf8.clear();
            break;
        }
    }

    return utf8;
}

std::basic_string<UTF32Char> StringUtil::UTF16ToUTF32(const UTF16Char* utf16, size_t length)
{
    std::vector<UTF32Char> data;
    data.resize(8192);
    UTF32Char* output = &data[0];
    const UTF16* src_begin = reinterpret_cast<const UTF16*>(utf16);
    const UTF16* src_end = src_begin + length;
    UTF32* dst_begin = reinterpret_cast<UTF32*>(output);

    std::basic_string<UTF32Char> utf32;
    while (src_begin < src_end)
    {
        ConversionResult result = ConvertUTF16toUTF32(&src_begin,
            src_end,
            &dst_begin,
            dst_begin + data.size(),
            lenientConversion);

        utf32.append(output, dst_begin - reinterpret_cast<UTF32*>(output));
        dst_begin = reinterpret_cast<UTF32*>(output);
        if (result == sourceIllegal || result == sourceExhausted)
        {
            utf32.clear();
            break;
        }
    }

    return utf32;
}

std::wstring StringUtil::UTF32ToUTF16(const UTF32Char* utf32, size_t length)
{
    std::vector<UTF16Char> data;
    data.resize(8192);
    UTF16Char* output = &data[0];
    const UTF32* src_begin = reinterpret_cast<const UTF32*>(utf32);
    const UTF32* src_end = src_begin + length;
    UTF16* dst_begin = reinterpret_cast<UTF16*>(output);

    std::wstring utf16;
    while (src_begin < src_end)
    {
        ConversionResult result = ConvertUTF32toUTF16(&src_begin,
            src_end,
            &dst_begin,
            dst_begin + data.size(),
            lenientConversion);

        utf16.append(output, dst_begin - reinterpret_cast<UTF16*>(output));
        dst_begin = reinterpret_cast<UTF16*>(output);
        if (result == sourceIllegal || result == sourceExhausted)
        {
            utf16.clear();
            break;
        }
    }

    return utf16;
}

std::wstring StringUtil::UTF8ToUTF16(const std::string& utf8)
{
    return UTF8ToUTF16(utf8.c_str(), utf8.length());
}

std::string StringUtil::UTF16ToUTF8(const std::wstring& utf16)
{
    return UTF16ToUTF8(utf16.c_str(), utf16.length());
}

std::string StringUtil::TToUTF8(const DString& str)
{
#ifdef DUILIB_UNICODE
    return StringUtil::UTF16ToUTF8(str);
#else
    return str;
#endif
}

DString StringUtil::UTF8ToT(const UTF8Char* utf8, size_t length)
{
#ifdef DUILIB_UNICODE
    return StringUtil::UTF8ToUTF16(utf8, length);
#else
    return std::string(utf8, length);
#endif
}

DString StringUtil::UTF8ToT(const std::string& utf8)
{
#ifdef DUILIB_UNICODE
    return StringUtil::UTF8ToUTF16(utf8);
#else
    return utf8;
#endif
}

std::basic_string<UTF32Char> StringUtil::UTF8ToUTF32(const std::string& utf8)
{
    return UTF8ToUTF32(utf8.c_str(), utf8.length());
}

std::string StringUtil::UTF32ToUTF8(const std::basic_string<UTF32Char>& utf32)
{
    return UTF32ToUTF8(utf32.c_str(), utf32.length());
}

std::basic_string<UTF32Char> StringUtil::UTF16ToUTF32(const DString& utf16)
{
    return UTF16ToUTF32(utf16.c_str(), utf16.length());
}

std::wstring StringUtil::UTF32ToUTF16(const std::basic_string<UTF32Char>& utf32)
{
    return UTF32ToUTF16(utf32.c_str(), utf32.length());
}

#ifdef DUILIB_PLATFORM_WIN
bool StringUtil::MBCSToUnicode(const std::string &input, DString& output, int code_page)
{
    output.clear();
    int length = ::MultiByteToWideChar(code_page, 0, input.c_str(), static_cast<int>(input.size()), NULL, 0);
    output.resize(length);
    if (output.empty())
        return true;
    ::MultiByteToWideChar(code_page,
        0,
        input.c_str(),
        static_cast<int>(input.size()),
        &output[0],
        static_cast<int>(output.size()));
    return true;
}

bool StringUtil::UnicodeToMBCS(const DString& input, std::string &output, int code_page)
{
    output.clear();
    int length = ::WideCharToMultiByte(code_page, 0, input.c_str(), static_cast<int>(input.size()), NULL, 0, NULL, NULL);
    output.resize(length);
    if (output.empty())
        return true;
    ::WideCharToMultiByte(code_page,
        0,
        input.c_str(),
        static_cast<int>(input.size()),
        &output[0],
        static_cast<int>(output.size()),
        NULL,
        NULL);
    return true;
}
#endif

std::string StringUtil::TrimLeft(const char *input)
{
    std::string output = input;
    TrimLeft(output);
    return output;
}

std::string StringUtil::TrimRight(const char *input)
{
    std::string output = input;
    TrimRight(output);
    return output;
}

std::string StringUtil::Trim(const char *input) /* both left and right */
{
    std::string output = input;
    Trim(output);
    return output;
}

std::string& StringUtil::TrimLeft(std::string &input)
{
    StringTrimLeftT<char>(input);
    return input;
}

std::string& StringUtil::TrimRight(std::string &input)
{
    StringTrimRightT<char>(input);
    return input;
}

std::string& StringUtil::Trim(std::string &input) /* both left and right */
{
    StringTrimT<char>(input);
    return input;
}

std::wstring StringUtil::TrimLeft(const wchar_t *input)
{
    std::wstring output = input;
    TrimLeft(output);
    return output;
}

std::wstring StringUtil::TrimRight(const wchar_t *input)
{
    std::wstring output = input;
    TrimRight(output);
    return output;
}

std::wstring StringUtil::Trim(const wchar_t *input) /* both left and right */
{
    std::wstring output = input;
    Trim(output);
    return output;
}

std::wstring& StringUtil::TrimLeft(std::wstring&input)
{
    StringTrimLeftT<wchar_t>(input);
    return input;
}

std::wstring& StringUtil::TrimRight(std::wstring&input)
{
    StringTrimRightT<wchar_t>(input);
    return input;
}

std::wstring& StringUtil::Trim(std::wstring&input) /* both left and right */
{
    StringTrimT<wchar_t>(input);
    return input;
}


std::list<std::string> StringUtil::Split(const std::string& input, const std::string& delimitor)
{
    std::list<std::string> output;
    std::string input2(input);

    if (input2.empty())
        return output;

    char* context = nullptr;
    char *token = strtok_s(input2.data(), delimitor.c_str(), &context);
    while (token != NULL)
    {
        output.push_back(token);
        token = strtok_s(NULL, delimitor.c_str(), &context);
    }
    return output;
}

std::list<DString> StringUtil::Split(const std::wstring& input, const std::wstring& delimitor)
{
    std::list<std::wstring> output;
    std::wstring input2(input);

    if (input2.empty())
        return output;

    wchar_t* context = nullptr;
    wchar_t* token = wcstok_s(input2.data(), delimitor.c_str(), &context);
    while (token != NULL)
    {
        output.push_back(token);
        token = wcstok_s(NULL, delimitor.c_str(), &context);
    }
    return output;
}

static bool IsEqualNoCasePrivate(const wchar_t* lhs, const wchar_t* rhs)
{
    if ((lhs == nullptr) || (rhs == nullptr)) {
        return true;
    }
    if (lhs == rhs) {
        return true;
    }
    for (;;) {
        if (*lhs == *rhs) {
            if (*lhs++ == 0) {
                return true;
            }
            rhs++;
            continue;
        }
        if (towupper(*lhs++) == towupper(*rhs++)) {
            continue;
        }
        return false;
    }
}

static bool IsEqualNoCasePrivate(const char* lhs, const char* rhs)
{
    if ((lhs == nullptr) || (rhs == nullptr)) {
        return true;
    }
    if (lhs == rhs) {
        return true;
    }
    for (;;) {
        if (*lhs == *rhs) {
            if (*lhs++ == 0) {
                return true;
            }
            rhs++;
            continue;
        }
        if (toupper(*lhs++) == toupper(*rhs++)) {
            continue;
        }
        return false;
    }
}

bool StringUtil::IsEqualNoCase(const std::wstring& lhs, const std::wstring& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return IsEqualNoCasePrivate(lhs.c_str(), rhs.c_str());
}

bool StringUtil::IsEqualNoCase(const wchar_t* lhs, const std::wstring& rhs)
{
    if (lhs == nullptr) {
        return false;
    }
    return IsEqualNoCasePrivate(lhs, rhs.c_str());
}

bool StringUtil::IsEqualNoCase(const char* lhs, const std::string& rhs)
{
    if (lhs == nullptr) {
        return false;
    }
    return IsEqualNoCasePrivate(lhs, rhs.c_str());
}

bool StringUtil::IsEqualNoCase(const std::wstring& lhs, const wchar_t* rhs)
{
    if (rhs == nullptr) {
        return false;
    }
    return IsEqualNoCasePrivate(lhs.c_str(), rhs);
}

bool StringUtil::IsEqualNoCase(const std::string& lhs, const char* rhs)
{
    if (rhs == nullptr) {
        return false;
    }
    return IsEqualNoCasePrivate(lhs.c_str(), rhs);
}

bool StringUtil::IsEqualNoCase(const wchar_t* lhs, const wchar_t* rhs)
{
    if (lhs == nullptr) {
        return (rhs == nullptr) ? true : false;
    }
    else if (rhs == nullptr) {
        return (lhs == nullptr) ? true : false;
    }
    return IsEqualNoCasePrivate(lhs, rhs);
}

bool StringUtil::IsEqualNoCase(const char* lhs, const char* rhs)
{
    if (lhs == nullptr) {
        return (rhs == nullptr) ? true : false;
    }
    else if (rhs == nullptr) {
        return (lhs == nullptr) ? true : false;
    }
    return IsEqualNoCasePrivate(lhs, rhs);
}

std::wstring StringUtil::UInt64ToStringW(uint64_t value)
{
    wchar_t temp[32] = {0, };
    int pos = 0;
    do {
        temp[pos++] = (wchar_t)(L'0' + (int)(value % 10));
        value /= 10;
    } while (value != 0);

    std::wstring str;
    do {
        str += temp[--pos];
    } while (pos > 0);
    return str;
}

std::wstring StringUtil::UInt32ToStringW(uint32_t value)
{
    return UInt64ToStringW(value);
}

std::string StringUtil::UInt64ToStringA(uint64_t value)
{
    char temp[32] = { 0 };
    int pos = 0;
    do {
        temp[pos++] = (char)('0' + (int)(value % 10));
        value /= 10;
    } while (value != 0);

    std::string str;
    do {
        str += temp[--pos];
    } while (pos > 0);
    return str;
}

std::string StringUtil::UInt32ToStringA(uint32_t value)
{
    return UInt64ToStringA(value);
}

#ifdef DUILIB_UNICODE
std::wstring StringUtil::UInt64ToString(uint64_t value)
{
    return UInt64ToStringW(value);
}

std::wstring StringUtil::UInt32ToString(uint32_t value)
{
    return UInt32ToStringW(value);
}
#else

std::string StringUtil::UInt64ToString(uint64_t value)
{
    return UInt64ToStringA(value);
}

std::string StringUtil::UInt32ToString(uint32_t value)
{
    return UInt32ToStringA(value);
}

#endif
} // namespace ui
