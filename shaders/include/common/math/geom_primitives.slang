#ifndef MATH_GEOM_PRIMITIVES_H
#define MATH_GEOM_PRIMITIVES_H

#include "common/math/constants.slang"
#include "common/math/common.slang"


struct SPHERE
{
    float3 position;
    float radius;
};


struct AABB
{
    float3 GetCenter()
    {
        return (minn + maxx) * 0.5f;
    }

    float3 GetSize()
    {
        return maxx - minn;
    }

    void GetVertices(out float3 verts[8])
    {
        const float3 size = GetSize();

        verts[0] = minn;
        verts[1] = minn + float3(size.x, 0.f, 0.f);
        verts[2] = minn + float3(0.f, size.y, 0.f);
        verts[3] = minn + float3(0.f, 0.f, size.z);
        verts[4] = minn + float3(size.xy, 0.f);
        verts[5] = minn + float3(0.f, size.yz);
        verts[6] = minn + float3(size.x, 0.f, size.z);
        verts[7] = minn + size;
    }

    void GetMinMaxUVZ(in float4x4 viewProjMatr, out float2 minUV, out float2 maxUV, out float closestZ)
    {
        float3 verts[8];
        GetVertices(verts);

        minUV = ONEF2;
        maxUV = ZEROF2;

    #ifdef ENV_REVERSED_Z
        closestZ = 0.f;
    #else
        closestZ = 1.f;
    #endif

        for (uint i = 0; i < 8; ++i) {
            float4 clipPos = mul(viewProjMatr, float4(verts[i], 1.f));

            float3 NDC = clipPos.xyz / clipPos.w;
            NDC.xy = clamp(NDC.xy, -ONEF2, ONEF2);
            NDC.z = clamp(NDC.z, 0.f, 1.f);

            const float2 UV = NDCToUV(NDC.xy);

            minUV = min(UV, minUV);
            maxUV = max(UV, maxUV);

        #ifdef ENV_REVERSED_Z
            closestZ = saturate(max(closestZ, NDC.z));
        #else
            closestZ = saturate(min(closestZ, NDC.z));
        #endif
        }
    }    

    float3 minn;
    float3 maxx;
};


struct PLANE
{
    float3 normal;
    float distance;
};


struct FRUSTUM
{
    bool IsIntersect(in SPHERE sphere)
    {
        for (uint i = 0; i < 6; ++i) {
            PLANE plane = planes[i];

            if (dot(plane.normal, sphere.position) + plane.distance < -sphere.radius) {
                return false;
            }
        }

        return true;
    }

    bool IsIntersect(in AABB aabb)
    {
        for (uint i = 0; i < 6; ++i) {
            PLANE plane = planes[i];

            const float3 p = float3(
                (plane.normal.x >= 0.f) ? aabb.maxx.x : aabb.minn.x,
                (plane.normal.y >= 0.f) ? aabb.maxx.y : aabb.minn.y,
                (plane.normal.z >= 0.f) ? aabb.maxx.z : aabb.minn.z
            );

            if (dot(plane.normal, p) + plane.distance < 0.f) {
                return false;
            }
        }

        return true;
    }

    PLANE planes[6];
};


#endif