#ifndef DDRAW_EMU_H
#define DDRAW_EMU_H

#include "../helpers/interface.h"
#include "../helpers/log.h"
#include "ddraw.h"
#include "d3d.h"
#include "../hw/hw_layer.h"
#include <list>

namespace emu {

class DirectDrawSurfaceEmu;

/**
 * @brief Guid used to identify our emulated device.
 */
const GUID EMULATED_DEVICE_GUID = {0x95DD7321, 0xDDB6, 0x443C, 0xB8, 0x38, 0x73, 0xD8, 0x97, 0xBF, 0xFC, 0xCB};

/**
 * @brief Special value used when the two pass base+glow effect is done.
 */
//@{
const DWORD GLOW_HACK_SHADING_MODE_BASE = 0x12;
const DWORD GLOW_HACK_SHADING_MODE_OVERLAY = 0x11;
//@}

class DirectDrawEmu : public IUnknownImpl, public IDirectDraw, public IDirectDraw2, public IDirect3D {

protected:

    /**
     * @brief HW layer to use.
     */
    HWLayer * const hw_layer;

    /**
     * @brief Handle of instance for this module for use
     * with window-related api.
     */
    const HINSTANCE instance;

protected:

    /**
     * @brief Window to use.
     */
    HWND window;

    /**
     * @brief Mode parameters.
     */
    size_t width, height, bpp;

public:

    DirectDrawEmu(HWLayer * const the_hw_layer, const HINSTANCE the_instance);
    virtual ~DirectDrawEmu();

public:

    static void patch_game(void);
    static void unpatch_game(void);

public:

    // IUnknown.

    IUNKNOWN_IMPLEMENTATION()
    GET_IMPLEMENTATION_BEGIN()
    GET_IMPLEMENTATION_IFACE(IID_IDirectDraw, IDirectDraw)
    GET_IMPLEMENTATION_IFACE(IID_IDirectDraw2, IDirectDraw2)
    GET_IMPLEMENTATION_IFACE(IID_IDirect3D, IDirect3D)
    GET_IMPLEMENTATION_END()

    //  IDirectDraw methods

    STDMETHODIMP Compact() UNSUPPORTED;
    STDMETHODIMP CreateClipper(DWORD, LPDIRECTDRAWCLIPPER FAR*, IUnknown FAR *) UNIMPLEMENTED;
    STDMETHODIMP CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE FAR*, IUnknown FAR *) UNIMPLEMENTED;
    STDMETHODIMP CreateSurface(LPDDSURFACEDESC, LPDIRECTDRAWSURFACE FAR *, IUnknown FAR *);
    STDMETHODIMP DuplicateSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE FAR *) UNIMPLEMENTED;
    STDMETHODIMP EnumDisplayModes(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK);
    STDMETHODIMP EnumSurfaces(DWORD, LPDDSURFACEDESC, LPVOID,LPDDENUMSURFACESCALLBACK) UNIMPLEMENTED;
    STDMETHODIMP FlipToGDISurface() UNIMPLEMENTED;
    STDMETHODIMP GetCaps(DDCAPS *, DDCAPS *);
    STDMETHODIMP GetDisplayMode(LPDDSURFACEDESC) UNIMPLEMENTED;
    STDMETHODIMP GetFourCCCodes(LPDWORD, LPDWORD) UNIMPLEMENTED;
    STDMETHODIMP GetGDISurface(LPDIRECTDRAWSURFACE FAR *) UNIMPLEMENTED;
    STDMETHODIMP GetMonitorFrequency(LPDWORD) UNSUPPORTED;
    STDMETHODIMP GetScanLine(LPDWORD) UNIMPLEMENTED;
    STDMETHODIMP GetVerticalBlankStatus(LPBOOL) UNIMPLEMENTED;
    STDMETHODIMP Initialize(GUID FAR *) UNIMPLEMENTED;
    STDMETHODIMP RestoreDisplayMode();
    STDMETHODIMP SetCooperativeLevel(HWND, DWORD);
    STDMETHODIMP SetDisplayMode(DWORD, DWORD,DWORD);
    STDMETHODIMP WaitForVerticalBlank(DWORD, HANDLE) DUMMY;

    // IDirect3D methods.

    STDMETHODIMP Initialize(REFCLSID) UNSUPPORTED;
    STDMETHODIMP EnumDevices(LPD3DENUMDEVICESCALLBACK,LPVOID);
    STDMETHODIMP CreateLight(LPDIRECT3DLIGHT*,IUnknown*) UNIMPLEMENTED;
    STDMETHODIMP CreateMaterial(LPDIRECT3DMATERIAL*,IUnknown*);
    STDMETHODIMP CreateViewport(LPDIRECT3DVIEWPORT*,IUnknown*);
    STDMETHODIMP FindDevice(LPD3DFINDDEVICESEARCH,LPD3DFINDDEVICERESULT) UNIMPLEMENTED;

    // Fake IDirectDraw2 for version detection by the launcher.

    STDMETHODIMP SetDisplayMode(DWORD, DWORD,DWORD, DWORD, DWORD) UNIMPLEMENTED;
    STDMETHODIMP GetAvailableVidMem(LPDDSCAPS, LPDWORD, LPDWORD) UNIMPLEMENTED;
};

} // namespace emu

#endif // DDRAW_EMU_H

// EOF //
