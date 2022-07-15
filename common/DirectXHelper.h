//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include <wrl.h>

#include <future>

#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Foundation.Collections.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <filesystem>

namespace DXHelper
{
    template <typename F>
    void D3D11StoreAndRestoreState(ID3D11DeviceContext* immediateContext, F customRenderingCode)
    {
        // Query the d3d11 state before rendering
        static_assert(
            sizeof(winrt::com_ptr<ID3D11ShaderResourceView>) == sizeof(void*),
            "Below code reiles on winrt::com_ptr being exactly one pointer in size");

        winrt::com_ptr<ID3D11VertexShader> vertexShader;
        winrt::com_ptr<ID3D11GeometryShader> geometryShader;
        winrt::com_ptr<ID3D11PixelShader> pixelShader;
        winrt::com_ptr<ID3D11Buffer> vsConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        winrt::com_ptr<ID3D11Buffer> psConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        winrt::com_ptr<ID3D11ShaderResourceView> views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
        winrt::com_ptr<ID3D11SamplerState> psSampler;
        winrt::com_ptr<ID3D11RasterizerState> rasterizerState;
        winrt::com_ptr<ID3D11DepthStencilState> depthStencilState;
        winrt::com_ptr<ID3D11BlendState> blendState;
        winrt::com_ptr<ID3D11InputLayout> inputLayout;
        winrt::com_ptr<ID3D11Buffer> vertexBuffer;
        winrt::com_ptr<ID3D11Buffer> indexBuffer;
        FLOAT blendFactor[4] = {};
        UINT sampleMask = 0;
        D3D11_PRIMITIVE_TOPOLOGY primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        UINT stencilRef = 0;
        UINT vertexBufferStrides = 0;
        UINT vertexBufferOffsets = 0;
        DXGI_FORMAT indexBufferFormat = DXGI_FORMAT_UNKNOWN;
        UINT indexBufferOffset = 0;

        immediateContext->VSGetShader(vertexShader.put(), nullptr, nullptr);
        immediateContext->VSGetConstantBuffers(
            0, std::extent<decltype(vsConstantBuffers)>::value, reinterpret_cast<ID3D11Buffer**>(vsConstantBuffers));
        immediateContext->GSGetShader(geometryShader.put(), nullptr, nullptr);
        immediateContext->PSGetShader(pixelShader.put(), nullptr, nullptr);
        immediateContext->PSGetShaderResources(0, std::extent<decltype(views)>::value, reinterpret_cast<ID3D11ShaderResourceView**>(views));
        immediateContext->PSGetConstantBuffers(
            0, std::extent<decltype(psConstantBuffers)>::value, reinterpret_cast<ID3D11Buffer**>(psConstantBuffers));
        immediateContext->PSGetSamplers(0, 1, psSampler.put());
        immediateContext->RSGetState(rasterizerState.put());
        immediateContext->OMGetDepthStencilState(depthStencilState.put(), &stencilRef);
        immediateContext->OMGetBlendState(blendState.put(), blendFactor, &sampleMask);
        immediateContext->IAGetPrimitiveTopology(&primitiveTopology);
        immediateContext->IAGetInputLayout(inputLayout.put());
        immediateContext->IAGetVertexBuffers(0, 1, vertexBuffer.put(), &vertexBufferStrides, &vertexBufferOffsets);
        immediateContext->IAGetIndexBuffer(indexBuffer.put(), &indexBufferFormat, &indexBufferOffset);

        customRenderingCode();

        // Restore the d3d11 state.
        immediateContext->VSSetShader(vertexShader.get(), nullptr, 0);
        immediateContext->VSSetConstantBuffers(
            0, std::extent<decltype(vsConstantBuffers)>::value, reinterpret_cast<ID3D11Buffer**>(vsConstantBuffers));
        immediateContext->GSSetShader(geometryShader.get(), nullptr, 0);
        immediateContext->PSSetShader(pixelShader.get(), nullptr, 0);
        immediateContext->PSSetShaderResources(0, std::extent<decltype(views)>::value, reinterpret_cast<ID3D11ShaderResourceView**>(views));
        immediateContext->PSSetConstantBuffers(
            0, std::extent<decltype(psConstantBuffers)>::value, reinterpret_cast<ID3D11Buffer**>(psConstantBuffers));
        immediateContext->PSSetSamplers(0, 1, reinterpret_cast<ID3D11SamplerState**>(&psSampler));
        immediateContext->RSSetState(rasterizerState.get());
        immediateContext->OMSetDepthStencilState(depthStencilState.get(), stencilRef);
        immediateContext->OMSetBlendState(blendState.get(), blendFactor, sampleMask);
        immediateContext->IASetPrimitiveTopology(primitiveTopology);
        immediateContext->IASetInputLayout(inputLayout.get());
        ID3D11Buffer* vb = vertexBuffer.get();
        immediateContext->IASetVertexBuffers(0, 1, &vb, &vertexBufferStrides, &vertexBufferOffsets);
        immediateContext->IASetIndexBuffer(indexBuffer.get(), indexBufferFormat, indexBufferOffset);
    }

    // Function that reads from a binary file as a blocking operation.
    // This is based on https://docs.microsoft.com/en-us/windows/win32/fileio/opening-a-file-for-reading-or-writing.
    // Modifications were made to ensure this works on Win10 UWP, based on information from
    // https://walbourn.github.io/dual-use-coding-techniques-for-games-part-2/.
    inline std::vector<byte> ReadFromFile(const std::wstring& fileName)
    {

// Need to use absolute filepath on Desktop
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

        wchar_t moduleFullyQualifiedFilename[MAX_PATH] = {};
        uint32_t moduleFileNameLength = GetModuleFileNameW(NULL, moduleFullyQualifiedFilename, _countof(moduleFullyQualifiedFilename) - 1);
        moduleFullyQualifiedFilename[moduleFileNameLength] = L'\0';

        std::filesystem::path modulePath = moduleFullyQualifiedFilename;
        modulePath.replace_filename(fileName);
        std::wstring filePath = modulePath.c_str();
#else
        std::wstring filePath = fileName;
#endif
        // Use RAII for managing the handle, as shown in https://walbourn.github.io/dual-use-coding-techniques-for-games-part-1/
        struct HandleCloser
        {
            void operator()(HANDLE h)
            {
                if (h != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(h);
                }
            }
        };

        std::unique_ptr<void, HandleCloser> hFile(CreateFile2(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr));

        // In case of error, provide a customized message that includes the filePath and error code
        auto generateErrorMessage = [&filePath](const std::wstring& message) {
            throw winrt::hresult_error(
                GetLastError(),
                message + L" at" + filePath +
                    L".\nYou can find more information under https://docs.microsoft.com/en-us/windows/win32/debug/");
        };

        if (hFile.get() == INVALID_HANDLE_VALUE)
        {
            generateErrorMessage(L"Failed to access file");
        }

        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(hFile.get(), &fileSize) == false)
        {
            generateErrorMessage(L"Failed to read file size");
        }

        std::vector<byte> fileData(fileSize.QuadPart, 0);
        if (ReadFile(hFile.get(), &fileData[0], (DWORD)fileSize.QuadPart, 0, nullptr) == false)
        {
            generateErrorMessage(L"Failed to read file");
        }

        return fileData;
    }
    // Converts a length in device-independent pixels (DIPs) to a length in physical pixels.
    inline float ConvertDipsToPixels(float dips, float dpi)
    {
        static const float dipsPerInch = 96.0f;
        return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
    }

    inline DirectX::XMFLOAT3 Float3ToXMFloat3(winrt::Windows::Foundation::Numerics::float3 i)
    {
        DirectX::XMFLOAT3 o;
        XMStoreFloat3(&o, DirectX::XMLoadFloat3(&i));
        return o;
    };

    inline DirectX::XMFLOAT2 Float2ToXMFloat2(winrt::Windows::Foundation::Numerics::float2 i)
    {
        DirectX::XMFLOAT2 o;
        XMStoreFloat2(&o, DirectX::XMLoadFloat2(&i));
        return o;
    };
} // namespace DXHelper
