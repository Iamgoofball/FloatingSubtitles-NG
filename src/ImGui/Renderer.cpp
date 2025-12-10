#include "Renderer.h"

#include "ImGui/FontStyles.h"
#include "Manager.h"
#include <windows.h>
#include <d3d11.h>

namespace ImGui::Renderer
{
	struct CreateD3DAndSwapChain
	{
		static void thunk()
		{
			func();

			if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
				const auto swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows->swapChain);
				if (!swapChain) {
					logger::error("couldn't find swapChain");
					return;
				}

				DXGI_SWAP_CHAIN_DESC desc{};
				if (FAILED(swapChain->GetDesc(std::addressof(desc)))) {
					logger::error("IDXGISwapChain::GetDesc failed.");
					return;
				}

				const auto device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);
				const auto context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);

				logger::info("Initializing ImGui..."sv);

				ImGui::CreateContext();

				auto& io = ImGui::GetIO();
				io.IniFilename = nullptr;

				if (!ImGui_ImplWin32_Init(desc.OutputWindow)) {
					logger::error("ImGui initialization failed (Win32)");
					return;
				}
				if (!ImGui_ImplDX11_Init(device, context)) {
					logger::error("ImGui initialization failed (DX11)"sv);
					return;
				}

				ImGui::FontStyles::GetSingleton()->LoadFontStyles();

				logger::info("ImGui initialized.");

				initialized.store(true);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// IMenu::PostDisplay
	struct PostDisplay
	{
		static void thunk(RE::IMenu* a_menu)
		{
			// Skip if Imgui is not loaded
			if (!initialized.load() || Manager::GetSingleton()->SkipRender()) {
				func(a_menu);
				return;
			}

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			{
				//trick imgui into rendering at game's real resolution (ie. if upscaled with Display Tweaks)
				static const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();

				auto& io = ImGui::GetIO();
				io.DisplaySize.x = static_cast<float>(screenSize.width);
				io.DisplaySize.y = static_cast<float>(screenSize.height);

			}
			ImGui::NewFrame();
			{
				// disable windowing
				GImGui->NavWindowingTarget = nullptr;

				Manager::GetSingleton()->Draw();
			}
			ImGui::EndFrame();
			ImGui::Render();
			
			ID3D11DepthStencilState * originalState;
			unsigned int stencilRef;
			D3D11_DEPTH_STENCIL_DESC alwaysOnTopStencil;
			memset(&alwaysOnTopStencil,0, sizeof(alwaysOnTopStencil));			
			if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
					auto device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);
					auto context = reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context);
					context->OMGetDepthStencilState(&originalState,&stencilRef);
					alwaysOnTopStencil.DepthEnable = FALSE;
					alwaysOnTopStencil.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
					alwaysOnTopStencil.DepthFunc = D3D11_COMPARISON_LESS;
					alwaysOnTopStencil.StencilEnable = FALSE;
					ID3D11DepthStencilState* pDepthState;
					device->CreateDepthStencilState(&alwaysOnTopStencil, &pDepthState);
					context->OMSetDepthStencilState(pDepthState,stencilRef);
			}
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			context->OMSetDepthStencilState(originalState,stencilRef);

			func(a_menu);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::size_t                      idx{ 0x6 };
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(75595, 77226), OFFSET(0x9, 0x275) };  // BSGraphics::InitD3D
		stl::write_thunk_call<CreateD3DAndSwapChain>(target.address());

		stl::write_vfunc<RE::HUDMenu, PostDisplay>();
	}
}
