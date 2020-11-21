#include "execute_buffer_emu.h"
#include "structure_log.h"
#include "../hw/hw_layer.h"
#include "surface_emu.h"
#include <assert.h>

namespace emu {

Direct3DExecuteBufferEmu::Direct3DExecuteBufferEmu(HWLayer &the_hw_layer, const size_t buffer_size)
    : hw_layer(the_hw_layer)
    , size(buffer_size)
    , memory(NULL)
{
    LOG_METHOD();
    memory = malloc(size);
    memset(&execute_data, 0, sizeof(execute_data));
}

Direct3DExecuteBufferEmu::~Direct3DExecuteBufferEmu()
{
    LOG_METHOD();
    if (memory) {
        free(memory);
    }
}

void Direct3DExecuteBufferEmu::execute(DirectDrawSurfaceEmu &device, const LPDIRECT3DVIEWPORT viewport,const DWORD flags)
{
    assert(viewport);

    // The viewport is currently ignored as the game provides the coordinates
    // if the final format.

    REFERENCE(viewport);

    if (flags & D3DEXECUTE_CLIPPED) {
        logKA(MSG_ULTRA_VERBOSE, 1, "Clipped");
    }
    else {
        logKA(MSG_ULTRA_VERBOSE, 1, "Unclipped");
    }

    log_structure(MSG_ULTRA_VERBOSE, 2, execute_data);

    // Notify the device about total number of vertices.

    device.begin_geometry(execute_data.dwVertexCount);

    assert((execute_data.dwInstructionOffset + execute_data.dwInstructionLength) <= size);

    size_t offset = execute_data.dwInstructionOffset;
    size_t length = execute_data.dwInstructionLength;

    while (length > 0) {

        // Fetch the instruction header.

        assert(length >= sizeof(D3DINSTRUCTION));
        const D3DINSTRUCTION instruction = *reinterpret_cast<const D3DINSTRUCTION * >(static_cast<char *>(memory) + offset);
        offset += sizeof(D3DINSTRUCTION);
        length -= sizeof(D3DINSTRUCTION);

        // Check that the data block it describes fits to the buffer.

        const size_t data_length = instruction.bSize * instruction.wCount;
        assert(data_length <= length);

#define OPERATION(name, type) case D3DOP_##name : assert(sizeof(type) == instruction.bSize); if (! execute_block<type>(device, offset, instruction.wCount)) { length = 0; }; break;
#define UNSUPPORTED_OPERATION(name) case D3DOP_##name : logKA(MSG_ERROR, 0, "Unsupported D3DOP_" #name " %u %u", instruction.bSize, instruction.wCount); break;
        switch (instruction.bOpcode) {
            OPERATION(POINT, D3DPOINT);
            OPERATION(LINE, D3DLINE);
            OPERATION(TRIANGLE, D3DTRIANGLE);
            UNSUPPORTED_OPERATION(MATRIXLOAD);
            UNSUPPORTED_OPERATION(MATRIXMULTIPLY);
            UNSUPPORTED_OPERATION(STATETRANSFORM);
            UNSUPPORTED_OPERATION(STATELIGHT);
            OPERATION(STATERENDER, D3DSTATE);
            OPERATION(PROCESSVERTICES, D3DPROCESSVERTICES);
            UNSUPPORTED_OPERATION(TEXTURELOAD);
            UNSUPPORTED_OPERATION(BRANCHFORWARD);
            UNSUPPORTED_OPERATION(SPAN);
            UNSUPPORTED_OPERATION(SETSTATUS);

            // End of the buffer.

            case D3DOP_EXIT: {
                assert(instruction.bSize == 0);
                assert(instruction.wCount == 0);
                length = 0;
                break;
            }
        }
#undef OPERATION
#undef UNSUPPORTED_OPERATION

        // Skip to the next instruction.

        offset += data_length;
        length -= data_length;
    }

    // We are done.

    device.end_geometry();
}

/**
 * @brief Execute block of instructions.
 */
template<typename Type>
bool Direct3DExecuteBufferEmu::execute_block(DirectDrawSurfaceEmu &device, const size_t start_offset, const size_t count)
{
    const Type * const data = reinterpret_cast<const Type *>(static_cast<const char *>(memory) + start_offset);
    for (size_t i = 0; i < count; ++i) {
        if (! execute_operation(device, data[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Executes the D3DOP_STATERENDER operation.
 */
bool Direct3DExecuteBufferEmu::execute_operation(DirectDrawSurfaceEmu &device, const D3DSTATE &data)
{
    logKA(MSG_ULTRA_VERBOSE, 3, "D3DOP_STATERENDER");

    // logKA information about the operation.

    #define OPERATION(name) case D3DRENDERSTATE_##name : logKA(MSG_ULTRA_VERBOSE, 4, "D3DRENDERSTATE_" #name " %u %08X %f", data.dwArg[0], data.dwArg[0], data.dvArg[0]); break;
    switch (data.drstRenderStateType) {
        OPERATION(TEXTUREHANDLE);
        OPERATION(ANTIALIAS);
        OPERATION(TEXTUREADDRESS);
        OPERATION(TEXTUREPERSPECTIVE);
        OPERATION(WRAPU);
        OPERATION(WRAPV);
        OPERATION(ZENABLE);
        OPERATION(FILLMODE);
        OPERATION(SHADEMODE);
        OPERATION(LINEPATTERN);
        OPERATION(MONOENABLE);
        OPERATION(ROP2);
        OPERATION(PLANEMASK);
        OPERATION(ZWRITEENABLE);
        OPERATION(ALPHATESTENABLE);
        OPERATION(LASTPIXEL);
        OPERATION(TEXTUREMAG);
        OPERATION(TEXTUREMIN);
        OPERATION(SRCBLEND);
        OPERATION(DESTBLEND);
        OPERATION(TEXTUREMAPBLEND);
        OPERATION(CULLMODE);
        OPERATION(ZFUNC);
        OPERATION(ALPHAREF);
        OPERATION(ALPHAFUNC);
        OPERATION(DITHERENABLE);
        OPERATION(BLENDENABLE);
        OPERATION(FOGENABLE);
        OPERATION(SPECULARENABLE);
        OPERATION(ZVISIBLE);
        OPERATION(SUBPIXEL);
        OPERATION(SUBPIXELX);
        OPERATION(STIPPLEDALPHA);
        OPERATION(FOGCOLOR);
        OPERATION(FOGTABLEMODE);
        OPERATION(FOGTABLESTART);
        OPERATION(FOGTABLEEND);
        OPERATION(FOGTABLEDENSITY);
        OPERATION(STIPPLEENABLE);
        OPERATION(STIPPLEPATTERN00);
        OPERATION(STIPPLEPATTERN31);
        default: {
            logKA(MSG_ULTRA_VERBOSE, 4, "Unknown render state %u", data.drstRenderStateType);
        }
    }
    #undef OPERATION

    // Apply it.

    device.set_render_state(data);
    return true;
}

/**
 * @brief Executes the D3DOP_POINT operation.
 */
bool Direct3DExecuteBufferEmu::execute_operation(DirectDrawSurfaceEmu &device, const D3DPOINT &data)
{
    logKA(MSG_ULTRA_VERBOSE, 4, "wFirst: %u", data.wFirst);
    logKA(MSG_ULTRA_VERBOSE, 4, "wCount: %u", data.wCount);

    device.add_points(data.wFirst, data.wCount);
    return true;
}

/**
 * @brief Executes the D3DOP_LINE operation.
 */
bool Direct3DExecuteBufferEmu::execute_operation(DirectDrawSurfaceEmu &device, const D3DLINE &data)
{
    logKA(MSG_ULTRA_VERBOSE, 4, "v1: %u", data.v1);
    logKA(MSG_ULTRA_VERBOSE, 4, "v2: %u", data.v2);

    device.add_line(data.v1, data.v2);
    return true;
}

/**
 * @brief Executes the D3DOP_TRIANGLE operation.
 */
bool Direct3DExecuteBufferEmu::execute_operation(DirectDrawSurfaceEmu &device, const D3DTRIANGLE &data)
{
    // Print parameters.

    logKA(MSG_ULTRA_VERBOSE, 3, "D3DOP_TRIANGLE");
    if (data.wFlags & D3DTRIFLAG_EDGEENABLE1) {
        logKA(MSG_ULTRA_VERBOSE, 4, "EDGEENABLE1");
    }
    if (data.wFlags & D3DTRIFLAG_EDGEENABLE2) {
        logKA(MSG_ULTRA_VERBOSE, 4, "EDGEENABLE1");
    }
    if (data.wFlags & D3DTRIFLAG_EDGEENABLE2) {
        logKA(MSG_ULTRA_VERBOSE, 4, "EDGEENABLE1");
    }
    switch (data.wFlags & 31) {
        case D3DTRIFLAG_EVEN: logKA(MSG_ULTRA_VERBOSE, 4, "EVEN"); break;
        case D3DTRIFLAG_ODD: logKA(MSG_ULTRA_VERBOSE, 4, "ODD"); break;
        case D3DTRIFLAG_START: logKA(MSG_ULTRA_VERBOSE, 4, "START"); break;
        default: logKA(MSG_ULTRA_VERBOSE, 4, "STARTLEN(%u)", data.wFlags & 31); break;
    }
    logKA(MSG_ULTRA_VERBOSE, 4, "v1: %u", data.v1);
    logKA(MSG_ULTRA_VERBOSE, 4, "v2: %u", data.v2);
    logKA(MSG_ULTRA_VERBOSE, 4, "v3: %u", data.v3);

    // Queue it.

    device.add_triangle(data.v1, data.v2, data.v3);
    return true;
}

/**
 * @brief Executes the D3DOP_PROCESSVERTICES operation.
 */
bool Direct3DExecuteBufferEmu::execute_operation(DirectDrawSurfaceEmu &device, const D3DPROCESSVERTICES &data)
{
    // Check that this is a supported operation.

    logKA(MSG_ULTRA_VERBOSE, 3, "D3DOP_PROCESSVERTICES");
    const size_t op = data.dwFlags & D3DPROCESSVERTICES_OPMASK;
    switch (op) {
        case D3DPROCESSVERTICES_TRANSFORMLIGHT: {
            logKA(MSG_ERROR, 0, "TRANSFORMLIGHT vertex processing operation is not supported");
            return false;
        }
        case D3DPROCESSVERTICES_TRANSFORM: {
            logKA(MSG_ERROR, 0, "TRANSFORM vertex processing operation is not supported");
            return false;
        }
        case D3DPROCESSVERTICES_COPY: {
            logKA(MSG_ULTRA_VERBOSE, 4, "COPY");
            // The only supported mode.
            break;
        }
        default: {
            logKA(MSG_ERROR, 0, "Unsupported vertex processing operation %u", op);
            return false;
        }
    }

    // Check that only supported combination of flags is set.

    if (data.dwFlags & D3DPROCESSVERTICES_NOCOLOR) {
        logKA(MSG_ERROR, 0, "NOCOLOR vertex processing flag is not supported");
        return false;
    }

    if (data.dwFlags & D3DPROCESSVERTICES_UPDATEEXTENTS) {
        logKA(MSG_ERROR, 0, "UPDATEEXTENTS vertex processing flag is not supported");
        return false;
    }

    // logKA parameters of the operation.

    logKA(MSG_ULTRA_VERBOSE, 4, "wStart: %u", data.wStart);
    logKA(MSG_ULTRA_VERBOSE, 4, "wDest: %u", data.wDest);
    logKA(MSG_ULTRA_VERBOSE, 4, "dwCount: %u", data.dwCount);
    logKA(MSG_ULTRA_VERBOSE, 4, "dwReserved: %u", data.dwReserved);

    // Currently only vertices added at start of the buffer are supported.

    if (data.wStart != 0) {
        logKA(MSG_ERROR, 0, "Unsupported value of wStart");
        return false;
    }
    if (data.wDest != 0) {
        logKA(MSG_ERROR, 0, "Unsupported value of wDest");
        return false;
    }

    // Set the vertices.

    const size_t start_offset =
        execute_data.dwVertexOffset +
        (data.wStart * sizeof(D3DTLVERTEX))
    ;

    return device.set_vertices(
        data.wDest,
        reinterpret_cast<const D3DTLVERTEX *>(static_cast<const char *>(memory) + start_offset),
        data.dwCount
    );
}

/**
 * @brief Locks the buffer.
 */
STDMETHODIMP Direct3DExecuteBufferEmu::Lock(LPD3DEXECUTEBUFFERDESC desc)
{
    LOG_METHOD();
    CHECK_STRUCTURE(desc, D3DEXECUTEBUFFERDESC);
    log_structure(MSG_ULTRA_VERBOSE, 1, *desc);

    desc->dwFlags |= D3DDEB_CAPS | D3DDEB_BUFSIZE | D3DDEB_LPDATA;
    desc->dwCaps = D3DDEBCAPS_VIDEOMEMORY;
    desc->dwBufferSize = size;
    desc->lpData = memory;
    return DD_OK;
}

/**
 * @brief Unlocks the buffer.
 */
STDMETHODIMP Direct3DExecuteBufferEmu::Unlock()
{
    LOG_METHOD();
    return DD_OK;
}

/**
 * @brief Sets new execute data.
 */
STDMETHODIMP Direct3DExecuteBufferEmu::SetExecuteData(LPD3DEXECUTEDATA data)
{
    LOG_METHOD();
    CHECK_STRUCTURE(data, D3DEXECUTEDATA);
    log_structure(MSG_VERBOSE, 1, *data);

    execute_data = *data;
    return DD_OK;
}

/**
 * @brief Returns associated execute data.
 */
STDMETHODIMP Direct3DExecuteBufferEmu::GetExecuteData(LPD3DEXECUTEDATA data)
{
    LOG_METHOD();
    CHECK_STRUCTURE(data, D3DEXECUTEDATA);

    *data = execute_data;
    return DD_OK;
}

} // namespace emu

// EOF //
