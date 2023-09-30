#pragma once
#include <Windows.h>
#include <vector>
#include <d3d11.h>
#include <dwmapi.h>
#include <string>
#include <functional>
#include "includes.hpp"
#include "spoof.hpp"
#include "structs.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "sdk.hpp"
#ifndef OVERLAY_HPP
#define OVERLAY_HPP

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
struct c_overlay
{
	WNDCLASSEXW wc;
	HWND fortnite_window = nullptr;
	HWND Window = nullptr;
	IDXGISwapChain* pSwapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC swap_chain;
	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11DeviceContext = nullptr;
	ID3D11RenderTargetView* MainRenderTargetView = nullptr;
	MARGINS Margin = { NULL };
	MSG Message = { nullptr };

	void CreateRenderTarget();
	void DestroyRenderTarget();
}; c_overlay* overlay;

void c_overlay::CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&pBackBuffer))))
	{
		D3D11Device->CreateRenderTargetView(pBackBuffer, NULL, &MainRenderTargetView);
		pBackBuffer->Release();
	}
}

void c_overlay::DestroyRenderTarget()
{
	if (MainRenderTargetView) { MainRenderTargetView->Release(); MainRenderTargetView = nullptr; }
	if (pSwapChain) { pSwapChain->Release(); pSwapChain = nullptr; }
	if (D3D11DeviceContext) { D3D11DeviceContext->Release(); D3D11DeviceContext = nullptr; }
	if (D3D11Device) { D3D11Device->Release(); D3D11Device = nullptr; }
}

__forceinline BOOL directx_init(c_overlay* overlay)
{
	ZeroMemory(&overlay->swap_chain, sizeof(overlay->swap_chain));
	overlay->swap_chain.Windowed = TRUE;
	overlay->swap_chain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	overlay->swap_chain.BufferCount = 2;
	overlay->swap_chain.BufferDesc.Width = 0;
	overlay->swap_chain.BufferDesc.Height = 0;
	overlay->swap_chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	overlay->swap_chain.OutputWindow = overlay->Window;
	overlay->swap_chain.SampleDesc.Count = 1;
	overlay->swap_chain.SampleDesc.Quality = DEFAULT_QUALITY;
	overlay->swap_chain.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	overlay->swap_chain.BufferDesc.RefreshRate.Numerator = 240;
	overlay->swap_chain.BufferDesc.RefreshRate.Denominator = 1;

	// Setup swap chain
	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT resolution = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &overlay->swap_chain, &overlay->pSwapChain, &overlay->D3D11Device, &featureLevel, &overlay->D3D11DeviceContext);
	if (resolution == DXGI_ERROR_UNSUPPORTED) {
		{
			std::cout << xorstr(" Warning: Try high-performance WARP software driver if hardware is not available. ") << std::endl;
			Sleep(1000);
		}
		resolution = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &overlay->swap_chain, &overlay->pSwapChain, &overlay->D3D11Device, &featureLevel, &overlay->D3D11DeviceContext);
	}
	if (resolution != S_OK)
		return FALSE;

	overlay->CreateRenderTarget();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImWchar icons_ranges[] = { 0xf000, 0xf5ff, 0 };
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.OversampleH = 1;
	icons_config.OversampleV = 1;
	ImFontConfig CustomFont;
	CustomFont.FontDataOwnedByAtlas = false;

	ESPFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 15.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
	bold_segu = io.Fonts->AddFontFromMemoryTTF(&bold_segue, sizeof bold_segue, 30, NULL, io.Fonts->GetGlyphRangesCyrillic());
	io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(FontCustom), sizeof(FontCustom), 14.0f, &CustomFont);
	io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 13.0f, &icons_config, icons_ranges);
	titleicon = io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 25.0f, &icons_config, icons_ranges);
	subtitle = io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(FontCustom), sizeof(FontCustom), 13.0f, &CustomFont);

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(overlay->Window);
	ImGui_ImplDX11_Init(overlay->D3D11Device, overlay->D3D11DeviceContext);
	overlay->D3D11Device->Release();

	return TRUE;
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (overlay->D3D11Device != nullptr && wParam != SIZE_MINIMIZED)
		{
			overlay->MainRenderTargetView->Release();
			overlay->MainRenderTargetView = nullptr;
			overlay->pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			overlay->CreateRenderTarget();
		}
		return 0;

	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;

	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;

	case WM_DPICHANGED:
		if (ImGui::GetIO().ConfigFlags)
		{
			const RECT* suggested_rect = (RECT*)lParam;
			::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		break;

	default:
		break;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

BOOL create_window(c_overlay* overlay)
{
	RtlZeroMemory(&overlay->wc, sizeof(overlay->wc));
	overlay->wc.cbSize = sizeof(overlay->wc);
	overlay->wc.lpszClassName = (L"Overlay");
	overlay->wc.lpfnWndProc = WinProc;
	RegisterClassExW(&overlay->wc);

	RECT GameRect = { NULL };
	GetClientRect(overlay->fortnite_window, &GameRect);
	overlay->Window = CreateWindowExA(NULL, "Overlay", "Overlay Util", WS_POPUP | WS_VISIBLE, 0, 0, GameRect.right, GameRect.bottom, 0, 0, 0, 0);
	if (!overlay->Window)
		return FALSE;

	DwmExtendFrameIntoClientArea(overlay->Window, &overlay->Margin);
	ShowWindow(overlay->Window, SW_SHOW);
	UpdateWindow(overlay->Window);
	SetWindowPos(overlay->Window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetWindowLongA(overlay->Window, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_COMPOSITED);
	return TRUE;
}

__forceinline void filled_box(float left, float bottom, float width, float height, ImColor color, float rounding) {
	ImDrawList* p = ImGui::GetBackgroundDrawList();
	p->AddRectFilled(ImVec2(left, bottom), ImVec2(left + width, bottom + height), color, rounding, ImDrawFlags_RoundCornersNone);
}

__forceinline void box_esp(float offset, float top, float bottom, float right, float left, ImU32 color, float thickness, bool outlined) {
	ImDrawList* p = ImGui::GetBackgroundDrawList();
	if (outlined) {
		auto outline_color = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f / 255.0f, 1.f / 255.0f, 1.f / 255.0f, 255.f / 255.0f));
		p->AddLine(ImVec2(left - offset, top - offset), ImVec2(right + offset, top - offset), outline_color, thickness + 1.f); //top line
		p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(right + offset, bottom + offset), outline_color, thickness + 1.f); //bottom line
		p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(left - offset, top - offset), outline_color, thickness + 1.f); //left line
		p->AddLine(ImVec2(right + offset, top - offset), ImVec2(right + offset, bottom + offset), outline_color, thickness + 1.f); //right line
	}

	p->AddLine(ImVec2(left - offset, top - offset), ImVec2(right + offset, top - offset), color, thickness); //top line

	p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(right + offset, bottom + offset), color, thickness); //bottom line

	p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(left - offset, top - offset), color, thickness); //left line

	p->AddLine(ImVec2(right + offset, top - offset), ImVec2(right + offset, bottom + offset), color, thickness); //right line
}

__forceinline void corner_esp(float offset, float top, float bottom, float right, float left, float corner_width, float corner_height, ImU32 color, float thickness, bool outlined) {
	ImDrawList* p = ImGui::GetBackgroundDrawList();
	if (outlined) {
		auto outline_color = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f / 255.0f, 1.f / 255.0f, 1.f / 255.0f, 255.f / 255.0f));
		p->AddLine(ImVec2(left - offset, top - offset), ImVec2(left - offset + corner_width, top - offset), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(left - offset, top - offset), ImVec2(left - offset, top - offset + corner_height), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(right + offset, top - offset), ImVec2(right + offset - corner_width, top - offset), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(right + offset, top - offset), ImVec2(right + offset, top - offset + corner_height), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(left - offset + corner_width, bottom + offset), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(left - offset, bottom + offset - corner_height), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(right + offset, bottom + offset), ImVec2(right + offset - corner_width, bottom + offset), outline_color, thickness + 1.f);
		p->AddLine(ImVec2(right + offset, bottom + offset), ImVec2(right + offset, bottom + offset - corner_height), outline_color, thickness + 1.f);
	}

	//top left
	p->AddLine(ImVec2(left - offset, top - offset), ImVec2(left - offset + corner_width, top - offset), color, thickness);
	p->AddLine(ImVec2(left - offset, top - offset), ImVec2(left - offset, top - offset + corner_height), color, thickness);

	//top right
	p->AddLine(ImVec2(right + offset, top - offset), ImVec2(right + offset - corner_width, top - offset), color, thickness);
	p->AddLine(ImVec2(right + offset, top - offset), ImVec2(right + offset, top - offset + corner_height), color, thickness);

	//bottom left
	p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(left - offset + corner_width, bottom + offset), color, thickness);
	p->AddLine(ImVec2(left - offset, bottom + offset), ImVec2(left - offset, bottom + offset - corner_height), color, thickness);

	//bottom right
	p->AddLine(ImVec2(right + offset, bottom + offset), ImVec2(right + offset - corner_width, bottom + offset), color, thickness);
	p->AddLine(ImVec2(right + offset, bottom + offset), ImVec2(right + offset, bottom + offset - corner_height), color, thickness);
}

__forceinline void threed_box(float offset, float bottom_x, float bottom_y, float top_x, float top_y, ImColor color, float thickness) {
	ImDrawList* p = ImGui::GetBackgroundDrawList();
	FVector2D bottom1 = FVector2D(bottom_x + offset, bottom_y - offset);
	FVector2D bottom2 = FVector2D(bottom_x - offset, bottom_y - offset);
	FVector2D bottom3 = FVector2D(bottom_x - offset, bottom_y + offset);
	FVector2D bottom4 = FVector2D(bottom_x + offset, bottom_y + offset);

	FVector2D top1 = FVector2D(top_x + offset, top_y - offset);
	FVector2D top2 = FVector2D(top_x - offset, top_y - offset);
	FVector2D top3 = FVector2D(top_x - offset, top_y + offset);
	FVector2D top4 = FVector2D(top_x + offset, top_y + offset);

	p->AddLine(ImVec2(bottom1.x, bottom1.y), ImVec2(top1.x, top1.y), color, thickness);
	p->AddLine(ImVec2(bottom2.x, bottom2.y), ImVec2(top2.x, top2.y), color, thickness);
	p->AddLine(ImVec2(bottom3.x, bottom3.y), ImVec2(top3.x, top3.y), color, thickness);
	p->AddLine(ImVec2(bottom4.x, bottom4.y), ImVec2(top4.x, top4.y), color, thickness);

	p->AddLine(ImVec2(bottom1.x, bottom1.y), ImVec2(bottom2.x, bottom2.y), color, thickness);
	p->AddLine(ImVec2(bottom2.x, bottom2.y), ImVec2(bottom3.x, bottom3.y), color, thickness);
	p->AddLine(ImVec2(bottom3.x, bottom3.y), ImVec2(bottom4.x, bottom4.y), color, thickness);
	p->AddLine(ImVec2(bottom4.x, bottom4.y), ImVec2(bottom1.x, bottom1.y), color, thickness);

	p->AddLine(ImVec2(top1.x, top1.y), ImVec2(top2.x, top2.y), color, thickness);
	p->AddLine(ImVec2(top2.x, top2.y), ImVec2(top3.x, top3.y), color, thickness);
	p->AddLine(ImVec2(top3.x, top3.y), ImVec2(top4.x, top4.y), color, thickness);
	p->AddLine(ImVec2(top4.x, top4.y), ImVec2(top1.x, top1.y), color, thickness);
}

__forceinline void _DrawText(ImFont* Font, ImVec2 pos, ImU32 color, const char* text, float size = 18.0f, bool outline = false)
{
	ImDrawList* p = ImGui::GetBackgroundDrawList();
	if (outline) {
		ImVec2 text_size = Font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
		ImU32 outline_color = ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0));
		p->AddText(Font, size, ImVec2((pos.x) + 1, (pos.y + text_size.y) + 1), outline_color, text);
		p->AddText(Font, size, ImVec2((pos.x) - 1, (pos.y + text_size.y) - 1), outline_color, text);
		p->AddText(Font, size, ImVec2((pos.x) + 1, (pos.y + text_size.y) - 1), outline_color, text);
		p->AddText(Font, size, ImVec2((pos.x) - 1, (pos.y + text_size.y) + 1), outline_color, text);
	}

	p->AddText(Font, size, pos, color, text);
}
#endif // OVERLAY_HPP