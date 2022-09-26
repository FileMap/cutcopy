#include <windows.h>
// #include <Ole2.h>
#include <shlObj.h>
#include <gdiplus.h>
#include <memory>

#include <vector>
#include <string>

#include <iostream>

#define pv(x)                  \
    std::cout << #x << ": ";   \
    for (auto k : x)           \
    {                          \
        std::cout << k << " "; \
    }                          \
    std::cout << std::endl;
#define dbg(x) std::cout << #x << ": " << x << std::endl;

using namespace Gdiplus;

std::string Utf16CStringToUtf8String(LPCWSTR input, UINT len)
{
    int target_len = WideCharToMultiByte(CP_UTF8, 0, input, len, NULL, 0, NULL, NULL);
    std::string result(target_len, '\0');
    const char *x = result.c_str();
    char *data = (char *)x;
    WideCharToMultiByte(CP_UTF8, 0, input, len, data, target_len, NULL, NULL);
    return result;
}

class ClipboardScope
{

    bool valid;

public:
    ClipboardScope()
    {
        valid = static_cast<bool>(OpenClipboard(NULL));
    }

    ~ClipboardScope()
    {
        CloseClipboard();
    }

    bool IsValid()
    {
        return valid;
    }
};

std::vector<std::string> ReadFilePaths()
{
    auto result = std::vector<std::string>();

    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        return result;
    }

    HDROP drop_files_handle = (HDROP)GetClipboardData(CF_HDROP);
    if (!drop_files_handle)
    {
        return result;
    }

    UINT file_count = DragQueryFileW(drop_files_handle, 0xFFFFFFFF, NULL, 0);
    result.reserve(file_count);
    for (UINT i = 0; i < file_count; ++i)
    {
        UINT path_len = DragQueryFileW(drop_files_handle, i, NULL, 0);
        UINT buffer_len = path_len + 1;
        std::unique_ptr<WCHAR[]> buffer(new WCHAR[buffer_len]);
        path_len = DragQueryFileW(drop_files_handle, i, buffer.get(), buffer_len);
        result.emplace_back(Utf16CStringToUtf8String(buffer.get(), path_len));
    }

    return result;
}

std::wstring Utf8StringToUtf16String(const std::string &input)
{
    int target_len = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.size(), NULL, 0);
    std::wstring result(target_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.size(), result.data(), target_len);
    return result;
}

std::wstring LongPathToShort(const std::wstring &long_path)
{
    const WCHAR *prefix = L"\\\\?\\";
    std::wstring prefixed_long_path = prefix + long_path;
    DWORD buffer_size = GetShortPathNameW(prefixed_long_path.c_str(), NULL, 0);
    auto buffer_pointer = std::unique_ptr<WCHAR[]>(new WCHAR[buffer_size]);
    GetShortPathNameW(prefixed_long_path.c_str(), buffer_pointer.get(), buffer_size);
    size_t offset = wcslen(prefix);
    return buffer_pointer.get() + offset;
}

int dropEffect = 0;

// dropEffect 0: none, 1: copy, 2: move
void changeDropEffect(int dEffect)
{
    int ef = 0;
    switch (dEffect)
    {
    case 1:
        ef = DROPEFFECT_COPY;
        break;
    case 2:
        ef = DROPEFFECT_MOVE;
        break;
    }
    dropEffect = dEffect;
}

void WriteFilePaths(const std::vector<std::string> &file_paths)
{
    std::vector<std::wstring> file_paths_unicode;
    file_paths_unicode.reserve(file_paths.size());
    for (auto p = file_paths.cbegin(); p != file_paths.cend(); ++p)
    {
        std::wstring path_unicode = Utf8StringToUtf16String(*p);
        if (path_unicode.size() > MAX_PATH)
        {
            path_unicode = LongPathToShort(path_unicode);
        }
        file_paths_unicode.emplace_back(path_unicode);
    }

    // size of DROPFILES structure followed by file paths in double null-terminated string
    SIZE_T structure_size_in_bytes = sizeof(DROPFILES);
    for (auto p = file_paths_unicode.cbegin(); p != file_paths_unicode.cend(); ++p)
    {
        structure_size_in_bytes += (p->size() + 1) * sizeof(WCHAR);
    }
    structure_size_in_bytes += sizeof(WCHAR);

    HANDLE data_handle2 = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD)); // to hold the preffered operation.
    if (!data_handle2)
    {
        return;
    }

    DWORD *data_pointer2 = static_cast<DWORD *>(GlobalLock(data_handle2));
    if (!data_pointer2)
    {
        return;
    }

    *data_pointer2 = dropEffect;

    HANDLE data_handle = GlobalAlloc(GMEM_MOVEABLE, structure_size_in_bytes);
    if (!data_handle)
    {
        return;
    }

    BYTE *data_pointer = static_cast<BYTE *>(GlobalLock(data_handle));
    if (!data_pointer)
    {
        return;
    }

    DROPFILES *drop_files_pointer = reinterpret_cast<DROPFILES *>(data_pointer);
    drop_files_pointer->pFiles = sizeof(DROPFILES);
    drop_files_pointer->fWide = TRUE;

    SIZE_T current_offset_in_bytes = sizeof(DROPFILES);
    for (auto p = file_paths_unicode.cbegin(); p != file_paths_unicode.cend(); ++p)
    {
        WCHAR *target_path = reinterpret_cast<WCHAR *>(data_pointer + current_offset_in_bytes);
        SIZE_T offset_in_chars = p->size() + 1;
        SIZE_T offset_in_bytes = offset_in_chars * sizeof(WCHAR);
        memcpy(target_path, p->c_str(), offset_in_bytes);
        current_offset_in_bytes += offset_in_bytes;
    }

    WCHAR *tail = reinterpret_cast<WCHAR *>(data_pointer + current_offset_in_bytes);
    *tail = L'\0';

    GlobalUnlock(data_handle);
    GlobalUnlock(data_handle2);

    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        GlobalFree(data_handle);
        GlobalFree(data_handle2);
        return;
    }

    EmptyClipboard();
    CLIPFORMAT cf = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
    auto res1 = SetClipboardData(cf, data_handle2);
    auto res2 = SetClipboardData(CF_HDROP, data_handle);
    // dbg(res1);
    // dbg(res2);
    // dbg(*(DWORD *)GlobalLock(res1));

    if (!res2)
    {
        GlobalFree(data_handle);
        GlobalFree(data_handle2);
        return;
    }
}

void ClearClipboard()
{
    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        return;
    }

    EmptyClipboard();
}

class GdiplusScope
{

    ULONG_PTR gdiplusToken;
    Status initStatus;

public:
    GdiplusScope()
    {
        GdiplusStartupInput gdiplusStartupInput;
        initStatus = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }

    ~GdiplusScope()
    {
        GdiplusShutdown(gdiplusToken);
    }

    bool IsValid()
    {
        return initStatus == Ok;
    }
};

struct FreeDeleter
{
    void operator()(void *x)
    {
        free(x);
    }
};

bool GetEncoderClsid(const WCHAR *format, CLSID *pClsid)
{
    UINT num = 0;  // number of image encoders
    UINT size = 0; // size of the image encoder array in bytes

    GetImageEncodersSize(&num, &size);
    if (size == 0)
    {
        return false;
    }

    std::unique_ptr<ImageCodecInfo, FreeDeleter> pImageCodecInfo((ImageCodecInfo *)(malloc(size)));
    if (!pImageCodecInfo)
    {
        return false;
    }

    if (GetImageEncoders(num, size, pImageCodecInfo.get()) != Ok)
    {
        return false;
    }

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo.get()[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo.get()[j].Clsid;
            return true;
        }
    }

    return false;
}

bool SaveBitmapAsJpeg(HBITMAP hBmp, LPCWSTR lpszFilename, ULONG uQuality)
{
    GdiplusScope gdiplus_scope;
    if (!gdiplus_scope.IsValid())
    {
        return false;
    }

    std::unique_ptr<Bitmap> pBitmap(new Bitmap(hBmp, NULL));

    CLSID imageCLSID;
    GetEncoderClsid(L"image/jpeg", &imageCLSID);

    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].Value = &uQuality;

    Status result = pBitmap->Save(lpszFilename, &imageCLSID, &encoderParams);

    return result == Ok;
}

bool SaveBitmapAsPng(HBITMAP hBmp, LPCWSTR lpszFilename)
{
    GdiplusScope gdiplus_scope;
    if (!gdiplus_scope.IsValid())
    {
        return false;
    }

    std::unique_ptr<Bitmap> pBitmap(new Bitmap(hBmp, NULL));

    CLSID imageCLSID;
    GetEncoderClsid(L"image/png", &imageCLSID);

    Status result = pBitmap->Save(lpszFilename, &imageCLSID, NULL);

    return result == Ok;
}

bool SaveClipboardImageAsJpeg(const std::string &target_path, float compression_factor)
{
    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        return false;
    }

    HBITMAP image_handle = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!image_handle)
    {
        return false;
    }

    std::wstring target_path_unicode = Utf8StringToUtf16String(target_path);
    ULONG quality = (ULONG)(compression_factor * 100);
    return SaveBitmapAsJpeg(image_handle, target_path_unicode.c_str(), quality);
}

bool SaveClipboardImageAsPng(const std::string &target_path)
{
    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        return false;
    }

    HBITMAP image_handle = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!image_handle)
    {
        return false;
    }

    std::wstring target_path_unicode = Utf8StringToUtf16String(target_path);
    return SaveBitmapAsPng(image_handle, target_path_unicode.c_str());
}

bool PutImageIntoClipboard(const std::string &image_path)
{
    GdiplusScope gdiplus_scope;
    if (!gdiplus_scope.IsValid())
    {
        return false;
    }

    std::wstring image_path_unicode = Utf8StringToUtf16String(image_path);
    std::unique_ptr<Bitmap> pImage(new Bitmap(image_path_unicode.c_str()));
    HBITMAP handle;

    if (pImage->GetHBITMAP(Color::White, &handle) != Ok)
    {
        return false;
    }

    BITMAP bm;
    GetObject(handle, sizeof bm, &bm);
    BITMAPINFOHEADER bi = {sizeof bi, bm.bmWidth, bm.bmHeight, 1, bm.bmBitsPixel, BI_RGB};

    std::vector<BYTE> vec(bm.bmWidthBytes * bm.bmHeight);
    auto hdc = GetDC(NULL);
    GetDIBits(hdc, handle, 0, bi.biHeight, vec.data(), (BITMAPINFO *)&bi, 0);
    ReleaseDC(NULL, hdc);

    auto hmem = GlobalAlloc(GMEM_MOVEABLE, sizeof bi + vec.size());
    if (!hmem)
    {
        return false;
    }

    auto buffer = (BYTE *)GlobalLock(hmem);
    memcpy(buffer, &bi, sizeof bi);
    memcpy(buffer + sizeof bi, vec.data(), vec.size());
    GlobalUnlock(hmem);

    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        GlobalFree(hmem);
        return false;
    }

    EmptyClipboard();

    if (!SetClipboardData(CF_DIB, hmem))
    {
        GlobalFree(hmem);
        return false;
    }

    return true;
}

bool ClipboardHasImage()
{
    ClipboardScope clipboard_scope;
    if (!clipboard_scope.IsValid())
    {
        return false;
    }

    return static_cast<bool>(GetClipboardData(CF_BITMAP));
}

FORMATETC *createPREFFormat()
{
    FORMATETC *a = new FORMATETC();
    // CLIPFORMAT
    CLIPFORMAT cfo = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
    a->cfFormat = cfo;
    // a->cfFormat = CF_HDROP;
    a->tymed = TYMED_HGLOBAL;
    a->ptd = NULL;
    a->dwAspect = DVASPECT_CONTENT;
    a->lindex = -1;
    return a;
}

int isCut()
{
    LPDATAOBJECT d;

    HRESULT res = OleGetClipboard(&d);
    if (res == S_OK)
    {
        // dbg("yes");
    }
    else
    {
        return -1;
    }

    auto a = createPREFFormat();

    STGMEDIUM b;
    // dbg(DV_E_LINDEX);
    // dbg(DV_E_FORMATETC);
    // dbg(DV_E_TYMED);
    // dbg(DV_E_DVASPECT);
    // dbg(OLE_E_NOTRUNNING);
    // dbg(STG_E_MEDIUMFULL);
    // dbg(E_UNEXPECTED);
    // dbg(E_INVALIDARG);
    // dbg(E_OUTOFMEMORY);
    HRESULT res2 = d->GetData(a, &b);
    // dbg(res2);
    // dbg(S_OK);
    if (res2 != S_OK)
    {
        delete a;
        return -1;
    }
    d->Release();
    delete a;
    a = NULL;

    DWORD *data_pointer = static_cast<DWORD *>(GlobalLock(b.hGlobal));
    if (!data_pointer)
    {
        return -1;
    }
    // dbg(*data_pointer);

    bool cutOp = false;
    if (*data_pointer & DROPEFFECT_MOVE)
    {
        cutOp = true;
    }
    GlobalUnlock(b.hGlobal);
    GlobalFree(b.hGlobal);

    // if (cutOp==-1){
    //     std::cout<<"can't access clipboard preferences."<<std::endl;
    // }else if(cutOp==1){
    //     std::cout<<"files are cut."<<std::endl;
    // }else{
    //     std::cout<<"files are copied."<<std::endl;
    // }

    return cutOp;
}