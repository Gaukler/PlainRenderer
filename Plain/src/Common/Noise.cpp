#include "pch.h"
#include "Noise.h"

//----- private function declarations -----
namespace VoidAndClusterFunctions {
	std::array<int, 256> computeHistogramm(const std::vector<uint8_t>& array);
	std::vector<bool> binarizeArray(const std::vector<uint8_t>& array, const float positivePercentage);

	glm::ivec2 indexToCoordinate(const size_t index, const glm::ivec2& resolution);
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
}

namespace PerlinNoiseHelperFunctions {
	glm::vec2 sampleGradientVectors2D(const int x, const int y, const std::vector<std::vector<glm::vec2>>& gradientVectors,
		const int gridCellCount);

	float computePerlineAbsMax(const int dimensions);
	float smoothstep(const float t);
}

//----- function implementations -----

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

        const size_t totalCount = array.size();
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
        for (size_t i = 0; i < totalCount; i++) {
            binaryArray[i] = array[i] <= threshold;
        }
        return binaryArray;
    }

    //sigma taken from: https://blog.demofox.org/2019/06/25/generating-blue-noise-textures-with-void-and-cluster/
    float gaussianFilter(const glm::ivec2& offset) {
        const float sigma = 1.9f;
        const float sigmaSquared = sigma * sigma;
        const float rSquared = float(offset.x * offset.x + offset.y * offset.y);
        return glm::exp(-rSquared / (2 * sigmaSquared));
    }

    glm::ivec2 indexToCoordinate(const size_t index, const glm::ivec2& resolution) {
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
        for (int y = 0; y < resolution.y; y++) {
            for (int x = 0; x < resolution.x; x++) {
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
        for (size_t i = 0; i < binaryArray.size(); i++) {
            currentPositiveCount += binaryArray[i] ? 1 : 0;
            if (currentPositiveCount > positivePixelCount) {
                binaryArray[i] = false;
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
std::vector<uint8_t> generateBlueNoiseTexture(const glm::ivec2& resolution, const size_t channelCount) {
    using namespace VoidAndClusterFunctions;

	const size_t pixelCount = size_t(resolution.x) * size_t(resolution.y);
	const size_t byteCount = pixelCount * channelCount;

	std::vector<uint8_t> texture;
	texture.resize(byteCount);

	for (size_t channel = 0; channel < channelCount; channel++) {
		const std::vector<bool> prototypeBinaryPattern = createPrototypeBinaryPattern(resolution, uint32_t(pixelCount * 0.1f));
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
			blueNoise[i] = uint8_t((rankMatrix[i] + 0.5f) / pixelCount * 255.f);
		}

		//write into texture
		for (size_t i = 0; i < blueNoise.size(); i++) {
			texture[i * channelCount + channel] = blueNoise[i];
		}
	}
    return texture;
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

namespace PerlinNoiseHelperFunctions {
	glm::vec2 sampleGradientVectors2D(const int x, const int y, const std::vector<std::vector<glm::vec2>>& gradientVectors,
		const int gridCellCount) {
		return gradientVectors[x % gridCellCount][y % gridCellCount];
	};

	//for max range see: https://digitalfreepen.com/2017/06/20/range-perlin-noise.html
	float computePerlineAbsMax(const int dimensions) {
		return glm::sqrt(dimensions / 4.f);
	}

	//from: "Improving Noise", from Ken Perlin
	float smoothstep(const float t) {
		return glm::pow(t, 5.f) * 6.f
			- 15.f * glm::pow(t, 4.f)
			+ 10.f * glm::pow(t, 3.f);
	}
}

std::vector<uint8_t> generate2DPerlinNoise(const glm::ivec2& resolution, const int gridCellCount) {
	using namespace PerlinNoiseHelperFunctions;

	//init gradient vectors
	std::vector<std::vector<glm::vec2>> gradientVectors;

	//set correct size
	gradientVectors.resize(gridCellCount);
	for (std::vector<glm::vec2>& vector : gradientVectors) {
		vector.resize(gridCellCount);
	}

	for (int x = 0; x < gridCellCount; x++) {
		for (int y = 0; y < gridCellCount; y++) {
			//using random vectors instead of robust hashing method of original paper
			//but allows arbitrary grid cell count
			const float random = rand() / float(RAND_MAX) * 2.f * 3.1415f;
			gradientVectors[x][y] = glm::vec2(cos(random), sin(random));
		}
	}

	const int dimensions = 2;
	const float maxAbsValue = computePerlineAbsMax(dimensions);
	std::vector<uint8_t> noise(resolution.x * resolution.y);

	//compute value of every pixel
	for (int y = 0; y < resolution.y; y++) {
		for (int x = 0; x < resolution.x; x++) {

			const glm::vec2 uv = glm::vec2(x, y) / glm::vec2(resolution);
			glm::ivec2 gridIndex = glm::ivec2(uv * float(gridCellCount));
			glm::vec2 residual = float(gridCellCount) * uv - glm::vec2(gridIndex);

			//compute offset to all four cell corners and compute dot between offset and corner gradient vector
			glm::vec2 x0_offset = glm::vec2(residual.x, residual.y);
			float x0_dot = glm::dot(sampleGradientVectors2D(gridIndex.x, gridIndex.y, gradientVectors, gridCellCount), x0_offset);

			glm::vec2 x1_offset = glm::vec2(residual.x-1,residual.y);
			float x1_dot = glm::dot(sampleGradientVectors2D(gridIndex.x+1, gridIndex.y, gradientVectors, gridCellCount), x1_offset);

			glm::vec2 y0_offset = glm::vec2(residual.x, residual.y-1);
			float y0_dot = glm::dot(sampleGradientVectors2D(gridIndex.x, gridIndex.y+1, gradientVectors, gridCellCount), y0_offset);

			glm::vec2 y1_offset = glm::vec2(residual.x-1, residual.y-1);
			float y1_dot = glm::dot(sampleGradientVectors2D(gridIndex.x+1, gridIndex.y+1, gradientVectors, gridCellCount), y1_offset);

			//interpolation
			glm::vec2 t = glm::vec2(smoothstep(residual.x), smoothstep(residual.y));
			float value = glm::mix(glm::mix(x0_dot, x1_dot, t.x), glm::mix(y0_dot, y1_dot, t.x), t.y);

			//bring to range [-1, 1]
			value /= maxAbsValue;

			//bring to range [0, 1]
			value = value * 0.5f + 0.5f;
			value = glm::clamp(value, 0.f, 1.f);

			//store as short, which is integer in range [0, 255]
			noise[x + y * resolution.x] = (uint8_t)(value * 255);
		}
	}

	return noise;
}

float dotGradientOffset(const glm::ivec3& index, const glm::ivec3& cornerOffset, 
	const std::vector<std::vector<std::vector<glm::vec3>>>& gradientVectors, const glm::vec3& residual, const int gridCellCount) {

	const glm::vec3 offset = glm::vec3(residual.x-cornerOffset.x, residual.y-cornerOffset.y, residual.z- cornerOffset.z);
	const glm::vec3 gradientVector = gradientVectors
		[(index.x + cornerOffset.x) % gridCellCount]
		[(index.y + cornerOffset.y) % gridCellCount]
		[(index.z + cornerOffset.z) % gridCellCount];
	return glm::dot(gradientVector, offset);
}

std::vector<uint8_t> generate3DPerlinNoise(const glm::ivec3& resolution, const int gridCellCount) {
	using namespace PerlinNoiseHelperFunctions;

	//init gradient vectors
	std::vector<std::vector<std::vector<glm::vec3>>> gradientVectors;

	//set correct size
	gradientVectors.resize(gridCellCount);
	for (std::vector<std::vector<glm::vec3>>& vector2D : gradientVectors) {
		vector2D.resize(gridCellCount);
		for (std::vector<glm::vec3>& vector : vector2D) {
			vector.resize(gridCellCount);
		}
	}

	for (int x = 0; x < gridCellCount; x++) {
		for (int y = 0; y < gridCellCount; y++) {
			for (int z = 0; z < gridCellCount; z++) {
				//using random vectors instead of robust hashing method of original paper
				//but allows arbitrary grid cell count
				const glm::vec2 random = glm::vec2(rand(), rand()) / float(RAND_MAX) * 2.f * 3.1415f;
				gradientVectors[x][y][z] = 
					glm::vec3(sin(random.x) * cos(random.y), sin(random.x) * sin(random.x), cos(random.x));
			}
		}
	}

	const int dimensions = 3;
	const float maxAbsValue = computePerlineAbsMax(dimensions);
	std::vector<uint8_t> noise(resolution.x * resolution.y * resolution.z);

	//compute value of every pixel
	for (int y = 0; y < resolution.y; y++) {
		for (int x = 0; x < resolution.x; x++) {
			for (int z = 0; z < resolution.z; z++) {
				const glm::vec3 uv = glm::vec3(x, y, z) / glm::vec3(resolution);
				glm::ivec3 gridIndex = glm::ivec3(uv * float(gridCellCount));
				glm::vec3 residual = float(gridCellCount) * uv - glm::vec3(gridIndex);

				//compute dot of offset and gradients
				const float dot_000 = dotGradientOffset(gridIndex, glm::ivec3(0, 0, 0), gradientVectors, residual, gridCellCount);
				const float dot_001 = dotGradientOffset(gridIndex, glm::ivec3(0, 0, 1), gradientVectors, residual, gridCellCount);
				const float dot_010 = dotGradientOffset(gridIndex, glm::ivec3(0, 1, 0), gradientVectors, residual, gridCellCount);
				const float dot_011 = dotGradientOffset(gridIndex, glm::ivec3(0, 1, 1), gradientVectors, residual, gridCellCount);
				const float dot_100 = dotGradientOffset(gridIndex, glm::ivec3(1, 0, 0), gradientVectors, residual, gridCellCount);
				const float dot_101 = dotGradientOffset(gridIndex, glm::ivec3(1, 0, 1), gradientVectors, residual, gridCellCount);
				const float dot_110 = dotGradientOffset(gridIndex, glm::ivec3(1, 1, 0), gradientVectors, residual, gridCellCount);
				const float dot_111 = dotGradientOffset(gridIndex, glm::ivec3(1, 1, 1), gradientVectors, residual, gridCellCount);

				//interpolation
				const glm::vec3 t = glm::vec3(smoothstep(residual.x), smoothstep(residual.y), smoothstep(residual.z));
				const float interpolation_00 = glm::mix(dot_000, dot_001, residual.z);
				const float interpolation_01 = glm::mix(dot_010, dot_011, residual.z);
				const float interpolation_10 = glm::mix(dot_100, dot_101, residual.z);
				const float interpolation_11 = glm::mix(dot_110, dot_111, residual.z);

				const float interpolation_0 = glm::mix(interpolation_00, interpolation_01, residual.y);
				const float interpolation_1 = glm::mix(interpolation_10, interpolation_11, residual.y);

				float value = glm::mix(interpolation_0, interpolation_1, residual.x);

				//bring to range [-1, 1]
				value /= maxAbsValue;

				//bring to range [0, 1]
				value = value * 0.5f + 0.5f;
				value = glm::clamp(value, 0.f, 1.f);

				//store as short, which is integer in range [0, 255]
				noise[x + y * resolution.x + z * resolution.x * resolution.y] = (uint8_t)(value * 255);
			}
		}
	}

	return noise;
}