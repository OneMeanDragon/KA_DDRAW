#include <windows.h>
#include "helpers/log.h"
#include "ddraw/ddraw_emu.h"
#include "ddraw7/ddraw7_emu.h"
#include "hw/hw_layer.h"
#include "d3d_emu.h"
#include "helpers/common.h"

namespace {

/**
 * @brief Handle of instance to use for window api.
 */
HINSTANCE instance = NULL;

/**
 * @brief Name of window class used by our helper window used
 * to deliver timer messages.
 */
const char * const TIMER_WINDOW_CLASS_NAME = "D3DEMUTimerWindowClass";

/**
 * @brief Window function for the timer window.
 */
LRESULT CALLBACK timer_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE) {
        const LPCREATESTRUCT info = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(info->lpCreateParams));
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/**
 * @brief Registers windows class for the timer window.
 */
void register_timer_window_class(const HINSTANCE instance)
{
    WNDCLASSEXA classinfo;
    memset(&classinfo, 0, sizeof(classinfo));
    classinfo.cbSize = sizeof(classinfo);
    classinfo.style = 0;
    classinfo.lpfnWndProc = timer_window_proc;
    classinfo.hInstance = instance;
    classinfo.lpszClassName = TIMER_WINDOW_CLASS_NAME;
    RegisterClassExA(&classinfo);
}

} // anonymous namespace

HRESULT WINAPI DirectDrawCreateEmu(
  GUID FAR* UNUSED_PARAMETER(guid),
  LPDIRECTDRAW FAR* object,
  IUnknown FAR* outer
)
{
    LOG_FUNCTION_NAME();
    CHECK_NOT_NULL(object);
    CHECK_NULL(outer);

    *object = new emu::DirectDrawEmu(emu::create_hw_layer(), instance);
    return DD_OK;
}

HRESULT WINAPI DirectDrawCreateExEmu(
  GUID FAR* UNUSED_PARAMETER(guid),
  LPDIRECTDRAW7 FAR* object,
  REFIID iid,
  IUnknown FAR* outer
)
{
    LOG_FUNCTION_NAME();
    CHECK_NOT_NULL(object);
    CHECK_NULL(outer);

    if (iid != IID_IDirectDraw7) {
        return DDERR_INVALIDPARAMS;
    }

    LOG_FUNCTION_NAME();
    *object = new emu::DirectDraw7Emu();
    return DD_OK;
}

HRESULT WINAPI DirectDrawEnumerateExAEmu(
  LPDDENUMCALLBACKEXA callback,
  LPVOID context,
  DWORD UNUSED_PARAMETER(flags)
)
{
    LOG_FUNCTION_NAME();
    CHECK_NOT_NULL(callback);

    GUID guid = emu::EMULATED_DEVICE_GUID;
    callback(&guid, "Emulated device", "Emulated", context, NULL);
    return DD_OK;
}

HRESULT WINAPI DirectDrawCreateClipperEmu(
    DWORD UNUSED_PARAMETER(flags),
    LPDIRECTDRAWCLIPPER FAR *object,
    IUnknown FAR *outer
)
{
    LOG_FUNCTION_NAME();
    CHECK_NOT_NULL(object);
    CHECK_NULL(outer);

    return DDERR_OUTOFMEMORY;
}

HRESULT WINAPI DirectDrawEnumerateEmuA(
  LPDDENUMCALLBACKA UNUSED_PARAMETER(callback),
  LPVOID UNUSED_PARAMETER(context)
)
{
    LOG_FUNCTION_NAME();
    return DD_OK;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID UNUSED_PARAMETER(lpReserved)
					 )
{
	switch (ul_reason_for_call)
	{
        case DLL_PROCESS_ATTACH: {
            emu::log_init("d3demu.logKA");
            emu::DirectDrawEmu::patch_game();

            instance = hModule;
            register_timer_window_class(hModule);
            break;
        }
        case DLL_PROCESS_DETACH: {
            emu::DirectDrawEmu::unpatch_game();
            emu::log_close();
            instance = NULL;
            break;
        }
	    case DLL_THREAD_ATTACH:
	    case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

