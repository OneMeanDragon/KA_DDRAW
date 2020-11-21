#ifndef EXECUTE_BUFFER_EMU_H
#define EXECUTE_BUFFER_EMU_H

#include "../helpers/interface.h"
#include "../helpers/log.h"
#include "ddraw_emu.h"
#include "ddraw.h"
#include "d3d.h"
#include <deque>

namespace emu {

class DirectDrawSurfaceEmu;

class Direct3DExecuteBufferEmu : public IUnknownImpl, public IDirect3DExecuteBuffer {

private:

    HWLayer &hw_layer;

private:

    /**
     * @brief Size of this buffer.
     */
    size_t size;

    /**
     * @brief Memory of the buffer.
     */
    void * memory;

    /**
     * @brief Currently active execute data.
     */
    D3DEXECUTEDATA execute_data;

public:

    Direct3DExecuteBufferEmu(HWLayer &the_hw_layer, const size_t buffer_size);
    virtual ~Direct3DExecuteBufferEmu();

    void execute(DirectDrawSurfaceEmu &device, const LPDIRECT3DVIEWPORT viewport,const DWORD flags);

    template<typename Type>
    bool execute_block(DirectDrawSurfaceEmu &device, const size_t start_offset, const size_t count);

    bool execute_operation(DirectDrawSurfaceEmu &device, const D3DSTATE &data);
    bool execute_operation(DirectDrawSurfaceEmu &device, const D3DPOINT &data);
    bool execute_operation(DirectDrawSurfaceEmu &device, const D3DLINE &data);
    bool execute_operation(DirectDrawSurfaceEmu &device, const D3DTRIANGLE &data);
    bool execute_operation(DirectDrawSurfaceEmu &device, const D3DPROCESSVERTICES &data);

    // IUnknown.

    IUNKNOWN_IMPLEMENTATION()
    GET_IMPLEMENTATION_BEGIN()
    GET_IMPLEMENTATION_IFACE(IID_IDirect3DExecuteBuffer, IDirect3DExecuteBuffer)
    GET_IMPLEMENTATION_END()

    // IDirect3DExecuteBuffer methods

    STDMETHODIMP Initialize(LPDIRECT3DDEVICE,LPD3DEXECUTEBUFFERDESC) UNIMPLEMENTED;
    STDMETHODIMP Lock(LPD3DEXECUTEBUFFERDESC);
    STDMETHODIMP Unlock();
    STDMETHODIMP SetExecuteData(LPD3DEXECUTEDATA);
    STDMETHODIMP GetExecuteData(LPD3DEXECUTEDATA);
    STDMETHODIMP Validate(LPDWORD,LPD3DVALIDATECALLBACK,LPVOID,DWORD) UNIMPLEMENTED;
    STDMETHODIMP Optimize(DWORD) UNIMPLEMENTED;
};

} // namespace emu

#endif // EXECUTE_BUFFER_EMU_H

// EOF //
