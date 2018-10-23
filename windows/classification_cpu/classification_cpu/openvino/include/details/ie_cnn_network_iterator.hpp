// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief A header file for the CNNNetworkIterator class
 * @file ie_cnn_network_iterator.hpp
 */
#pragma once
#include <utility>
#include <unordered_set>
#include <list>
#include <iterator>

#include "ie_locked_memory.hpp"
#include "ie_icnn_network.hpp"

namespace InferenceEngine {
namespace details {

/**
 * @brief This class enables range loops for CNNNetwork objects
 */
class CNNNetworkIterator {
    std::unordered_set<CNNLayer*> visited;
    std::list<CNNLayerPtr> nextLayersTovisit;
    InferenceEngine::CNNLayerPtr currentLayer;
    ICNNNetwork * network = nullptr;

 public:
    /**
     * iterator trait definitions
     */
    typedef std::forward_iterator_tag iterator_category;
    typedef CNNLayerPtr value_type;
    typedef int         difference_type;
    typedef CNNLayerPtr pointer;
    typedef CNNLayerPtr reference;

    /**
     * @brief Default constructor
     */
    CNNNetworkIterator() = default;
    /**
     * @brief Constructor. Creates an iterator for specified CNNNetwork instance.
     * @param network Network to iterate. Make sure the network object is not destroyed before iterator goes out of scope.
     */
    explicit CNNNetworkIterator(ICNNNetwork * network) {
        InputsDataMap inputs;
        network->getInputsInfo(inputs);
        if (!inputs.empty()) {
            auto & nextLayers = inputs.begin()->second->getInputData()->getInputTo();
            if (!nextLayers.empty()) {
                currentLayer = nextLayers.begin()->second;
                nextLayersTovisit.push_back(currentLayer);
                visited.insert(currentLayer.get());
            }
        }
    }

    /**
     * @brief Performs pre-increment 
     * @return This CNNNetworkIterator instance
     */
    CNNNetworkIterator &operator++() {
        currentLayer = next();
        return *this;
    }

    /**
     * @brief Performs post-increment.
     * Implementation does not follow the std interface since only move semantics is used
     */
    void operator++(int) {
        currentLayer = next();
    }

    /**
     * @brief Checks if the given iterator is not equal to this one
     * @param that Iterator to compare with
     * @return true if the given iterator is not equal to this one, false - otherwise
     */
    bool operator!=(const CNNNetworkIterator &that) const {
        return !operator==(that);
    }

    /**
     * @brief Gets const layer pointer referenced by this iterator
     */
    const CNNLayerPtr &operator*() const {
        if (nullptr == currentLayer) {
            THROW_IE_EXCEPTION << "iterator of ouf bound";
        }
        return currentLayer;
    }

    /**
     * @brief Gets a layer pointer referenced by this iterator
     */
    CNNLayerPtr &operator*() {
        if (nullptr == currentLayer) {
            THROW_IE_EXCEPTION << "iterator out of bound";
        }
        return currentLayer;
    }
    /**
     * @brief Compares the given iterator with this one
     * @param that Iterator to compare with
     * @return true if the given iterator is equal to this one, false - otherwise
     */
    bool operator==(const CNNNetworkIterator &that) const {
        return network == that.network && currentLayer == that.currentLayer;
    }

 private:
    /**
     * @brief implementation based on BFS
     */
    CNNLayerPtr next() {
        if (nextLayersTovisit.empty()) {
            return nullptr;
        }

        auto nextLayer = nextLayersTovisit.front();
        nextLayersTovisit.pop_front();

        // visit child that not visited
        for (auto && output : nextLayer->outData) {
            for (auto && child : output->getInputTo()) {
                if (visited.find(child.second.get()) == visited.end()) {
                    nextLayersTovisit.push_back(child.second);
                    visited.insert(child.second.get());
                }
            }
        }

        // visit parents
        for (auto && parent  : nextLayer->insData) {
            auto parentLayer = parent.lock()->getCreatorLayer().lock();
            if (parentLayer && visited.find(parentLayer.get()) == visited.end()) {
                nextLayersTovisit.push_back(parentLayer);
                visited.insert(parentLayer.get());
            }
        }

        return nextLayersTovisit.empty() ? nullptr : nextLayersTovisit.front();
    }
};
}  // namespace details
}  // namespace InferenceEngine
