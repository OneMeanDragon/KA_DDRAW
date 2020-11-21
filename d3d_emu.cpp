#include "d3d_emu.h"
#include "hw/dx9/dx9_hw_layer.h"

namespace emu {

HWLayer *create_hw_layer(void)
{
    return new DX9HWLayer();
}

} // namespace emu

// EOF //
