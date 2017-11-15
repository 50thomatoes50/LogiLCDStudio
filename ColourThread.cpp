/*******************************************
A Docile Sloth 2017 (adocilesloth@gmail.com)
*******************************************/

#include "LCDThreads.h"
#include "obs-frontend-api/obs-frontend-api.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <ctime>
#include "resource.h"
#include "CGdiPlusBitmap.h"
#pragma comment (lib,"Gdiplus.lib")


Gdiplus::Bitmap *background = NULL;

#define VIDEO_BUFFER_SIZE (LOGI_LCD_COLOR_WIDTH * 180 * 4)
BYTE vidBuf[VIDEO_BUFFER_SIZE];

void videoDataCallback(void* param, video_data* frame) {
	//printf("videoDataCallback");
	memcpy_s(vidBuf, VIDEO_BUFFER_SIZE, frame->data[0], VIDEO_BUFFER_SIZE);
}

const std::wstring GetWC(const char *c)
{
	const size_t cSize = strlen(c) + 1;
	std::wstring wc(cSize, L'#');
	mbstowcs(&wc[0], c, cSize);

	return wc;
}

const std::string twoDigit(int i) {
	if (i > 9) {
		return std::to_string(i);
	}
	else {
		std::string tmp("0");
		return tmp + std::to_string(i);
	}
}

void Colour(std::atomic<bool>& close)
{
	std::wstring scene;
	bool leftlast = false;
	bool rightlast = false;
	bool uplast = false;
	bool downlast = false;
	bool oklast = false;
	bool cancellast = false;
	bool ResImgFailed = true;
	std::wstring steamStatus;

	//fps
	int fps = 0;
	int fpslastime;
	int lastframes;
	std::wstring sfps;

	//bit rate
	float bitrate = 0.0;
	int bpslastime;
	int lastbytes;
	std::wstringstream sbyte;

	bool altdisplay = false;
	//dropped frames
	int dropped;
	int total;
	double percent;
	std::wstringstream frames;
	//stream time
	std::time_t tstartimeStream, tstartimeRec;
	int uptime;
	int sec;
	int min;
	int hour;
	std::wstring stime;
	std::time_t tnow;
	std::stringstream currentdate;
	char stmp[256];

	//stream info
	obs_output_t* output;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	//LogiLcdColorSetTitle(L"OBS", 255, 255, 255);

	// Initialize GDI+.
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	HDC hdc = ::GetDC(NULL);

	// Load the image. Any of the following formats are supported: BMP, GIF, JPEG, PNG, TIFF, Exif, WMF, and EMF
	CGdiPlusBitmapResource *res_bg = new CGdiPlusBitmapResource;
	CGdiPlusBitmapResource *res_mic = new CGdiPlusBitmapResource;
	CGdiPlusBitmapResource *res_micoff = new CGdiPlusBitmapResource;
	CGdiPlusBitmapResource *res_spk = new CGdiPlusBitmapResource;
	CGdiPlusBitmapResource *res_spkoff = new CGdiPlusBitmapResource;

	if (!res_bg->Load(MAKEINTRESOURCE(IDB_BACKGROUND), L"PNG", GetModuleHandle(L"LogiLCDStudio.dll"))){
		char str[256];
		sprintf_s(str, "IDB_BACKGROUND errcode =  %d \n", GetLastError());
		OutputDebugStringA(str);
		goto error_continue;
	}
	if (!res_mic->Load(MAKEINTRESOURCE(IDB_MIC), L"PNG", GetModuleHandle(L"LogiLCDStudio.dll"))) {
		char str[256];
		sprintf_s(str, "IDB_MIC errcode =  %d \n", GetLastError());
		OutputDebugStringA(str);
		goto error_continue;
	}
	if (!res_micoff->Load(MAKEINTRESOURCE(IDB_MIC_OFF), L"PNG", GetModuleHandle(L"LogiLCDStudio.dll"))) {
		char str[256];
		sprintf_s(str, "IDB_MIC_OFF errcode =  %d \n", GetLastError());
		OutputDebugStringA(str);
		goto error_continue;
	}
	if (!res_spk->Load(MAKEINTRESOURCE(IDB_SPEAKER), L"PNG", GetModuleHandle(L"LogiLCDStudio.dll"))) {
		char str[256];
		sprintf_s(str, "IDB_SPEAKER errcode =  %d \n", GetLastError());
		OutputDebugStringA(str);
		goto error_continue;
	}
	if (!res_spkoff->Load(MAKEINTRESOURCE(IDB_SPEAKER_OFF), L"PNG", GetModuleHandle(L"LogiLCDStudio.dll"))) {
		char str[256];
		sprintf_s(str, "IDB_SPEAKER_OFF errcode =  %d \n", GetLastError());
		OutputDebugStringA(str);
		goto error_continue;
	}

	ResImgFailed = false;
	// Get the bitmap handle
	HBITMAP hBitmap = NULL;
	//Gdiplus::Status status = background->GetHBITMAP(RGB(0, 0, 0), &hBitmap);
	Gdiplus::Status status = res_bg->m_pBitmap->GetHBITMAP(Gdiplus::Color(0,0,0), &hBitmap);
	if (status != Gdiplus::Ok) {
		char str[256];
		sprintf_s(str, "errcode =  %d \n", GetLastError());

		OutputDebugStringA(str);
		goto error_continue;
	}

	BITMAPINFO bitmapInfo = { 0 };
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	// Check what we got
	int ret = GetDIBits(hdc, hBitmap, 0,
		0,
		NULL,
		&bitmapInfo, DIB_RGB_COLORS);

	if (LOGI_LCD_COLOR_WIDTH != bitmapInfo.bmiHeader.biWidth || LOGI_LCD_COLOR_HEIGHT != bitmapInfo.bmiHeader.biHeight)
	{
		MessageBoxW(NULL, L"Oooops. Make sure to use a 320 by 240 image for color background.", L"LCDDemo", MB_ICONEXCLAMATION);
		goto error_continue;
	}

	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biHeight = -bitmapInfo.bmiHeader.biHeight; // this value needs to be inverted, or else image will show up upside/down

	BYTE byteBitmap[LOGI_LCD_COLOR_WIDTH * LOGI_LCD_COLOR_HEIGHT * 4]; // we have 32 bits per pixel, or 4 bytes

																	   // Gets the "bits" from the bitmap and copies them into a buffer 
																	   // which is pointed to by byteBitmap.
	ret = GetDIBits(hdc, hBitmap, 0,
		-bitmapInfo.bmiHeader.biHeight, // height here needs to be positive. Since we made it negative previously, let's reverse it again.
		&byteBitmap,
		(BITMAPINFO *)&bitmapInfo, DIB_RGB_COLORS);

	LogiLcdColorSetBackground(byteBitmap);

	// delete the image when done 
	/*if (background)
	{
		delete background;
		background = NULL;
	}*/

	error_continue:
	
	//Wait for stuff to load or obs_frontend_streaming_active() causes a crash
	obs_source_t* sceneUsed = obs_frontend_get_current_scene();
	while(!sceneUsed)
	{
		//LogiLcdColorSetText(0, L"Open Broadcasting Software", 255, 255, 255);
		//LogiLcdColorSetText(2, L"   Loading...", 255, 255, 255);
		LogiLcdUpdate();
		sceneUsed = obs_frontend_get_current_scene();
		Sleep(16);
	}
	obs_source_release(sceneUsed);

	Sleep(50);

	video_t *vid = obs_get_video();
	video_output_get_width(vid);
	video_output_get_height(vid);
	video_scale_info vinfo;
	vinfo.format = VIDEO_FORMAT_BGRA;
	vinfo.colorspace = VIDEO_CS_DEFAULT;
	vinfo.range = VIDEO_RANGE_DEFAULT;
	vinfo.height = 180/*LOGI_LCD_COLOR_HEIGHT*/;
	vinfo.width = LOGI_LCD_COLOR_WIDTH;
	video_output_connect(vid, &vinfo, videoDataCallback, NULL);//callback with frame

	while (!close)
	{
		//mute and deafen buttons
		if (leftlast == true && LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_LEFT) == false) //button released
		{
			toggleMute();
		}
		if (rightlast == true && LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_RIGHT) == false) //button rleased
		{
			toggleDeaf();
		}
		leftlast = LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_LEFT);
		rightlast = LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_RIGHT);
		if (uplast == true && LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_UP) == false) //button released
		{
			if (obs_frontend_streaming_active())
			{
				obs_frontend_streaming_stop();
			}
			else
			{
				obs_frontend_streaming_start();
			}
		}
		if (downlast == true && LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_DOWN) == false) //button rleased
		{
			if (obs_frontend_recording_active())
			{
				obs_frontend_recording_stop();
			}
			else
			{
				obs_frontend_recording_start();
			}
		}
		uplast = LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_UP);
		downlast = LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_DOWN);
		//stream and preview buttons
		/*if (oklast == true && LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_OK) == false) //button released
		{
			
		}
		if (cancellast == true && LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_CANCEL) == false) //button released
		{
			OBSStartStopPreview();
		}*/
		oklast = LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_OK);
		cancellast = LogiLcdIsButtonPressed(LOGI_LCD_COLOR_BUTTON_CANCEL);

		if (!ResImgFailed) {

			Gdiplus::Bitmap* clone = res_bg->m_pBitmap->Clone(Gdiplus::Rect(0, 0, 320, 240), PixelFormatDontCare);


			Gdiplus::FontFamily  fontFamily(L"Segoe UI");
			Gdiplus::Font        title(&fontFamily, 14, Gdiplus::FontStyleBold, Gdiplus::UnitPixel),
								 txt(&fontFamily, 12, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
			Gdiplus::PointF      pointF(22.0f, 0.0f);
			Gdiplus::SolidBrush  solidBrush(Gdiplus::Color(255, 255, 255, 255)), coltxt(Gdiplus::Color::Black);
			Gdiplus::Graphics *g = Gdiplus::Graphics::FromImage(clone);

			/*** title ***/
			scene = L"OBS - ";
			scene.append(getScene());
			g->DrawString(scene.c_str(), -1, &title, pointF, &solidBrush);

			pointF = Gdiplus::PointF(5.2f, 8.2f);

			//Icon

			if (getMute()) {
				g->DrawImage(res_micoff->m_pBitmap, 2, 22, 16,16);
			}
			else {
				g->DrawImage(res_mic->m_pBitmap, 2, 22, 16, 16);
			}

			if (getDeaf()) {
				g->DrawImage(res_spkoff->m_pBitmap, 20, 22,16,16);
			}
			else {
				g->DrawImage(res_spk->m_pBitmap, 20, 22, 16, 16);
			}

			if (obs_frontend_streaming_active() && obs_frontend_recording_active())
				steamStatus = L"Live & Rec";
			else if(!obs_frontend_streaming_active() && !obs_frontend_recording_active())
				steamStatus = L"Offline";
			else {
				if (obs_frontend_streaming_active())
					steamStatus = L"Live";
				if (obs_frontend_recording_active())
					steamStatus = L"Recording";
			}

			pointF = Gdiplus::PointF(38, 22);
			g->DrawString(steamStatus.c_str(), -1, &txt, pointF, &coltxt);
			
			//FPS - bitrate
			if (obs_frontend_streaming_active() || obs_frontend_recording_active()) {
				getFPS(fps, lastframes, fpslastime);
				getbps(bitrate, lastbytes, bpslastime);
				sprintf(stmp, "%2dFPS - %6.0fkb/s", fps, bitrate);
			}
			else {
				sprintf(stmp, "--FPS - ------kb/s");
			}
			pointF = Gdiplus::PointF(100, 22);
			g->DrawString(GetWC(stmp).c_str(), -1, &txt, pointF, &coltxt);

			//dropped
			if (obs_frontend_streaming_active() || obs_frontend_recording_active()) {
				if (obs_frontend_streaming_active()){
					output = obs_frontend_get_streaming_output();
				}
				else if (obs_frontend_recording_active()){
					output = obs_frontend_get_recording_output();
				}

				dropped = obs_output_get_frames_dropped(output);
				total = obs_output_get_total_frames(output);
				percent = (double(dropped) / total) * 100;
				sprintf(stmp, "Dropped:%d(%.2f%%)", dropped, percent);
			}
			else {
				sprintf(stmp, "Dropped:--(-.--%%)");
			}
			pointF = Gdiplus::PointF(200.0f, 22.0f);
			g->DrawString(GetWC(stmp).c_str(), -1, &txt, pointF, &coltxt);
			
			/* Bottom txt */

			//time
			tnow = std::time(nullptr);
			currentdate.str("");
			currentdate << std::put_time(std::localtime(&tnow), "%F %X");
			sprintf(stmp, "%s", currentdate.str().c_str());

			currentdate << "   Live: ";
			if (obs_frontend_streaming_active()) {
				if (tstartimeStream == 0) {
					time(&tstartimeStream);
				}
				uptime = streamTime((int)tstartimeStream);
				hour = (uptime / (60 * 60)) % 60;
				min = (uptime / 60) % 60;
				sec = (uptime % 60);
				currentdate << hour << ":" << twoDigit(min) << ":" << twoDigit(sec);
			}
			else { 
				tstartimeStream = 0;
				currentdate << "-:--:--";
			}

			currentdate << "   Rec: ";
			if (obs_frontend_recording_active()) {
				if (tstartimeRec == 0) {
					time(&tstartimeRec);
				}
				uptime = streamTime((int)tstartimeRec);
				hour = (uptime / (60 * 60)) % 60;
				min = (uptime / 60) % 60;
				sec = (uptime % 60);
				currentdate << hour << ":" << twoDigit(min) << ":" << twoDigit(sec);
			}
			else { 
				tstartimeRec = 0; 
				currentdate << "-:--:--";
			}

			sprintf(stmp, "%s", currentdate.str().c_str());
			pointF = Gdiplus::PointF(2.0f, 222.0f);
			g->DrawString(GetWC(stmp).c_str(), -1, &txt, pointF, &coltxt);

			/*  convertion to buffer  */

			status = clone->GetHBITMAP(Gdiplus::Color(0, 0, 0), &hBitmap);
			if (status != Gdiplus::Ok) {
				char str[256];
				sprintf_s(str, "errcode =  %d \n", GetLastError());

				OutputDebugStringA(str);
				goto error_continue;
			}

			ret = GetDIBits(hdc, hBitmap, 0,
				-bitmapInfo.bmiHeader.biHeight, // height here needs to be positive. Since we made it negative previously, let's reverse it again.
				&byteBitmap,
				(BITMAPINFO *)&bitmapInfo, DIB_RGB_COLORS);


			memcpy(byteBitmap + (320 * 4 * 40), vidBuf, VIDEO_BUFFER_SIZE);

			LogiLcdColorSetBackground(byteBitmap);
		}
		//update screen
		LogiLcdUpdate();

		Sleep(16);
	}
	Gdiplus::GdiplusShutdown(gdiplusToken);
	LogiLcdShutdown();
	return;
}