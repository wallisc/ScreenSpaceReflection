//--------------------------------------------------------------------------------------
// File: Tutorial07.fx
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2D txDiffuse : register( t0 );

SamplerState samLinear : register( s0 );

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

cbuffer cbChangesEveryFrame : register( b2 )
{
    matrix World;
};

cbuffer cbMaterial : register( b3 )
{
    float reflectivity;
};

//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
    float4 Norm : NORMAL0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float4 Norm : NORMAL0;
    float4 CamPos : NORMAL1;
    float4 WorldPos : NORMAL2;
};

struct PS_OUTPUT
{
    float4 Color : COLOR0;
    float4 ScreenRefl : COLOR1;
    float2 Material : COLOR2;
    float4 CamPos : COLOR3;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output;

    output.WorldPos = mul( input.Pos, World );
    output.CamPos = mul( output.WorldPos, View );
    output.Pos = mul( output.CamPos, Projection );
    output.Norm = input.Norm;
    output.Tex = input.Tex;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
PS_OUTPUT PS( PS_INPUT input) : SV_Target
{
    PS_OUTPUT output;

	// Calculating in pixel shader for precision
    float3 n = normalize(mul( input.Norm.xyz, World ));
    float3 d = normalize(input.WorldPos.xyz - CamPos.xyz);
    float3 refl = normalize(reflect(d, n));
    float3 ScreenRefl = mul( float4(refl, 0), View );

   output.Color = txDiffuse.Sample( samLinear, input.Tex );
   output.ScreenRefl = float4(ScreenRefl, 0);
   output.Material = float4(reflectivity, 0, 0, 0);
   output.CamPos = input.CamPos;
   return output;
}
