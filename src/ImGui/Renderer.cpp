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
	typedef struct
	{
		ID3D11Device*               pd3dDevice;
		ID3D11DeviceContext*        pd3dDeviceContext;
		IDXGIFactory*               pFactory;
		ID3D11Buffer*               pVB;
		ID3D11Buffer*               pIB;
		ID3D11VertexShader*         pVertexShader;
		ID3D11InputLayout*          pInputLayout;
		ID3D11Buffer*               pVertexConstantBuffer;
		ID3D11PixelShader*          pPixelShader;
		ID3D11SamplerState*         pTexSamplerLinear;
		ID3D11RasterizerState*      pRasterizerState;
		ID3D11BlendState*           pBlendState;
		ID3D11DepthStencilState*    pDepthStencilState;
		int                         VertexBufferSize;
		int                         IndexBufferSize;
	} ImGui_ImplDX11_Data;
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
			ImGui_ImplDX11_Data* bd = ImGui::GetCurrentContext() ? ((ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData) : nullptr;
		    if (bd && bd->pd3dDevice) 
			{
				D3D11_RASTERIZER_DESC desc;
				ZeroMemory(&desc, sizeof(desc));
				desc.FillMode = D3D11_FILL_SOLID;
				desc.CullMode = D3D11_CULL_NONE;
				desc.ScissorEnable = false;
				desc.DepthClipEnable = false;
				if (bd->pRasterizerState)
				{
					bd->pRasterizerState->Release();
					bd->pRasterizerState = nullptr;
				}				
				bd->pd3dDevice->CreateRasterizerState(&desc, &bd->pRasterizerState);
			}
			{
				// disable windowing
				GImGui->NavWindowingTarget = nullptr;

				Manager::GetSingleton()->Draw();
			}
			ImGui::EndFrame();
			ImGui::Render();

			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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
