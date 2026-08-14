#ifndef COMMON_REGISTERS_H
#define COMMON_REGISTERS_H

#include "common/system/sys_constants.slang"

#include "common/math/constants.slang"
#include "common/math/common.slang"

#include "common/common_dbg.slang"
#include "common/common_structs.slang"
#include "common/common_geom.slang"


[vk::binding(0, DESC_SET_PER_FRAME)] SamplerState           COMMON_SAMPLERS[GPU_CommonSamplerID::COUNT];
[vk::binding(1, DESC_SET_PER_FRAME)] SamplerComparisonState COMMON_CMP_SAMPLERS[GPU_CommonCmpSamplerID::COUNT];

// 2 - free

[vk::binding(3, DESC_SET_PER_FRAME)] ConstantBuffer<GPU_CommonCBData>    COMMON_CB;
[vk::binding(4, DESC_SET_PER_FRAME)] ConstantBuffer<GPU_CommonDbgCBData> COMMON_DBG_CB;

[vk::binding(5, DESC_SET_PER_FRAME)] ByteAddressBuffer COMMON_GEOM_STREAMS[GPU_GeomStreamID::COUNT];

[vk::binding(6, DESC_SET_PER_FRAME)] StructuredBuffer<GPU_MeshLOD>      COMMON_MESH_LOD_BUFFER;
[vk::binding(7, DESC_SET_PER_FRAME)] StructuredBuffer<GPU_Mesh>         COMMON_MESH_BUFFER;
[vk::binding(8, DESC_SET_PER_FRAME)] StructuredBuffer<GPU_GeomMaterial> COMMON_MATERIALS;
[vk::binding(9, DESC_SET_PER_FRAME)] Texture2D                          COMMON_MTL_TEXTURES[];

[vk::binding(10, DESC_SET_PER_FRAME)] StructuredBuffer<GPU_GeomInst> COMMON_INST_BUFFER;

[vk::binding(11, DESC_SET_PER_FRAME)] Texture2D COMMON_DEPTH;
[vk::binding(12, DESC_SET_PER_FRAME)] Texture2D COMMON_HZB;


#define GET_COMMON_SAMPLER(SMP_IDX)     COMMON_SAMPLERS[(uint)(SMP_IDX)]
#define GET_COMMON_CMP_SAMPLER(SMP_IDX) COMMON_CMP_SAMPLERS[(uint)(SMP_IDX)]


#define COMMON_DBG_USE_IBL                                  DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 0, false)
#define COMMON_DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED      DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 1, true)
#define COMMON_DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED          DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 2, true)
#define COMMON_DBG_IS_CSM_ENABLED                           DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 3, true)
#define COMMON_DBG_IS_CSM_VISUALIZATION_ENABLED             DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 4, false)
#define COMMON_DBG_IS_CSM_CASCADE_BLEND_ENABLED             DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 5, true)
#define COMMON_DBG_IS_CSM_FILTER_RANDOM_OFFSETS_ENABLED     DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 6, false)
#define COMMON_DBG_IS_CSM_PCSS_ENABLED                      DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 7, true)
#define COMMON_DBG_IS_CSM_PCSS_RAND_ROT_ENABLED             DBG_BIT_VALUE(COMMON_DBG_CB.flags_0, 8, true)

#define COMMON_DBG_FORCED_GEOM_LOD                          DBG_VALUE(COMMON_DBG_CB.forcedGeomLOD, -1)
#define COMMON_DBG_RT_VIEW_TYPE                             DBG_VALUE(COMMON_DBG_CB.rtViewType, GPU_DbgRTViewType::NONE)
#define COMMON_DBG_TONEMAP_PRESET                           DBG_VALUE(COMMON_DBG_CB.tonemapPreset, GPU_DbgTonemapPreset::ACES)
#define COMMON_DBG_CSM_PCF_PRESET                           DBG_VALUE(COMMON_DBG_CB.csmPCFPreset, GPU_DbgCsmPCFPreset::VOGEL_DISK)

#define COMMON_DBG_IS_GEOM_GPU_CULLING_ENABLED              (COMMON_DBG_IS_GEOM_GPU_FRUSTUM_CULLING_ENABLED || COMMON_DBG_IS_GEOM_GPU_HZB_CULLING_ENABLED)


#define COMMON_MAIN_CAM_WPOS                        COMMON_CB.mainCamWPos
#define COMMON_MAIN_CAM_Z_NEAR                      COMMON_CB.mainCamZNear
#define COMMON_MAIN_CAM_Z_FAR                       COMMON_CB.mainCamZFar
#define COMMON_MAIN_CAM_FRUSTUM                     COMMON_CB.mainCamFrustum
#define COMMON_MAIN_CAM_VIEW_MATRIX                 COMMON_CB.mainCamViewMatr
#define COMMON_MAIN_CAM_PROJ_MATRIX                 COMMON_CB.mainCamProjMatr
#define COMMON_MAIN_CAM_VIEW_PROJ_MATRIX            COMMON_CB.mainCamViewProjMatr
#define COMMON_MAIN_CAM_INV_VIEW_MATRIX             COMMON_CB.mainCamInvViewMatr
#define COMMON_MAIN_CAM_INV_PROJ_MATRIX             COMMON_CB.mainCamInvProjMatr
#define COMMON_MAIN_CAM_INV_VIEW_PROJ_MATRIX        COMMON_CB.mainCamInvViewProjMatr

#define COMMON_SCREEN_SIZE                          COMMON_CB.screenSize
#define COMMON_INV_SCREEN_SIZE                      rcp(COMMON_SCREEN_SIZE_F)

#define COMMON_CULLING_FRUSTUM                      COMMON_CB.cullingFrustum
#define COMMON_CULLING_VIEW_PROJ_MATRIX             COMMON_CB.cullingViewProjMatr
#define COMMON_CULLING_VIEW_PROJ_MATRIX_PREV        COMMON_CB.cullingViewProjMatrPrev

#define COMMON_SUN_LIGHT_DIRECTION                  COMMON_CB.sunLightDir
#define COMMON_SUN_LIGHT_COLOR                      unpackUnorm4x8ToFloat(COMMON_CB.sunLightColor)

#define COMMON_CSM_DATA                                       COMMON_CB.csmData
#define COMMON_CSM_VIEW_FRUSTUM(CASCADE_IDX)                  COMMON_CSM_DATA.viewFrustums[CASCADE_IDX]
#define COMMON_CSM_VIEW_MATRIX(CASCADE_IDX)                   COMMON_CSM_DATA.viewMatrices[CASCADE_IDX]
#define COMMON_CSM_VIEW_PROJ_MATRIX(CASCADE_IDX)              COMMON_CSM_DATA.viewProjMatrices[CASCADE_IDX]
#define COMMON_CSM_CASCADE_DISTANCE(CASCADE_IDX)              COMMON_CSM_DATA.cascadeDistances[CASCADE_IDX]
#define COMMON_CSM_CASCADE_BOUNDS(CASCADE_IDX)                float2((CASCADE_IDX) == 0 ? 0.f : COMMON_CSM_CASCADE_DISTANCE((CASCADE_IDX) - 1), COMMON_CSM_CASCADE_DISTANCE(CASCADE_IDX))
#define COMMON_CSM_CASCADE_Z_NEAR(CASCADE_IDX)                COMMON_CSM_DATA.cascadeZNear[CASCADE_IDX]
#define COMMON_CSM_CASCADE_Z_FAR(CASCADE_IDX)                 COMMON_CSM_DATA.cascadeZFar[CASCADE_IDX]
#define COMMON_CSM_CASCADE_WORLD_UNITS_PER_TEXEL(CASCADE_IDX) COMMON_CSM_DATA.cascadeWorldUnitsPerPixel[CASCADE_IDX]
#define COMMON_CSM_CASCADE_BLEND_THRESHOLD_COEF               COMMON_CSM_DATA.cascadeBlendThresholdCoef
#define COMMON_CSM_FILTER_DISK_SAMPLE_COUNT                   COMMON_CSM_DATA.filterDiskSampleCount
#define COMMON_CSM_FILTER_DISK_RADIUS                         COMMON_CSM_DATA.filterDiskRadius
#define COMMON_CSM_FILTER_GRID_HALF_SIZE                      COMMON_CSM_DATA.filterGridHalfSize
#define COMMON_CSM_TEX_SIZE                                   COMMON_CSM_DATA.texSize
#define COMMON_CSM_INV_TEX_SIZE                               (ONEF2 / COMMON_CSM_TEX_SIZE)

#define COMMON_CSM_PCSS_DATA                         COMMON_CSM_DATA.pcssData
#define COMMON_CSM_PCSS_LIGHT_ANGULAR_SLOPE          COMMON_CSM_PCSS_DATA.lightAngularSlope
#define COMMON_CSM_PCSS_MAX_SEARCH_RADIUS_TEXELS     COMMON_CSM_PCSS_DATA.maxSearchRadiusTexels
#define COMMON_CSM_PCSS_MAX_FILTER_RADIUS_TEXELS     COMMON_CSM_PCSS_DATA.maxFilterRadiusTexels
#define COMMON_CSM_PCSS_MIN_FILTER_RADIUS_TEXELS     COMMON_CSM_PCSS_DATA.minFilterRadiusTexels
#define COMMON_CSM_PCSS_SEARCH_RADIUS_SCALE          COMMON_CSM_PCSS_DATA.searchRadiusScale
#define COMMON_CSM_PCSS_FILTER_RADIUS_SCALE          COMMON_CSM_PCSS_DATA.filterRadiusScale
#define COMMON_CSM_PCSS_SEARCH_SAMPLES_COUNT         COMMON_CSM_PCSS_DATA.searchSamplesCount
#define COMMON_CSM_PCSS_FILTER_SAMPLES_COUNT         COMMON_CSM_PCSS_DATA.filterSamplesCount


#define COMMON_INST_COUNT COMMON_INST_BUFFER.getCount()


#endif