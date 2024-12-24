#ifndef DECISIONHELPER_H
#define DECISIONHELPER_H

#include <string>
#include <vector>

class DecisionHelper {
public:
    DecisionHelper(const std::vector<std::string>& criteria, const std::vector<double>& weights);
    void setCriteriaValues(const std::vector<double>& values);
    void calculateScore();
    std::string makeDecision(double threshold);

private:
    std::vector<std::string> criteria;
    std::vector<double> weights;
    std::vector<double> values;
    double score;
};

#endif // DECISIONHELPER_H