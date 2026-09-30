#include "DecisionHelper.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <stdexcept>

namespace {
double clamp10(double v) { return std::min(10.0, std::max(0.0, v)); }
const double kNeutral = 5.0;
}  // namespace

DecisionHelper::DecisionHelper(const std::vector<std::string>& names, const std::vector<double>& weights) {
    if (names.size() != weights.size()) throw std::invalid_argument("Criteres et poids de tailles differentes");
    for (size_t i = 0; i < names.size(); ++i) {
        Criterion c;
        c.name = names[i];
        c.weight = weights[i];
        criteria.push_back(c);
    }
}

void DecisionHelper::setCriteriaValues(const std::vector<double>& values) {
    if (values.size() != criteria.size()) {
        throw std::invalid_argument("Values size does not match criteria size");
    }
    if (options.empty()) options.push_back({"Option unique", values});
    else options[0].values = values;
}

void DecisionHelper::calculateScore() {
    score = options.empty() ? 0.0 : scoreOf(0, normalizedWeights());
}

std::string DecisionHelper::makeDecision(double threshold) {
    return score >= threshold ? "y aller" : "ne pas y aller";
}

size_t DecisionHelper::addCriterion(const Criterion& c) {
    criteria.push_back(c);
    for (auto& o : options) o.values.push_back(kNeutral);
    return criteria.size() - 1;
}

void DecisionHelper::removeCriterion(size_t i) {
    if (i >= criteria.size()) throw std::out_of_range("critere inconnu");
    criteria.erase(criteria.begin() + i);
    for (auto& o : options) o.values.erase(o.values.begin() + i);
}

size_t DecisionHelper::addOption(const std::string& name, const std::vector<double>& values) {
    if (values.size() != criteria.size()) throw std::invalid_argument("nombre de notes != nombre de criteres");
    Option o;
    o.name = name;
    for (double v : values) o.values.push_back(clamp10(v));
    options.push_back(o);
    return options.size() - 1;
}

void DecisionHelper::removeOption(size_t i) {
    if (i >= options.size()) throw std::out_of_range("option inconnue");
    options.erase(options.begin() + i);
}

void DecisionHelper::setValue(size_t o, size_t c, double v) {
    if (o >= options.size() || c >= criteria.size()) throw std::out_of_range("indice invalide");
    options[o].values[c] = clamp10(v);
}

void DecisionHelper::setWeight(size_t c, double w) {
    if (c >= criteria.size()) throw std::out_of_range("critere inconnu");
    criteria[c].weight = std::max(0.0, w);
}

std::vector<double> DecisionHelper::normalizedWeights() const {
    std::vector<double> w(criteria.size(), 0.0);
    double sum = 0;
    for (auto& c : criteria) sum += std::max(0.0, c.weight);
    for (size_t i = 0; i < w.size(); ++i)
        w[i] = sum > 0 ? std::max(0.0, criteria[i].weight) / sum : 1.0 / criteria.size();
    return w;
}

double DecisionHelper::normalizedValue(size_t o, size_t c) const {
    double v = clamp10(options[o].values[c]);
    return criteria[c].benefit ? v : 10.0 - v;
}

std::string DecisionHelper::vetoReason(size_t o) const {
    for (size_t c = 0; c < criteria.size(); ++c) {
        if (!criteria[c].hasVeto) continue;
        double v = options[o].values[c];
        std::ostringstream s;
        if (criteria[c].benefit && v < criteria[c].veto) {
            s << criteria[c].name << " trop faible (" << v << " < " << criteria[c].veto << ")";
            return s.str();
        }
        if (!criteria[c].benefit && v > criteria[c].veto) {
            s << criteria[c].name << " trop élevé (" << v << " > " << criteria[c].veto << ")";
            return s.str();
        }
    }
    return "";
}

double DecisionHelper::scoreOf(size_t o, const std::vector<double>& w) const {
    double s = 0;
    for (size_t c = 0; c < criteria.size(); ++c) s += w[c] * normalizedValue(o, c);
    return s;
}

std::vector<OptionScore> DecisionHelper::rank() const {
    const size_t n = options.size(), m = criteria.size();
    std::vector<OptionScore> res(n);
    auto w = normalizedWeights();
    std::vector<size_t> alive;
    for (size_t i = 0; i < n; ++i) {
        res[i].index = i;
        res[i].score = scoreOf(i, w) * 10.0;
        res[i].vetoReason = vetoReason(i);
        res[i].eliminated = !res[i].vetoReason.empty();
        if (!res[i].eliminated) alive.push_back(i);
    }
    // TOPSIS sur les options admissibles
    std::vector<double> best(m, -1e9), worst(m, 1e9), denom(m, 0.0);
    for (size_t c = 0; c < m; ++c) {
        for (size_t i : alive) denom[c] += normalizedValue(i, c) * normalizedValue(i, c);
        denom[c] = std::sqrt(denom[c]);
        for (size_t i : alive) {
            double y = denom[c] > 0 ? w[c] * normalizedValue(i, c) / denom[c] : 0.0;
            best[c] = std::max(best[c], y);
            worst[c] = std::min(worst[c], y);
        }
    }
    for (size_t i : alive) {
        double dp = 0, dm = 0;
        for (size_t c = 0; c < m; ++c) {
            double y = denom[c] > 0 ? w[c] * normalizedValue(i, c) / denom[c] : 0.0;
            dp += (y - best[c]) * (y - best[c]);
            dm += (y - worst[c]) * (y - worst[c]);
        }
        dp = std::sqrt(dp);
        dm = std::sqrt(dm);
        res[i].topsis = (dp + dm) > 1e-12 ? dm / (dp + dm) : 0.5;
    }
    std::stable_sort(res.begin(), res.end(), [](const OptionScore& a, const OptionScore& b) {
        if (a.eliminated != b.eliminated) return !a.eliminated;
        return a.score > b.score;
    });
    return res;
}

MonteCarloResult DecisionHelper::monteCarlo(int runs, double weightSigma, double valueSigma,
                                            unsigned seed) const {
    const size_t n = options.size(), m = criteria.size();
    MonteCarloResult r;
    r.runs = runs;
    r.winProb.assign(n, 0.0);
    r.mean.assign(n, 0.0);
    r.p05.assign(n, 0.0);
    r.p95.assign(n, 0.0);
    r.avgRank.assign(n, 0.0);
    if (n == 0 || m == 0 || runs <= 0) return r;

    std::vector<size_t> alive;
    for (size_t i = 0; i < n; ++i)
        if (vetoReason(i).empty()) alive.push_back(i);
    if (alive.empty()) return r;

    std::mt19937 gen(seed);
    std::normal_distribution<double> N(0.0, 1.0);
    auto base = normalizedWeights();
    std::vector<std::vector<double>> samples(n);
    for (auto& s : samples) s.reserve(runs);

    for (int k = 0; k < runs; ++k) {
        std::vector<double> w(m);
        double sum = 0;
        for (size_t c = 0; c < m; ++c) {
            w[c] = base[c] * std::exp(weightSigma * N(gen) - 0.5 * weightSigma * weightSigma);
            sum += w[c];
        }
        for (auto& x : w) x = sum > 0 ? x / sum : 1.0 / m;
        std::vector<double> sc(n, -1e9);
        for (size_t i : alive) {
            double s = 0;
            for (size_t c = 0; c < m; ++c) s += w[c] * clamp10(normalizedValue(i, c) + valueSigma * N(gen));
            sc[i] = s * 10.0;
            samples[i].push_back(sc[i]);
        }
        std::vector<size_t> order = alive;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return sc[a] > sc[b]; });
        r.winProb[order[0]] += 1.0;
        for (size_t p = 0; p < order.size(); ++p) r.avgRank[order[p]] += p + 1;
    }
    for (size_t i : alive) {
        auto& s = samples[i];
        std::sort(s.begin(), s.end());
        double tot = 0;
        for (double v : s) tot += v;
        r.mean[i] = tot / s.size();
        r.p05[i] = s[static_cast<size_t>(0.05 * (s.size() - 1))];
        r.p95[i] = s[static_cast<size_t>(0.95 * (s.size() - 1))];
        r.winProb[i] /= runs;
        r.avgRank[i] /= runs;
    }
    return r;
}

double DecisionHelper::probabilityAbove(size_t o, double threshold, int runs, double weightSigma,
                                        double valueSigma, unsigned seed) const {
    const size_t m = criteria.size();
    if (o >= options.size() || m == 0 || runs <= 0) return 0.0;
    std::mt19937 gen(seed);
    std::normal_distribution<double> N(0.0, 1.0);
    auto base = normalizedWeights();
    int hits = 0;
    for (int k = 0; k < runs; ++k) {
        std::vector<double> w(m);
        double sum = 0;
        for (size_t c = 0; c < m; ++c) {
            w[c] = base[c] * std::exp(weightSigma * N(gen) - 0.5 * weightSigma * weightSigma);
            sum += w[c];
        }
        double s = 0;
        for (size_t c = 0; c < m; ++c)
            s += (sum > 0 ? w[c] / sum : 1.0 / m) * clamp10(normalizedValue(o, c) + valueSigma * N(gen));
        if (s >= threshold) ++hits;
    }
    return static_cast<double>(hits) / runs;
}

Sensitivity DecisionHelper::sensitivity() const {
    const size_t m = criteria.size();
    Sensitivity res;
    res.weightFlips.assign(m, WeightFlip());
    res.valueFlips.assign(m, ValueFlip());
    auto ranking = rank();
    std::vector<size_t> alive;
    for (auto& r : ranking)
        if (!r.eliminated) alive.push_back(r.index);
    if (alive.size() < 2) return res;
    const size_t win = alive[0];
    res.winner = win;
    auto w = normalizedWeights();
    const double Sw = scoreOf(win, w);

    for (size_t j = 0; j < m; ++j) {
        double bestDist = 1e9, bestDelta = 1e9;
        for (size_t k = 1; k < alive.size(); ++k) {
            size_t r = alive[k];
            // Bascule du poids : S_i(t) = t*n_ij + (1-t)*R_i, les autres poids restant proportionnels.
            if (1.0 - w[j] > 1e-9) {
                double Rw = (Sw - w[j] * normalizedValue(win, j)) / (1.0 - w[j]);
                double Rr = (scoreOf(r, w) - w[j] * normalizedValue(r, j)) / (1.0 - w[j]);
                double a = (normalizedValue(win, j) - Rw) - (normalizedValue(r, j) - Rr);
                if (std::fabs(a) > 1e-12) {
                    double t = (Rr - Rw) / a;
                    if (t >= 0.0 && t <= 1.0 && std::fabs(t - w[j]) < bestDist) {
                        bestDist = std::fabs(t - w[j]);
                        res.weightFlips[j] = {true, j, r, w[j], t};
                    }
                }
            }
            // Bascule d'une note : écart cumulé nécessaire sur ce critère.
            if (w[j] > 1e-9) {
                double delta = (Sw - scoreOf(r, w)) / w[j];
                double room = normalizedValue(win, j) + (10.0 - normalizedValue(r, j));
                if (delta <= room && delta < bestDelta) {
                    bestDelta = delta;
                    res.valueFlips[j] = {true, j, r, delta};
                }
            }
        }
    }
    return res;
}

AhpResult DecisionHelper::ahp(const std::vector<std::vector<double>>& a) {
    const size_t n = a.size();
    AhpResult r;
    if (n == 0) return r;
    for (auto& row : a)
        if (row.size() != n) throw std::invalid_argument("matrice AHP non carree");
    std::vector<double> w(n, 1.0 / n), v(n);
    for (int it = 0; it < 200; ++it) {
        double sum = 0;
        for (size_t i = 0; i < n; ++i) {
            v[i] = 0;
            for (size_t j = 0; j < n; ++j) v[i] += a[i][j] * w[j];
            sum += v[i];
        }
        for (size_t i = 0; i < n; ++i) w[i] = v[i] / sum;
    }
    double lam = 0;
    for (size_t i = 0; i < n; ++i) {
        double s = 0;
        for (size_t j = 0; j < n; ++j) s += a[i][j] * w[j];
        lam += s / w[i];
    }
    r.weights = w;
    r.lambdaMax = lam / n;
    static const double RI[] = {0, 0, 0.58, 0.90, 1.12, 1.24, 1.32, 1.41, 1.45, 1.49};
    if (n > 2) {
        r.ci = (r.lambdaMax - n) / (n - 1);
        r.cr = r.ci / RI[std::min<size_t>(n - 1, 9)];
    }
    r.consistent = r.cr < 0.10;
    return r;
}
