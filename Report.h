#ifndef REPORT_H
#define REPORT_H

#include <string>

#include "DecisionHelper.h"

// Rendu texte (terminal) ou Markdown (md=true) des analyses.
namespace report {
std::string full(const DecisionHelper& h, const MonteCarloResult& mc, bool md = false);
std::string ranking(const DecisionHelper& h, const MonteCarloResult& mc, bool md = false);
std::string verdict(const DecisionHelper& h, const MonteCarloResult& mc);
std::string why(const DecisionHelper& h);              // contributions gagnant vs second
std::string strategies(const DecisionHelper& h);       // optimiste / prudent / équilibré / sans regret
std::string sensitivity(const DecisionHelper& h, bool md = false);
std::string devilsAdvocate(const DecisionHelper& h);
std::string preMortem(const DecisionHelper& h);
std::string weights(const DecisionHelper& h);
std::string bar(double value, double max, int width = 20);
}  // namespace report

#endif  // REPORT_H
