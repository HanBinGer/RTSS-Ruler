#pragma once
#include "stdafx.h"

static DEVMODEA scr;

void initSrc() {
	scr.dmSize = sizeof(DEVMODE);
	scr.dmDriverExtra = 0;
	EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &scr);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	using namespace Gdiplus;
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return 0;
}

inline void take_screenshot(const std::string& filename)
{
	using namespace Gdiplus;
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	{
		HDC scrdc, memdc;
		HBITMAP membit;
		scrdc = ::GetDC(0);
		int Height = scr.dmPelsHeight;//GetSystemMetrics(SM_CYSCREEN);//GetSystemMetrics(SM_CYVIRTUALSCREEN);
		int Width = scr.dmPelsWidth;//GetSystemMetrics(SM_CXSCREEN);// GetSystemMetrics(SM_CXVIRTUALSCREEN);
		memdc = CreateCompatibleDC(scrdc);
		membit = CreateCompatibleBitmap(scrdc, Width, Height);
		HBITMAP hOldBitmap = (HBITMAP)SelectObject(memdc, membit);
		BitBlt(memdc, 0, 0, Width, Height, scrdc, 0, 0, SRCCOPY);

		Gdiplus::Bitmap bitmap(membit, NULL);
		CLSID clsid;
		const char* name = filename.c_str();
		const size_t cSize = strlen(name) + 1;
		wchar_t* wc = new wchar_t[cSize];
		size_t retval;
		mbstowcs_s(&retval, wc, cSize, name, cSize);
		GetEncoderClsid(L"image/png", &clsid);
		bitmap.Save(wc, &clsid, NULL);

		SelectObject(memdc, hOldBitmap);

		DeleteObject(memdc);

		DeleteObject(membit);

		::ReleaseDC(0, scrdc);
	}

	GdiplusShutdown(gdiplusToken);
}