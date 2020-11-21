#ifndef MATERIAL_EMU_H
#define MATERIAL_EMU_H

#include "../helpers/interface.h"
#include "../helpers/log.h"
#include "ddraw_emu.h"
#include "ddraw.h"
#include "d3d.h"
#include <deque>

namespace emu {

class Direct3DMaterialEmu : public IUnknownImpl, public IDirect3DMaterial {

private:

    /**
     * @brief Parameters of this material.
     */
    D3DMATERIAL material;

    /**
     * @brief Extracted parameters.
     */
    //@{
    DWORD diffuse_color;
    //@}

public:

    Direct3DMaterialEmu();
    virtual ~Direct3DMaterialEmu();

    // Retrieval of material parameters for emulation purposes.

    DWORD get_diffuse_color(void) const;

    // IUnknown.

    IUNKNOWN_IMPLEMENTATION()
    GET_IMPLEMENTATION_BEGIN()
    GET_IMPLEMENTATION_IFACE(IID_IDirect3DMaterial, IDirect3DMaterial)
    GET_IMPLEMENTATION_END()

    // IDirect3DMaterial methods

    STDMETHODIMP Initialize(LPDIRECT3D) UNSUPPORTED;
    STDMETHODIMP SetMaterial(LPD3DMATERIAL);
    STDMETHODIMP GetMaterial(LPD3DMATERIAL);
    STDMETHODIMP GetHandle(LPDIRECT3DDEVICE,LPD3DMATERIALHANDLE);
    STDMETHODIMP Reserve() UNIMPLEMENTED;
    STDMETHODIMP Unreserve() UNIMPLEMENTED;

};

} // namespace emu

#endif // VIEWPORT_EMU_H

// EOF //
