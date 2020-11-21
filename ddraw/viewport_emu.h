#ifndef VIEWPORT_EMU_H
#define VIEWPORT_EMU_H

#include "../helpers/interface.h"
#include "../helpers/log.h"
#include "ddraw_emu.h"
#include "ddraw.h"
#include "d3d.h"
#include <deque>

namespace emu {

class Direct3DMaterialEmu;

class Direct3DViewportEmu : public IUnknownImpl, public IDirect3DViewport {

private:

    HWLayer &hw_layer;

private:

    /**
     * @brief Viewport parameters.
     */
    D3DVIEWPORT viewport;

    /**
     * @brief Material to use as background.
     */
    Direct3DMaterialEmu * background_material;

public:

    Direct3DViewportEmu(HWLayer &the_hw_layer);
    virtual ~Direct3DViewportEmu();

    // IUnknown.

    IUNKNOWN_IMPLEMENTATION()
    GET_IMPLEMENTATION_BEGIN()
    GET_IMPLEMENTATION_IFACE(IID_IDirect3DViewport, IDirect3DViewport)
    GET_IMPLEMENTATION_END()

    // IDirect3DViewport

    STDMETHODIMP Initialize(LPDIRECT3D) UNSUPPORTED;
    STDMETHODIMP GetViewport(LPD3DVIEWPORT);
    STDMETHODIMP SetViewport(LPD3DVIEWPORT);
    STDMETHODIMP TransformVertices(DWORD,LPD3DTRANSFORMDATA,DWORD,LPDWORD) UNIMPLEMENTED;
    STDMETHODIMP LightElements(DWORD,LPD3DLIGHTDATA) UNIMPLEMENTED;
    STDMETHODIMP SetBackground(D3DMATERIALHANDLE);
    STDMETHODIMP GetBackground(LPD3DMATERIALHANDLE,LPBOOL);
    STDMETHODIMP SetBackgroundDepth(LPDIRECTDRAWSURFACE) UNIMPLEMENTED;
    STDMETHODIMP GetBackgroundDepth(LPDIRECTDRAWSURFACE*,LPBOOL) UNIMPLEMENTED;
    STDMETHODIMP Clear(DWORD,LPD3DRECT,DWORD);
    STDMETHODIMP AddLight(LPDIRECT3DLIGHT) UNIMPLEMENTED;
    STDMETHODIMP DeleteLight(LPDIRECT3DLIGHT) UNIMPLEMENTED;
    STDMETHODIMP NextLight(LPDIRECT3DLIGHT,LPDIRECT3DLIGHT*,DWORD) UNIMPLEMENTED;

};

} // namespace emu

#endif // VIEWPORT_EMU_H

// EOF //
