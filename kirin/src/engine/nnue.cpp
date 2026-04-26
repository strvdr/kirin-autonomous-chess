/*   Kirin is an autonomous chess system that allows you to play against an AI opponent in the real world.
*    Copyright (C) 2026 Strydr Silverberg
*    nnue.cpp - Efficiently updatable neural network evaluation scaffold
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "nnue.h"
#include "bitboard.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

/************ Network Format Constants ************/
static constexpr char nnueMagic[8] = { 'K', 'I', 'R', 'I', 'N', 'N', 'U', 'E' };
static constexpr std::uint32_t nnueFormatVersion = 1;
static constexpr int nnueFeatureCount = 12 * 64;
static constexpr int defaultHiddenSize = 32;
static constexpr int defaultActivationLimit = 255;
static constexpr int maxHiddenSize = 1024;

struct Network {
    int hiddenSize;
    int activationLimit;
    int outputBias;
    std::vector<int> hiddenBiases;
    std::vector<int> outputWeights;
    std::vector<int> featureWeights;
    std::string source;
    bool external;
    bool linearFeatureFastPath;
};

static bool nnueInitialized = false;
static bool nnueEnabled = false;
static std::string nnueLastError = "NNUE has not been loaded";
static Network network;

static int basePieceValue(int piece) {
    switch (piece) {
        case P:
        case p:
            return 100;
        case N:
        case n:
            return 320;
        case B:
        case b:
            return 330;
        case R:
        case r:
            return 500;
        case Q:
        case q:
            return 900;
        default:
            return 0;
    }
}

static int centralityBonus(int square) {
    int rank = square / 8;
    int file = square % 8;
    int rankDistance = (rank < 4) ? (3 - rank) : (rank - 4);
    int fileDistance = (file < 4) ? (3 - file) : (file - 4);
    return 14 - 4 * (rankDistance + fileDistance);
}

static int pawnAdvanceBonus(int piece, int square) {
    int rank = square / 8;

    if (piece == P) {
        return (6 - rank) * 6;
    }
    if (piece == p) {
        return (rank - 1) * 6;
    }
    return 0;
}

static int bootstrapFeatureValue(int piece, int square) {
    int value = basePieceValue(piece);

    if (piece == N || piece == n || piece == B || piece == b) {
        value += centralityBonus(square);
    }
    if (piece == P || piece == p) {
        value += pawnAdvanceBonus(piece, square);
    }

    return (piece <= K) ? value : -value;
}

static int featureIndex(int piece, int square) {
    return piece * 64 + square;
}

static int featureWeight(const Network& net, int feature, int hidden) {
    return net.featureWeights[feature * net.hiddenSize + hidden];
}

static int clippedRelu(const Network& net, int value) {
    if (value < 0) return 0;
    if (value > net.activationLimit) return net.activationLimit;
    return value;
}

static bool readBytes(std::ifstream& input, void *data, std::size_t bytes) {
    input.read(static_cast<char *>(data), static_cast<std::streamsize>(bytes));
    return input.good();
}

static bool readU32(std::ifstream& input, std::uint32_t& value) {
    return readBytes(input, &value, sizeof(value));
}

static bool readI32(std::ifstream& input, std::int32_t& value) {
    return readBytes(input, &value, sizeof(value));
}

static bool readI32Vector(std::ifstream& input, std::vector<int>& values, std::size_t count) {
    values.resize(count);

    for (std::size_t index = 0; index < count; index++) {
        std::int32_t value = 0;
        if (!readI32(input, value)) {
            return false;
        }
        values[index] = static_cast<int>(value);
    }

    return true;
}

static Network makeEmptyNetwork(int hiddenSize, int activationLimit) {
    Network net;
    net.hiddenSize = hiddenSize;
    net.activationLimit = activationLimit;
    net.outputBias = 0;
    net.hiddenBiases.assign(hiddenSize, 0);
    net.outputWeights.assign(hiddenSize, 0);
    net.featureWeights.assign(nnueFeatureCount * hiddenSize, 0);
    net.source = "empty";
    net.external = false;
    net.linearFeatureFastPath = false;
    return net;
}

static bool isLinearFeatureNetwork(const Network& net) {
    if (net.hiddenSize != nnueFeatureCount || net.activationLimit != 1) {
        return false;
    }

    for (int hidden = 0; hidden < net.hiddenSize; hidden++) {
        if (net.hiddenBiases[hidden] != 0) {
            return false;
        }
    }

    for (int feature = 0; feature < nnueFeatureCount; feature++) {
        for (int hidden = 0; hidden < net.hiddenSize; hidden++) {
            int expected = (hidden == feature) ? 1 : 0;
            if (featureWeight(net, feature, hidden) != expected) {
                return false;
            }
        }
    }

    return true;
}

static Network makeBootstrapNetwork() {
    Network net = makeEmptyNetwork(defaultHiddenSize, defaultActivationLimit);

    for (int piece = P; piece <= k; piece++) {
        for (int square = a8; square <= h1; square++) {
            int value = bootstrapFeatureValue(piece, square);
            int scaled = value / 16;
            int index = featureIndex(piece, square);

            net.featureWeights[index * net.hiddenSize] = scaled;
            net.featureWeights[index * net.hiddenSize + 1] = -scaled;
        }
    }

    net.outputWeights[0] = 16;
    net.outputWeights[1] = -16;
    net.source = "builtin-bootstrap";
    return net;
}

static bool validateNetworkDimensions(std::uint32_t featureCount,
                                      std::uint32_t hiddenSize,
                                      std::int32_t activationLimit) {
    if (featureCount != nnueFeatureCount) {
        nnueLastError = "NNUE feature count does not match Kirin's piece-square features";
        return false;
    }
    if (hiddenSize == 0 || hiddenSize > maxHiddenSize) {
        nnueLastError = "NNUE hidden size is outside the supported range";
        return false;
    }
    if (activationLimit <= 0) {
        nnueLastError = "NNUE activation limit must be positive";
        return false;
    }
    return true;
}

/************ NNUE Initialization ************/
void initNNUE() {
    network = makeBootstrapNetwork();
    nnueInitialized = true;
    nnueLastError.clear();
}

bool isNNUEInitialized() {
    return nnueInitialized;
}

/************ NNUE Network Loading ************/
bool loadNNUE(const char *path) {
    if (path == nullptr || path[0] == '\0') {
        nnueLastError = "NNUE file path is empty";
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        nnueLastError = "NNUE file could not be opened";
        return false;
    }

    char magic[sizeof(nnueMagic)] = {};
    if (!readBytes(input, magic, sizeof(magic)) ||
        std::memcmp(magic, nnueMagic, sizeof(nnueMagic)) != 0) {
        nnueLastError = "NNUE file has an invalid magic header";
        return false;
    }

    std::uint32_t version = 0;
    std::uint32_t featureCount = 0;
    std::uint32_t hiddenSize = 0;
    std::int32_t activationLimit = 0;
    std::int32_t outputBias = 0;

    if (!readU32(input, version) ||
        !readU32(input, featureCount) ||
        !readU32(input, hiddenSize) ||
        !readI32(input, activationLimit) ||
        !readI32(input, outputBias)) {
        nnueLastError = "NNUE file header is truncated";
        return false;
    }

    if (version != nnueFormatVersion) {
        nnueLastError = "NNUE file version is unsupported";
        return false;
    }
    if (!validateNetworkDimensions(featureCount, hiddenSize, activationLimit)) {
        return false;
    }

    Network loaded = makeEmptyNetwork(static_cast<int>(hiddenSize), activationLimit);
    loaded.outputBias = outputBias;
    loaded.source = path;
    loaded.external = true;

    if (!readI32Vector(input, loaded.hiddenBiases, hiddenSize) ||
        !readI32Vector(input, loaded.outputWeights, hiddenSize) ||
        !readI32Vector(input, loaded.featureWeights,
                       static_cast<std::size_t>(featureCount) * hiddenSize)) {
        nnueLastError = "NNUE file weights are truncated";
        return false;
    }

    char extra = 0;
    if (input.read(&extra, 1)) {
        nnueLastError = "NNUE file has trailing data";
        return false;
    }

    loaded.linearFeatureFastPath = isLinearFeatureNetwork(loaded);

    network = loaded;
    nnueInitialized = true;
    nnueLastError.clear();
    return true;
}

void resetNNUEToBootstrap() {
    initNNUE();
}

const char *getNNUELastError() {
    return nnueLastError.c_str();
}

const char *getNNUESource() {
    if (!nnueInitialized) {
        return "";
    }
    return network.source.c_str();
}

bool hasExternalNNUE() {
    return nnueInitialized && network.external;
}

/************ NNUE Evaluation ************/
int evaluateNNUE() {
    if (!nnueInitialized) {
        initNNUE();
    }

    if (network.linearFeatureFastPath) {
        int score = network.outputBias;

        for (int piece = P; piece <= k; piece++) {
            U64 bb = bitboards[piece];
            while (bb) {
                int square = getLSBindex(bb);
                score += network.outputWeights[featureIndex(piece, square)];
                popBit(bb, square);
            }
        }

        return (side == white) ? score : -score;
    }

    std::vector<int> accumulator = network.hiddenBiases;

    for (int piece = P; piece <= k; piece++) {
        U64 bb = bitboards[piece];
        while (bb) {
            int square = getLSBindex(bb);
            int index = featureIndex(piece, square);

            for (int hidden = 0; hidden < network.hiddenSize; hidden++) {
                accumulator[hidden] += featureWeight(network, index, hidden);
            }

            popBit(bb, square);
        }
    }

    int score = network.outputBias;
    for (int hidden = 0; hidden < network.hiddenSize; hidden++) {
        score += network.outputWeights[hidden] * clippedRelu(network, accumulator[hidden]);
    }

    return (side == white) ? score : -score;
}

/************ NNUE Configuration ************/
void setNNUEEnabled(bool enabled) {
    nnueEnabled = enabled;
}

bool isNNUEEnabled() {
    return nnueEnabled;
}
