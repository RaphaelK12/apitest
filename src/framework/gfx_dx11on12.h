#pragma once

#include "gfx_dx11.h"

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------------------------------
class GfxApiDirect3D11On12 : public GfxApiDirect3D11
{
public:
	inline virtual const char* GetShortName() const override { return SGetShortName(); }
	inline virtual const char* GetLongName() const override { return SGetLongName(); }

	static const char* SGetShortName() { return "d3d11on12"; }
	static const char* SGetLongName() { return "Direct3D 11On12"; }
};