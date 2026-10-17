#include "registers/gaussian_blur_registers.slang"

#include "common/math/common.slang"


[shader("compute")]
[numthreads(GAUSSIAN_BLUR_CS_GROUP_SIZE_X, GAUSSIAN_BLUR_CS_GROUP_SIZE_Y, 1)]
void main(
    SV_GROUP_ID(groupID),
    SV_GROUP_THREAD_FLAT_ID(groupThreadFlatID),
    SV_DISPATCH_THREAD_ID(dispatchThreadID)
) {
    
}