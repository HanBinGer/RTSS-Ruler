#include "stdafx.h"
#include "RTSS_Ruler.h"
#include "RTSSSharedMemory.h"
#include "ScreenCapture.h"
#include "OpenCVPart.h"

#define M_PI 3.14159265358979323846

#define Map1 "RTSS_Ruler_Overlay"
#define Map2 "RTSS_Ruler_Placeholder"

typedef struct {
	HANDLE hMapFile;
	LPRTSS_SHARED_MEMORY pMem;
} THREAD_PARAM;

THREAD_PARAM param;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI ThreadProc(LPVOID param);
BOOL UpdateOSD(LPCSTR lpText, LPCSTR mapName);
void ReleaseOSD(LPCSTR mapName);
double ruler_scale = 225;
double ruler_pixscale = 100;
double ruler_dist = 225;
std::string outputstr = "";
CHAR ruler_outtext[256]="Distance: 0\r\nAzimuth: 0";
bool changeState;
POINT first, second;
HWND hWnd;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	initSrc();

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LPCWSTR szWindowClass = TEXT("RTSSRULER");
	LPCWSTR szTitle = TEXT("RTSS Ruler");

	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RTSSCROSSHAIR));
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;

	if (!RegisterClassExW(&wcex))
		return FALSE;

	/*HWND*/ hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0,
		425, GetSystemMetrics(SM_CYMIN) + 200,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
		return FALSE;

	CreateWindowW(
		TEXT("EDIT"),
		TEXT("Distance: 0\r\nAzimuth: 0"),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_READONLY,
		2, 2, 400, 36, hWnd, (HMENU)4, hInstance, NULL);
	
	CreateWindowW(
		TEXT("EDIT"),
		TEXT("F1 - first point (on enemy)\r\n"
			"F2 - second point (on yourself)\r\n"
			"F3 - set scale \r\n"
			"RightCtrl + Num0 - Default settings \r\n"
			"RightCtrl + Num2, Num4, Num6, Num8 - move by 1 pixel \r\n"
			"Num2, Num4, Num6, Num8 + RightShift - move by 50 pixels \r\n"
			"RightCtrl + Num+ - increase size by 10% \r\n"
			"RightCtrl + Num- - decrease size by 10%"),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_READONLY,
		2, 40, 400, 132, hWnd, (HMENU)1, hInstance, NULL);

	CreateWindowW(
		TEXT("EDIT"), TEXT("0"),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER,
		2, 174, 100, 20, hWnd, (HMENU)2, hInstance, NULL);

	CreateWindowW(
		TEXT("BUTTON"), TEXT("Apply"),
		WS_CHILD | WS_VISIBLE | WS_BORDER | BS_CENTER | BS_VCENTER,
		104, 174, 100, 20, hWnd, (HMENU)3, hInstance, NULL);

	UpdateOSD("Distance: 0\r\nAzimuth: 0", Map1);
	UpdateOSD("", Map2);

	DWORD dwThreadId;
	CreateThread(NULL, 0, ThreadProc, NULL, 0, &dwThreadId);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	
	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId)
		{
		case 3:
			CHAR toScale[256];
			GetDlgItemTextA(hWnd, 2, toScale, 256);
			ruler_scale = std::strtoul(toScale, NULL, 10);
			changeState = true;
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;


	case WM_DESTROY:
		ReleaseOSD(Map1);
		ReleaseOSD(Map2);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

DWORD WINAPI ThreadProc(LPVOID param)
{
	char markKey;
	{
		std::ifstream iFile("config.ini");
		std::string str;
		std::getline(iFile, str);
		if (str == "") {
			markKey = 0x51;
		}
		else if (str.substr(10, 2) == "X1") {
			markKey = 0x05;
		}
		else if (str.substr(10, 2) == "X2") {
			markKey = 0x06;
		}
		else {
			markKey = VkKeyScanA(str[10]);
		}

	}

	int crossX = 0, crossY = 0, crossSize = 100;
	changeState = false;
	CHAR crossFormat[256];

	INPUT MDown;
	//ZeroMemory(&MDown, sizeof(INPUT));
	MDown.type = INPUT_KEYBOARD;
	MDown.ki.wScan = MapVirtualKeyA(0x4D, MAPVK_VK_TO_VSC);
	MDown.ki.wVk = 0x4D;
	MDown.ki.dwFlags = 0 | KEYEVENTF_SCANCODE;
	MDown.ki.time = 0;
	MDown.ki.dwExtraInfo = 0;

	cv::dnn::Net net;
	initNet(net);
	std::vector<Detection> myMark;
	std::vector<Detection> yellMark;
	cv::Point center_of_rect;

	while (TRUE) {
		if (GetAsyncKeyState(markKey)) {
			std::cout << "markKey" << std::endl;
			Sleep(100);
			MDown.ki.dwFlags = 0;
			SendInput(1, &MDown, sizeof(INPUT));
			Sleep(50);
			take_screenshot("temp/screen.png");
			MDown.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE;
			SendInput(1, &MDown, sizeof(INPUT));

			cropImg();
			yolo(net, myMark, yellMark);
			if (!myMark.empty()) {
				center_of_rect = (myMark[0].box.br() + myMark[0].box.tl()) * 0.5;
				second.x = center_of_rect.x;
				second.y = center_of_rect.y;
				changeState = true;
				myMark.clear();
			}
			if (!yellMark.empty()) {
				center_of_rect = (yellMark[0].box.br() + yellMark[0].box.tl()) * 0.5;
				first.x = center_of_rect.x;
				first.y = center_of_rect.y;
				changeState = true;
				yellMark.clear();
			}
		}
		if (GetAsyncKeyState(VK_F1)) {
			GetCursorPos(&first);
			changeState = true;
		}
		if (GetAsyncKeyState(VK_F2)) {
			GetCursorPos(&second);
			changeState = true;
		}
		if (GetAsyncKeyState(VK_F3)) {
			double x = first.x - second.x;
			double y = first.y - second.y;
			ruler_pixscale = sqrt(x * x + y * y);
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_NUMPAD0)) {
			crossX = 0;
			crossY = 0;
			crossSize = 100;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RSHIFT) && GetAsyncKeyState(VK_NUMPAD4)) {
			crossX -= 50;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RSHIFT) && GetAsyncKeyState(VK_NUMPAD6)) {
			crossX += 50;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RSHIFT) && GetAsyncKeyState(VK_NUMPAD8)) {
			crossY -= 50;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RSHIFT) && GetAsyncKeyState(VK_NUMPAD2)) {
			crossY += 50;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_NUMPAD4)) {
			crossX--;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_NUMPAD6)) {
			crossX++;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_NUMPAD8)) {
			crossY--;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_NUMPAD2)) {
			crossY++;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_ADD)) {
			crossSize += 5;
			changeState = true;
		}

		if (GetAsyncKeyState(VK_RCONTROL) && GetAsyncKeyState(VK_SUBTRACT)) {
			crossSize -= 5;
			changeState = true;
		}

		if (changeState) {
			double x = first.x - second.x;
			double y = first.y - second.y;
			changeState = false;
			long double newdistance = (sqrt(x * x + y * y) * ruler_scale / ruler_pixscale) ;
			long double ang;
			if (x > 0) {
				if (y < 0) {
					ang = std::atan(x / (-y)) * 180 / M_PI;
				}
				else {
					ang = 90 + std::atan(y / x) * 180 / M_PI;
				}
			}
			else {
				if (y < 0) {
					ang = 360 - std::atan(x / y) * 180 / M_PI;
				}
				else {
					ang = 180 + std::atan((-x) / y) * 180 / M_PI;
				}
			}
			outputstr = "Distance: " + std::to_string(newdistance) + "\r\nAzimuth: " + std::to_string(ang);
			SetDlgItemTextA(hWnd, 4, outputstr.c_str());
			strcpy_s(ruler_outtext, outputstr.c_str());
			wsprintfA(crossFormat, "<P=%d,%d><S=%d>%s", crossX, crossY, crossSize, ruler_outtext);
			UpdateOSD(crossFormat, Map1);
			UpdateOSD("<P=0,0><S=100>", Map2);
		}

		Sleep(100);
	}
}

BOOL UpdateOSD(LPCSTR lpText, LPCSTR mapName)
{
	BOOL bResult = FALSE;

	HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, TEXT("RTSSSharedMemoryV2"));

	if (hMapFile)
	{
		LPVOID pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		LPRTSS_SHARED_MEMORY pMem = (LPRTSS_SHARED_MEMORY)pMapAddr;

		if (pMem)
		{
			if ((pMem->dwSignature == 'RTSS') &&
				(pMem->dwVersion >= 0x00020000))
			{
				for (DWORD dwPass = 0; dwPass < 2; dwPass++)
				{
					for (DWORD dwEntry = 1; dwEntry < pMem->dwOSDArrSize; dwEntry++)
					{
						RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)((LPBYTE)pMem + pMem->dwOSDArrOffset + dwEntry * pMem->dwOSDEntrySize);

						if (dwPass)
						{
							if (!lstrlenA(pEntry->szOSDOwner))
								lstrcpyA(pEntry->szOSDOwner, mapName);
						}

						if (!lstrcmpA(pEntry->szOSDOwner, mapName))
						{
							if (pMem->dwVersion >= 0x00020007)
							{
								if (pMem->dwVersion >= 0x0002000e)
								{
									DWORD dwBusy = _interlockedbittestandset(&pMem->dwBusy, 0);

									if (!dwBusy)
									{
										lstrcpynA(pEntry->szOSDEx, lpText, sizeof(pEntry->szOSDEx) - 1);

										pMem->dwBusy = 0;
									}
								}
								else
									lstrcpynA(pEntry->szOSDEx, lpText, sizeof(pEntry->szOSDEx) - 1);

							}
							else
								lstrcpynA(pEntry->szOSD, lpText, sizeof(pEntry->szOSD) - 1);

							pMem->dwOSDFrame++;

							bResult = TRUE;

							break;
						}
					}

					if (bResult)
						break;
				}
			}

			UnmapViewOfFile(pMapAddr);
		}

		CloseHandle(hMapFile);
	}

	return bResult;
}

void ReleaseOSD(LPCSTR mapName)
{
	HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, TEXT("RTSSSharedMemoryV2"));

	if (hMapFile)
	{
		LPVOID pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);

		LPRTSS_SHARED_MEMORY pMem = (LPRTSS_SHARED_MEMORY)pMapAddr;

		if (pMem)
		{
			if ((pMem->dwSignature == 'RTSS') &&
				(pMem->dwVersion >= 0x00020000))
			{
				for (DWORD dwEntry = 1; dwEntry < pMem->dwOSDArrSize; dwEntry++)
				{
					RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)((LPBYTE)pMem + pMem->dwOSDArrOffset + dwEntry * pMem->dwOSDEntrySize);

					if (!lstrcmpA(pEntry->szOSDOwner, mapName))
					{
						SecureZeroMemory(pEntry, pMem->dwOSDEntrySize);
						pMem->dwOSDFrame++;
					}
				}
			}

			UnmapViewOfFile(pMapAddr);
		}

		CloseHandle(hMapFile);
	}
}
