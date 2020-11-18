#include "pch.h"
#include "Noise.h"

namespace VoidAndClusterFunctions {
    std::array<int, 256> computeHistogramm(const std::vector<uint8_t>& array);
    std::vector<bool> binarizeArray(const std::vector<uint8_t>& array, const float positivePercentage);

    //search for biggest cluster or void
    glm::ivec2 findBestCandidate(const std::vector<bool>& binaryPattern, const glm::ivec2& resolution, const bool searchForCluster);
    
    //set countMinority to true to score clusters, set to false to score voids
    float voidClusterFilter(const glm::ivec2& coordinate, const std::vector<bool>& binaryPattern, const glm::ivec2& resolution, const bool countMinority);
    float gaussianFilter(glm::ivec2 offset);

    glm::ivec2 indexToCoordinate(const uint32_t index, const glm::ivec2& resolution);
    uint32_t coordinateToIndex(const glm::ivec2& coordinate, const glm::ivec2& resolution);

    std::vector<bool> createPrototypeBinaryPattern(const glm::ivec2& resolution);
}

std::vector<uint8_t> generateWhiteNoiseTexture(const glm::ivec2& resolution) {
    std::vector<uint8_t> noise;
    noise.resize(size_t(resolution.x) * size_t(resolution.y));
    for (auto& value : noise) {
        value = rand() % 256;
    }
    return noise;
}

namespace VoidAndClusterFunctions {

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

    glm::ivec2 findBestCandidate(const std::vector<bool>& binaryPattern, const glm::ivec2& resolution, const bool searchForCluster){
        glm::ivec2 candidateCoordinates = {};
        float candidateScore = 0;

        //iterate over every pixel
        for (uint32_t y = 0; y < resolution.y; y++) {
            for (uint32_t x = 0; x < resolution.x; x++) {
                const glm::ivec2 coordinate = glm::ivec2(x, y);
                const uint32_t index = coordinateToIndex(coordinate, resolution);
                const bool isMinorityPixel = binaryPattern[index];
                if (searchForCluster) {
                    //minority pixels are potential clusters
                    if (isMinorityPixel) {
                        const float currentScore = voidClusterFilter(coordinate, binaryPattern, resolution, true);
                        if (currentScore > candidateScore) {
                            candidateCoordinates = coordinate;
                            candidateScore = currentScore;
                        }
                    }
                }
                else {
                    //majority pixels are potential voids
                    if (!isMinorityPixel) {
                        const float currentScore = voidClusterFilter(coordinate, binaryPattern, resolution, false);
                        if (currentScore > candidateScore) {
                            candidateCoordinates = coordinate;
                            candidateScore = currentScore;
                        }
                    }
                }
            }
        }
        return candidateCoordinates;
    }

    float voidClusterFilter(const glm::ivec2& coordinate, const std::vector<bool>& binaryPattern, const glm::ivec2& resolution, const bool countMinority) {
        float score = 0.f;
        //iterate over offsets from pixel
        for (int q = -resolution.x / 2; q < resolution.x / 2; q++) {
            for (int p = -resolution.x / 2; p < resolution.x / 2; p++) {
                //transform offset into absolute pixel position
                const glm::ivec2 binaryPatternCo = glm::ivec2(
                    (resolution.x + coordinate.x - p) % resolution.x,
                    (resolution.x + coordinate.y - q) % resolution.x);

                const uint32_t index = coordinateToIndex(binaryPatternCo, resolution);
                //count pixel depending on if we count minority or majority pixels
                const bool isMinorityPixel = binaryPattern[index];
                const bool countPixel = countMinority ? isMinorityPixel : !isMinorityPixel;
                if (countPixel) {
                    score += gaussianFilter(glm::ivec2(p, q));
                }
            }
        }
        return score;
    }

    float gaussianFilter(glm::ivec2 offset) {
        const float standardDeviation = 1.5f;
        const float r = offset.x * offset.x + offset.y * offset.y;
        return glm::exp(-r * r / (2 * standardDeviation * standardDeviation));
    }

    glm::ivec2 indexToCoordinate(const uint32_t index, const glm::ivec2& resolution) {
        return glm::ivec2(index % resolution.x, index / resolution.x);
    }

    uint32_t coordinateToIndex(const glm::ivec2& coordinate, const glm::ivec2& resolution) {
        return resolution.y * coordinate.y + coordinate.x;
    }

    std::vector<bool> createPrototypeBinaryPattern(const glm::ivec2& resolution) {
        //create initial binary pattern by thresholding white noise
        const std::vector<uint8_t> whiteNoise = generateWhiteNoiseTexture(resolution);

        const float binarizationTargetPercentage = 0.1f;
        std::vector<bool> binaryArray = binarizeArray(whiteNoise, binarizationTargetPercentage);

        //see figure 2 in "The void-and-cluster method for dither array generation"
        //we now have a binary matrix with around 10 percent minority pixels which are set to true
        //now we want to modify the binary pattern so it's more even
        //to do this minority pixels in the biggest cluster are swapped with minority pixels in the biggest void
        //we stop when it converges
        while (true) {
            //search for biggest cluster
            const glm::ivec2 biggestCluster = findBestCandidate(binaryArray, resolution, true);

            const uint32_t clusterIndex = coordinateToIndex(biggestCluster, resolution);
            assert(binaryArray[clusterIndex] == true);

            //remove cluster
            binaryArray[clusterIndex] = false;

            //search for biggest void
            const glm::ivec2 biggestVoid = findBestCandidate(binaryArray, resolution, false);

            //stop when moving removing tightest cluster created biggest void
            if (biggestCluster == biggestVoid) {
                //restore removed pixel
                binaryArray[clusterIndex] = true;
                break;
            }

            const uint32_t voidIndex = coordinateToIndex(biggestVoid, resolution);
            assert(binaryArray[voidIndex] == false);

            //remove void, effectively swapping pixels
            binaryArray[voidIndex] = true;
        }
        return binaryArray;
    }
}

//reference: "The void-and-cluster method for dither array generation"
std::vector<uint8_t> generateBlueNoiseTexture(const glm::ivec2& resolution) {
    using namespace VoidAndClusterFunctions;
    const size_t pixelCount = size_t(resolution.x) * size_t(resolution.y);

    const std::vector<bool> prototypeBinaryPattern = createPrototypeBinaryPattern(resolution);    

    //remove minority pixels in tightest clusters and enter corresponding rank
    std::vector<bool> binaryPattern = prototypeBinaryPattern;
    std::vector<uint32_t> rankMatrix(pixelCount, 0);

    //rank corresponds to number of minority pixels in binary pattern
    int initialRank = 0;
    for (const auto isMinorityPixel : binaryPattern) {
        initialRank += isMinorityPixel ? 1 : 0;
    }
    int rank = initialRank;

    while (rank > 0) {
        const glm::vec2 tightestCluster = findBestCandidate(binaryPattern, resolution, true);
        const uint32_t clusterIndex = coordinateToIndex(tightestCluster, resolution);
        assert(binaryPattern[clusterIndex]);
        binaryPattern[clusterIndex] = false;
        rankMatrix[clusterIndex] = rank;
        rank--;
    }

    //reset rank and binary pattern
    rank = initialRank;
    binaryPattern = prototypeBinaryPattern;

    //fill up biggest voids
    //in the paper this is split into two phases because the meaning of majority and minority changes after half
    //however this is only semantic and does not have to be implemented in the code
    while (rank < pixelCount) {
        const glm::vec2 biggestVoid = findBestCandidate(binaryPattern, resolution, false);
        const uint32_t voidIndex = coordinateToIndex(biggestVoid, resolution);
        assert(!binaryPattern[voidIndex]);
        binaryPattern[voidIndex] = true;
        rankMatrix[voidIndex] = rank;
        rank++;
    }

    //normalize rank matrix for use as blue noise
    std::vector<uint8_t> blueNoise(pixelCount);
    float delta = (pixelCount - 1.f) / (255.f) / pixelCount;
    for (size_t i = 0; i < blueNoise.size(); i++) {
        blueNoise[i] = (rankMatrix[i] + 0.5f) / delta;
    }
    return blueNoise;
}