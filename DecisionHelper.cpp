#include "DecisionHelper.h"
#include <iostream>
#include <numeric>

DecisionHelper::DecisionHelper(const std::vector<std::string>& criteria, const std::vector<double>& weights)
    : criteria(criteria), weights(weights), score(0.0) {}

void DecisionHelper::setCriteriaValues(const std::vector<double>& values) {
    if (values.size() != criteria.size()) {
        throw std::invalid_argument("Values size does not match criteria size");
    }
    this->values = values;
}

void DecisionHelper::calculateScore() {
    score = std::inner_product(values.begin(), values.end(), weights.begin(), 0.0);
}

std::string DecisionHelper::makeDecision(double threshold) {
    return score >= threshold ? "y aller" : "ne pas y aller";
}
main.cpp