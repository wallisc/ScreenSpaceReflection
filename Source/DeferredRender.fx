Texture2D renderTx : register( t0 );
Texture2D screenRefl : register( t1 );
Texture2D material : register( t2 );
Texture2D camPos : register( t3 );

#define MAX_STEPS 300
#define MAX_INTERSECT_DIST 0.04

cbuffer cbNeverChanges : register( b0 )
{
    float4x4 View;
    float4 CamPos;
	float FarClip;
};

cbuffer cbChangeOnResize : register( b1 )
{
    float4x4 Projection;
    float4 Dimensions;
};

struct VertexOutput
{
   float4 pos : SV_POSITION;
   float2 tex : TEXCOORD0;
};

VertexOutput VS( uint id : SV_VertexID ) 
{
   VertexOutput output;
   output.tex = float2((id << 1) & 2, id & 2);
   output.pos = float4(output.tex * float2(2,-2) + float2(-1,1), 0, 1);
   return output;
}

float2 NormalizedDeviceCoordToScreenCoord(float2 ndc)
{
   float2 screenCoord;
   screenCoord.x = Dimensions.x * (ndc.x + 1.0f) / 2.0f;
   screenCoord.y = Dimensions.y * (1.0f - ((ndc.y + 1.0f) / 2.0f));
   return screenCoord;
}

float4 PS(VertexOutput input) : SV_TARGET
{
   int2 coord;
   int2 origin = input.tex * Dimensions;
   float t = 1;
   float4 reflRay = screenRefl[origin];
   float reflectivity = material[origin].x;
   float4 reflColor = float4(0, 0, 0, 0);
   
   // Tracing code from http://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html
   // c - view space coordinate
   // p - screen space coordinate
   // k - perspective divide
   float4 v0 = camPos[origin];
   float4 v1 = v0 + reflRay * FarClip;
     
   float4 p0 = mul( v0, Projection );
   float4 p1 = mul( v1, Projection );

   float k0 = 1.0f / p0.w;
   float k1 = 1.0f / p1.w;
   

   // 
   p0 *= k0; 
   p1 *= k1;

   p0.xy = NormalizedDeviceCoordToScreenCoord(p0.xy);
   p1.xy = NormalizedDeviceCoordToScreenCoord(p1.xy);

   v0 *= k0;
   v1 *= k1;

   float divisions = length(p1 - p0);
   float3 dV = (v1 - v0) / divisions;
   float dK = (k1 - k0) / divisions;
   float2 traceDir = (p1 - p0) / divisions;

   float maxSteps = min(MAX_STEPS, divisions);
   if (reflectivity > 0.0f)
   {
       while( t < maxSteps)
       {
          coord = origin + traceDir * t;
          if(coord.x >= Dimensions.x || coord.y >= Dimensions.y || coord.x < 0 || coord.y < 0) break;

          float curDepth = (v0 + dV * t).z;
          curDepth /= k0 + dK * t; // Reverse the perspective divide back to view space
          float storedDepth = camPos[coord].z;
          if( curDepth > storedDepth && curDepth - storedDepth < MAX_INTERSECT_DIST)
          {
             reflColor = renderTx[coord];
             break;
          }
          t++;
       }
   }
   return renderTx[origin] * (1.0f - reflectivity) + reflColor * reflectivity;
}