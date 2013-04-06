#include "HeightMap.h"

#include <algorithm>
#include <cassert>
#include <d3d11.h>
#include <DirectXPackedVector.h>
#include <fstream>

#include <DxErrorChecker.h>

namespace 
{
    // Function computes the average height of the ij element of a height map.
    // It averages itself with its eight neighbor pixels.  Note
    // that if a pixel is missing neighbor, we just do not include it
    // in the average.
    //
    // ----------
    // | 1| 2| 3|
    // ----------
    // |4 |ij| 6|
    // ----------
    // | 7| 8| 9|
    // ----------
    float computeAverageHeight(const HeightMap& heightMap,
                               const uint32_t rowIndex,
                               const uint32_t columnIndex)
    {
        float average = 0.0f;
        float numNeighbors = 0.0f;

        // Use int to allow negatives.
        for(int m = static_cast<int> (rowIndex) - 1; m <= static_cast<int> (rowIndex) + 1; ++m)
        {
            for(int n = static_cast<int> (columnIndex) - 1; n <= static_cast<int> (columnIndex) + 1; ++n)
            {
                const bool inBounds = m >= 0 && m < static_cast<int> (heightMap.mDimension) 
                                      && 
                                      n >= 0 && n < static_cast<int> (heightMap.mDimension);
                if(inBounds)
                {
                    average += heightMap.mData[m * heightMap.mDimension + n];
                    numNeighbors += 1.0f;
                }
            }
        }

        return average / numNeighbors;
    }
}

namespace HeightMapUtils
{
    // Load a height map from a RAW file and stores its content
    // in a vector.
    void loadHeightMapFromRAWFile(const std::string& filePath,  
                                  const float scaleFactor,
                                  HeightMap& heightMap)
    {
        assert(heightMap.mData.size() == heightMap.mDimension * heightMap.mDimension);

        // A height for each vertex
        const uint32_t numberOfPixels = heightMap.mDimension * heightMap.mDimension;
        std::vector<uint8_t> fileData(numberOfPixels);

        // Open the file.
        std::ifstream file;
        file.open(filePath.c_str(), std::ios_base::binary);
        assert(file);

        // Read the RAW bytes.
        file.read(reinterpret_cast<char*> (&fileData[0]), 
            static_cast<std::streamsize> (fileData.size()));

        // Done with file.
        file.close();

        // Copy the array data into a float array and scale it.
        for(size_t pixelIndex = 0; pixelIndex < numberOfPixels; ++pixelIndex)
        {
            heightMap.mData[pixelIndex] = (fileData[pixelIndex] / 255.0f) * scaleFactor;
        }
    }

    // Apply a filter to make height map more smooth
    // taking into account its neighbors pixels.
    void applyNeighborsFilter(HeightMap& heightMap)
    {
        assert(heightMap.mData.size() == heightMap.mDimension * heightMap.mDimension);

        HeightMap filteredHeightMap(heightMap.mDimension);

        for(uint32_t rowIndex = 0; rowIndex < heightMap.mDimension; ++rowIndex)
        {
            for(uint32_t columnIndex = 0; columnIndex < heightMap.mDimension; ++columnIndex)
            {
                const size_t currentIndex = rowIndex * heightMap.mDimension + columnIndex;
                filteredHeightMap.mData[currentIndex] = computeAverageHeight(heightMap,
                                                                             rowIndex, 
                                                                             columnIndex);
            }
        }

        // Replace the old height map with the filtered one.
        heightMap = filteredHeightMap;
    }

    ID3D11ShaderResourceView* buildHeightMapSRV(ID3D11Device& device,
                                                const HeightMap& heightMap,
                                                const uint32_t texture2DDescBindFlags)
    {
        assert(heightMap.mData.size() == heightMap.mDimension * heightMap.mDimension);

        D3D11_TEXTURE2D_DESC texture2DDesc;
        texture2DDesc.Width = heightMap.mDimension;
        texture2DDesc.Height = heightMap.mDimension;
        texture2DDesc.MipLevels = 1;
        texture2DDesc.ArraySize = 1;
        texture2DDesc.Format = DXGI_FORMAT_R16_FLOAT;
        texture2DDesc.SampleDesc.Count = 1;
        texture2DDesc.SampleDesc.Quality = 0;
        texture2DDesc.Usage = D3D11_USAGE_DEFAULT;
        texture2DDesc.BindFlags = texture2DDescBindFlags;
        texture2DDesc.CPUAccessFlags = 0;
        texture2DDesc.MiscFlags = 0;

        // HALF is defined for storing 16-bit float.
        std::vector<DirectX::PackedVector::HALF> halfHeightMap(heightMap.mData.size());
        std::transform(heightMap.mData.begin(), 
                       heightMap.mData.end(), 
                       halfHeightMap.begin(), 
                       DirectX::PackedVector::XMConvertFloatToHalf);

        D3D11_SUBRESOURCE_DATA subResourceData;
        subResourceData.pSysMem = &halfHeightMap[0];
        subResourceData.SysMemPitch = static_cast<uint32_t> (heightMap.mDimension) * sizeof(DirectX::PackedVector::HALF);
        subResourceData.SysMemSlicePitch = 0;

        ID3D11Texture2D* heightMapTexture = nullptr;
        HRESULT result = device.CreateTexture2D(&texture2DDesc, &subResourceData, &heightMapTexture);
        DxErrorChecker(result);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = texture2DDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = -1;
        ID3D11ShaderResourceView* heightMapSRV = nullptr;
        result = device.CreateShaderResourceView(heightMapTexture, &srvDesc, &heightMapSRV);
        DxErrorChecker(result);

        // SRV saves reference.
        heightMapTexture->Release();

        return heightMapSRV;
    }
}