/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "platforms/ext/listx_impl.h"
#include "graphics/d3d12/direct3d12.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/pipeline_structs.h"
#include "types/base/error.h"

TListImpl(D3D12_TEXTURE_BARRIER);
TListImpl(D3D12_BUFFER_BARRIER);
TListImpl(ID3D12PipelineState);

D3D12_COMPARISON_FUNC mapDxCompareOp(ECompareOp op) {
	switch(op) {
		default:				return D3D12_COMPARISON_FUNC_NEVER;
		case ECompareOp_Lt:		return D3D12_COMPARISON_FUNC_LESS;
		case ECompareOp_Eq:		return D3D12_COMPARISON_FUNC_EQUAL;
		case ECompareOp_Leq:	return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case ECompareOp_Gt:		return D3D12_COMPARISON_FUNC_GREATER;
		case ECompareOp_Neq:	return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case ECompareOp_Geq:	return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case ECompareOp_Always:	return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

Error dxCheck(HRESULT result) {

	if(SUCCEEDED(result))
		return Error_none();

	switch (result) {

		case DXGI_ERROR_WAIT_TIMEOUT:
			return Error_timedOut(0, 0, "dxCheck() timed out");

		case E_OUTOFMEMORY:
			return Error_outOfMemory(0, "dxCheck() out of memory");
		case DXGI_ERROR_MORE_DATA:
			return Error_outOfMemory(0, "dxCheck() more data was required but wasn't provided");

		case DXGI_ERROR_DEVICE_HUNG:
			return Error_invalidState(0, "dxCheck() device hung");
		case DXGI_ERROR_DEVICE_REMOVED:
			return Error_invalidState(1, "dxCheck() device removed");
		case DXGI_ERROR_DEVICE_RESET:
			return Error_invalidState(2, "dxCheck() device reset");
		case DXGI_ERROR_ACCESS_LOST:
			return Error_invalidState(3, "dxCheck() access lost");
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			return Error_invalidState(4, "dxCheck() internal driver error");
		case D3D12_ERROR_DRIVER_VERSION_MISMATCH:
			return Error_invalidState(5, "dxCheck() driver version mismatch");
		case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE:
			return Error_invalidState(6, "dxCheck() graphics source in use");
		case E_FAIL:
			return Error_invalidState(7, "dxCheck() failed");
		case DXGI_ERROR_CANNOT_PROTECT_CONTENT:
			return Error_invalidState(8, "dxCheck() can't protected content");
		case DXGI_ERROR_WAS_STILL_DRAWING:
			return Error_invalidState(9, "dxCheck() device is still busy");
		case DXGI_ERROR_NAME_ALREADY_EXISTS:
			return Error_invalidState(10, "dxCheck() name already exists");
		case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:
			return Error_invalidState(11, "dxCheck() not currently available");
		case DXGI_ERROR_SESSION_DISCONNECTED:
			return Error_invalidState(12, "dxCheck() session disconnected");
		case DXGI_ERROR_FRAME_STATISTICS_DISJOINT:
			return Error_invalidState(13, "dxCheck() statistics gathering was interrupted");
		case DXGI_ERROR_NONEXCLUSIVE:
			return Error_invalidState(14, "dxCheck() can't acquire global resource counter");
		case DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE:
			return Error_invalidState(15, "dxCheck() swapchain output is stale");

		case E_INVALIDARG:
			return Error_invalidParameter(0, 0, "dxCheck() invalid argument");
		case DXGI_ERROR_INVALID_CALL:
			return Error_invalidParameter(0, 0, "dxCheck() invalid call");

		case DXGI_ERROR_UNSUPPORTED:
			return Error_unsupportedOperation(0, "dxCheck() unsupported operation");

		case DXGI_ERROR_ALREADY_EXISTS:
			return Error_invalidParameter(0, 0, "dxCheck() already exists");

		case D3D12_ERROR_ADAPTER_NOT_FOUND:
			return Error_notFound(0, 0, "dxCheck() adapter not found");
		case DXGI_ERROR_NOT_FOUND:
			return Error_notFound(1, 0, "dxCheck() not found");
		case DXGI_ERROR_SDK_COMPONENT_MISSING:
			return Error_notFound(2, 0, "dxCheck() sdk component is missing");

		case DXGI_ERROR_ACCESS_DENIED:
			return Error_unauthorized(0, "dxCheck() not permitted");

		case E_NOTIMPL:
			return Error_unimplemented(0, "dxCheck() not implemented");

		default:
			return Error_unsupportedOperation(3, "dxCheck() has unknown error");
	}
}

D3D12_GPU_VIRTUAL_ADDRESS getDxDeviceAddress(DeviceData data) {
	return DeviceBufferRef_ptr(data.buffer)->resource.deviceAddress + data.offset;
}

D3D12_GPU_VIRTUAL_ADDRESS getDxLocation(DeviceData data, U64 localOffset) {
	return getDxDeviceAddress(data) + localOffset;
}
