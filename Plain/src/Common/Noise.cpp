#include "pch.h"
#include "Noise.h"

std::vector<uint8_t> generateWhiteNoiseTexture(const glm::ivec2& resolution) {
    std::vector<uint8_t> noise;
    noise.resize(size_t(resolution.x) * size_t(resolution.y));
    for (auto& value : noise) {
        value = rand() % 256;
    }
    return noise;
}

namespace VoidAndClusterFunctions {

    std::array<int, 256> computeHistogramm(const std::vector<uint8_t>& array);
    std::vector<bool> binarizeArray(const std::vector<uint8_t>& array, const float positivePercentage);

    glm::ivec2 indexToCoordinate(const uint32_t index, const glm::ivec2& resolution);
    uint32_t coordinateToIndex(const glm::ivec2& coordinate, const glm::ivec2& resolution);

    glm::ivec2 calculateToroidalOffset(const glm::ivec2 a, const glm::ivec2& b, const glm::ivec2& resolution);
    float gaussianFilter(const glm::ivec2& offset);

    std::vector<float> calculateFilterLut(const std::vector<bool>& binaryPattern, const glm::ivec2& resolution);
    std::vector<float> calculatePixelInfluence(const glm::ivec2& inputPosition, const glm::ivec2& resolution);
    std::vector<float> addInfluenceVectors(const std::vector<float>& a, const std::vector<float>& b);
    std::vector<float> subtractInfluenceVectors(const std::vector<float>& a, const std::vector<float>& b);
    
    size_t findBiggestVoid(const std::vector<float>& influence, const std::vector<bool>& binaryPattern);
    size_t findTightestCluster(const std::vector<float>& influence, const std::vector<bool>& binaryPattern);

    std::vector<bool> createPrototypeBinaryPattern(const glm::ivec2& resolution, const uint32_t minorityPixelCount);

    std::array<int, 256> computeHistogramm(const std::vector<uint8_t>& array) {
        std::array<int, 256> histogramm = {};
        for (const uint8_t value : array) {
            histogramm[value] += 1;
        }
        return histogramm;
    }

    std::vector<bool> binarizeArray(const std::vector<uint8_t>& array, const float targetPercentage) {
        assert(targetPercentage >= 0 || targetPercentage <= 1);
        std::array<int, 256> histogramm = computeHistogramm(array);

        const uint32_t totalCount = array.size();
        uint32_t currentCount = 0;
        uint32_t threshold = 0;
        //find threhold
        for (uint32_t i = 0; i < 256; i++) {
            currentCount += histogramm[i];
            const float currentPercentage = float(currentCount) / totalCount;
            if (currentPercentage >= targetPercentage) {
                threshold = i;
                break;
            }
        }

        std::vector<bool> binaryArray(totalCount);
        for (uint32_t i = 0; i < totalCount; i++) {
            binaryArray[i] = array[i] <= threshold;
        }
        return binaryArray;
    }

    //sigma taken from: https://blog.demofox.org/2019/06/25/generating-blue-noise-textures-with-void-and-cluster/
    float gaussianFilter(const glm::ivec2& offset) {
        const float sigma = 1.9;
        const float sigmaSquared = sigma * sigma;
        const float rSquared = offset.x * offset.x + offset.y * offset.y;
        return glm::exp(-rSquared / (2 * sigmaSquared));
    }

    glm::ivec2 indexToCoordinate(const uint32_t index, const glm::ivec2& resolution) {
        return glm::ivec2(index % resolution.x, index / resolution.x);
    }

    uint32_t coordinateToIndex(const glm::ivec2& coordinate, const glm::ivec2& resolution) {
        return resolution.y * coordinate.y + coordinate.x;
    }

    glm::ivec2 calculateToroidalOffset(const glm::ivec2 a, const glm::ivec2& b, const glm::ivec2& resolution) {
        //minimize offset in all dimensions separately
        glm::ivec2 result = abs(a - b);
        result = glm::min(result, abs(a - resolution - b));
        result = glm::min(result, abs(a + resolution - b));
        return result;
    }

    //return a vector containing the influence of the current pixel on every pixel
    std::vector<float> calculatePixelInfluence(const glm::ivec2& inputPosition, const glm::ivec2& resolution) {
        const size_t pixelCount = size_t(resolution.x) * size_t(resolution.y);
        std::vector<float> influence(pixelCount);

        //iterate over every pixel
        for (uint32_t y = 0; y < resolution.y; y++) {
            for (uint32_t x = 0; x < resolution.x; x++) {
                const glm::ivec2 currentPosition(x, y);
                const size_t index = coordinateToIndex(currentPosition, resolution);
                const glm::ivec2 toroidalOffset = calculateToroidalOffset(inputPosition, currentPosition, resolution);
                influence[index] = gaussianFilter(toroidalOffset);
            }
        }
        return influence;
    }
    
    std::vector<float> addInfluenceVectors(const std::vector<float>& a, const std::vector<float>& b) {
        assert(a.size() == b.size());
        std::vector<float> result(a.size());
        for (size_t i = 0; i < a.size(); i++) {
            result[i] = a[i] + b[i];
        }
        return result;
    }

    //a - b
    std::vector<float> subtractInfluenceVectors(const std::vector<float>& a, const std::vector<float>& b) {
        assert(a.size() == b.size());
        std::vector<float> result(a.size());
        for (size_t i = 0; i < a.size(); i++) {
            result[i] = a[i] - b[i];
        }
        return result;
    }

    std::vector<float> calculateFilterLut(const std::vector<bool>& binaryPattern, const glm::ivec2& resolution) {
        const size_t pixelCount = size_t(resolution.x) * size_t(resolution.y);
        std::vector<float> lut(pixelCount, 0);

        //iterate over every pixel
        for (int i = 0; i < binaryPattern.size(); i++) {
            if (binaryPattern[i]) {
                const glm::ivec2 position = indexToCoordinate(i, resolution);
                const std::vector<float> influence = calculatePixelInfluence(position, resolution);
                lut = addInfluenceVectors(lut, influence);
            }
        }
        return lut;
    }

    size_t findBiggestVoid(const std::vector<float>& influence, const std::vector<bool>& binaryPattern) {
        assert(influence.size() == binaryPattern.size());
        float min = std::numeric_limits<float>::max();
        size_t minIndex = 0;
        for (size_t i = 0; i < influence.size(); i++) {
            if (!binaryPattern[i] && influence[i] < min) {
                min = influence[i];
                minIndex = i;
            }
        }
        return minIndex;
    }
    
    size_t findTightestCluster(const std::vector<float>& influence, const std::vector<bool>& binaryPattern) {
        assert(influence.size() == binaryPattern.size());
        float max = std::numeric_limits<float>::min();
        size_t maxIndex = 0;
        for (size_t i = 0; i < influence.size(); i++) {
            if (binaryPattern[i] && influence[i] > max) {
                max = influence[i];
                maxIndex = i;
            }
        }
        return maxIndex;
    }

    std::vector<bool> createPrototypeBinaryPattern(const glm::ivec2& resolution, const uint32_t positivePixelCount) {
        //create initial binary pattern by thresholding white noise
        const std::vector<uint8_t> whiteNoise = generateWhiteNoiseTexture(resolution);
        
        const uint32_t pixelCount = resolution.x * resolution.y;
        const float binarizationTargetPercentage = float(positivePixelCount) / pixelCount;
        std::vector<bool> binaryArray = binarizeArray(whiteNoise, binarizationTargetPercentage);

        //remove true entries over target count as binarization may not be exact
        uint32_t currentPositiveCount = 0;
        for (auto& entry : binaryArray) {
            currentPositiveCount += entry ? 1 : 0;
            if (currentPositiveCount > positivePixelCount) {
                entry = false;
            }
        }

        std::vector<float> lut = calculateFilterLut(binaryArray, resolution);

        //see figure 2 in "The void-and-cluster method for dither array generation"
        //modify binary pattern so it's more even
        //swap minority pixels in biggest cluster with majority pixels in biggest void
        //stop when converging
        while (true) {
            //remove tightest pixel
            const size_t removedPixelIndex = findTightestCluster(lut, binaryArray);
            binaryArray[removedPixelIndex] = false;
            
            const glm::ivec2 removedPixelCoordinate = indexToCoordinate(removedPixelIndex, resolution);
            std::vector<float> removedPixelInfluence = calculatePixelInfluence(removedPixelCoordinate, resolution);
            lut = subtractInfluenceVectors(lut, removedPixelInfluence);

            const size_t addedPixelIndex = findBiggestVoid(lut, binaryArray);

            //stop when moving removing tightest cluster created biggest void
            if (removedPixelIndex == addedPixelIndex) {
                //restore removed pixel
                binaryArray[removedPixelIndex] = true;
                return binaryArray;
            }

            //remove void, effectively swapping pixels
            binaryArray[addedPixelIndex] = true;
            const glm::ivec2 addedPixelCoordinate = indexToCoordinate(addedPixelIndex, resolution);
            const std::vector<float> addedPixelInfluence = calculatePixelInfluence(addedPixelCoordinate, resolution);
            lut = addInfluenceVectors(lut, addedPixelInfluence);
        }
    }
}

//reference: "The void-and-cluster method for dither array generation"
//reference: https://blog.demofox.org/2019/06/25/generating-blue-noise-textures-with-void-and-cluster/
std::vector<uint8_t> generateBlueNoiseTexture(const glm::ivec2& resolution) {
    using namespace VoidAndClusterFunctions;
    const size_t pixelCount = size_t(resolution.x) * size_t(resolution.y);

    const std::vector<bool> prototypeBinaryPattern = createPrototypeBinaryPattern(resolution, pixelCount * 0.1f);
    const std::vector<float> initialLut = calculateFilterLut(prototypeBinaryPattern, resolution);

    std::vector<bool> binaryPattern = prototypeBinaryPattern;
    std::vector<float> lut = initialLut;
    std::vector<uint32_t> rankMatrix(pixelCount, 0);

    int onesCount = 0;
    for (const auto isMinorityPixel : binaryPattern) {
        onesCount += isMinorityPixel ? 1 : 0;
    }
    int rank = onesCount - 1;

    //remove tightest clusters and enter corresponding rank
    while (rank >= 0) {
        const size_t removedPixelIndex = findTightestCluster(lut, binaryPattern);
        binaryPattern[removedPixelIndex] = false;
        
        const glm::ivec2 removedPixelCoordinates = indexToCoordinate(removedPixelIndex, resolution);
        const std::vector<float> removedPixelInfluence = calculatePixelInfluence(removedPixelCoordinates, resolution);
        lut = subtractInfluenceVectors(lut, removedPixelInfluence);
    
        rankMatrix[removedPixelIndex] = rank;
        rank--;
    }

    //reset rank, binary pattern and lut
    rank = onesCount;
    binaryPattern = prototypeBinaryPattern;
    lut = initialLut;

    //fill up biggest voids
    //in the paper this is split into two phases because the meaning of majority and minority changes after half
    //however this is only semantic and does not have to be implemented in the code
    while (rank < pixelCount) {
        const size_t addedPixelIndex = findBiggestVoid(lut, binaryPattern);
        binaryPattern[addedPixelIndex] = true;
    
        const glm::ivec2 addedPixelCoordinates = indexToCoordinate(addedPixelIndex, resolution);
        const std::vector<float> addedPixelInfluence = calculatePixelInfluence(addedPixelCoordinates, resolution);
        lut = addInfluenceVectors(lut, addedPixelInfluence);
    
        rankMatrix[addedPixelIndex] = rank;
        rank++;
    }

    //normalize rank matrix for use as blue noise
    std::vector<uint8_t> blueNoise(pixelCount);
    for (size_t i = 0; i < blueNoise.size(); i++) {
        blueNoise[i] = (rankMatrix[i] + 0.5f) / pixelCount * 255.f;
    }

    return blueNoise;
}

std::vector<glm::vec2> generateBlueNoiseSampleSequence(const uint32_t count) {
    //using void and cluster prototype creation function to create discrete sample points
    const glm::ivec2 sampleMatrixResolution(64);
    const std::vector<bool> sampleMatrix = VoidAndClusterFunctions::createPrototypeBinaryPattern(sampleMatrixResolution, count);

    //turn into vector of coordinates
    std::vector<glm::vec2> samples;
    samples.reserve(count);
    for (size_t i = 0; i < sampleMatrix.size(); i++) {
        if (sampleMatrix[i]) {
            const glm::ivec2 iUV = VoidAndClusterFunctions::indexToCoordinate(i, sampleMatrixResolution);
            const glm::vec2 uv = glm::vec2(iUV) / glm::vec2(sampleMatrixResolution);
            samples.push_back(uv);
            if (samples.size() >= count) {
                for (size_t j = i+1; j < sampleMatrix.size(); j++) {
                    assert(!sampleMatrix[j]);
                }
                break;
            }
        }
    }
    assert(samples.size() == count);
    return samples;
}