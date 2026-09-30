#ifndef STORAGE_H
#define STORAGE_H

#include <map>
#include <string>
#include <vector>

#include "DecisionHelper.h"

// Sauvegarde / chargement d'un problème (format texte lisible et éditable à la main) :
//   title=Quel logement ?
//   criterion=nom|poids|benefit|cost|veto ou -
//   option=nom|note1;note2;...
bool saveModel(const DecisionHelper& h, const std::string& path, std::string& err);
bool loadModel(DecisionHelper& h, const std::string& path, std::string& err);

// Journal des décisions passées : sert à apprendre ce qui rend réellement satisfait.
struct HistoryRecord {
    std::string date, title, option;
    double satisfaction = 0.0;                // 0..10, saisie a posteriori
    std::map<std::string, double> ratings;    // notes normalisées (0..10, plus haut = mieux) par critère
};

bool appendHistory(const std::string& path, const HistoryRecord& r);
std::vector<HistoryRecord> loadHistory(const std::string& path);
HistoryRecord makeRecord(const DecisionHelper& h, size_t option, double satisfaction);

struct LearnedWeights {
    bool ok = false;
    std::string note;
    size_t samples = 0;
    std::vector<double> weights;      // somme = 1, même ordre que les critères du modèle
    std::vector<double> correlation;  // corrélation note/satisfaction, par critère
};

// Régression ridge de la satisfaction sur les notes : révèle les préférences réelles.
LearnedWeights learnWeights(const DecisionHelper& h, const std::vector<HistoryRecord>& history);

#endif  // STORAGE_H
