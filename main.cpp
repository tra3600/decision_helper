
#include <iostream>
#include "DecisionHelper.h"

int main() {
    std::vector<std::string> criteria = {"coût", "temps", "bénéfice"};
    std::vector<double> weights = {0.4, 0.3, 0.3};

    DecisionHelper decisionHelper(criteria, weights);

    std::vector<double> values(criteria.size());

    std::cout << "Entrez les valeurs pour les critères suivants:" << std::endl;
    for (size_t i = 0; i < criteria.size(); ++i) {
        std::cout << criteria[i] << ": ";
        std::cin >> values[i];
    }

    decisionHelper.setCriteriaValues(values);
    decisionHelper.calculateScore();

    double threshold;
    std::cout << "Entrez le seuil de décision: ";
    std::cin >> threshold;

    std::string decision = decisionHelper.makeDecision(threshold);
    std::cout << "La décision est: " << decision << std::endl;

    return 0;
}