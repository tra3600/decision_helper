#ifndef DECISIONHELPER_H
#define DECISIONHELPER_H

#include <cstddef>
#include <string>
#include <vector>

// Un critère : les notes vont de 0 à 10.
// benefit=true : plus c'est haut, mieux c'est. benefit=false (coût) : plus c'est bas, mieux c'est.
// Veto : seuil éliminatoire (note brute minimale pour un bénéfice, maximale pour un coût).
struct Criterion {
    std::string name;
    double weight = 1.0;
    bool benefit = true;
    bool hasVeto = false;
    double veto = 0.0;
};

struct Option {
    std::string name;
    std::vector<double> values;  // notes brutes 0..10, une par critère
};

struct OptionScore {
    size_t index = 0;
    double score = 0.0;   // somme pondérée, 0..100
    double topsis = 0.0;  // proximité à l'idéal, 0..1
    bool eliminated = false;
    std::string vetoReason;
};

struct MonteCarloResult {
    int runs = 0;
    std::vector<double> winProb, mean, p05, p95, avgRank;  // par option
};

struct WeightFlip {  // « si le poids du critère passe de from à to, le gagnant change »
    bool exists = false;
    size_t criterion = 0, rival = 0;
    double from = 0.0, to = 0.0;
};

struct ValueFlip {  // écart cumulé (points 0..10) sur un critère pour renverser le classement
    bool exists = false;
    size_t criterion = 0, rival = 0;
    double delta = 0.0;
};

struct Sensitivity {
    size_t winner = 0;
    std::vector<WeightFlip> weightFlips;  // un par critère
    std::vector<ValueFlip> valueFlips;    // un par critère
};

struct AhpResult {
    std::vector<double> weights;
    double lambdaMax = 0.0, ci = 0.0, cr = 0.0;
    bool consistent = true;
};

class DecisionHelper {
public:
    DecisionHelper() = default;
    // Compatibilité avec la première version (un seul choix « y aller / ne pas y aller »).
    DecisionHelper(const std::vector<std::string>& criteria, const std::vector<double>& weights);
    void setCriteriaValues(const std::vector<double>& values);
    void calculateScore();
    std::string makeDecision(double threshold);

    // Modèle
    void setTitle(const std::string& t) { title = t; }
    const std::string& getTitle() const { return title; }
    size_t addCriterion(const Criterion& c);
    void removeCriterion(size_t i);
    size_t addOption(const std::string& name, const std::vector<double>& values);
    void removeOption(size_t i);
    void setValue(size_t option, size_t criterion, double v);
    void setWeight(size_t criterion, double w);
    const std::vector<Criterion>& getCriteria() const { return criteria; }
    const std::vector<Option>& getOptions() const { return options; }
    std::vector<double> normalizedWeights() const;   // somme = 1
    double normalizedValue(size_t o, size_t c) const;  // 0..10, « plus haut = mieux »
    std::string vetoReason(size_t o) const;            // vide si l'option est admissible
    double scoreOf(size_t o, const std::vector<double>& w) const;  // 0..10

    // Analyses
    std::vector<OptionScore> rank() const;  // trié : admissibles par score décroissant, puis éliminées
    MonteCarloResult monteCarlo(int runs = 5000, double weightSigma = 0.35, double valueSigma = 0.8,
                                unsigned seed = 42) const;
    double probabilityAbove(size_t o, double threshold, int runs = 5000, double weightSigma = 0.35,
                            double valueSigma = 0.8, unsigned seed = 42) const;
    Sensitivity sensitivity() const;
    static AhpResult ahp(const std::vector<std::vector<double>>& pairwise);

private:
    std::string title;
    std::vector<Criterion> criteria;
    std::vector<Option> options;
    double score = 0.0;
};

#endif  // DECISIONHELPER_H
