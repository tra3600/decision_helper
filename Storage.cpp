#include "Storage.h"

#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>

namespace {
std::string clean(std::string s) {
    for (auto& c : s)
        if (c == '|' || c == ';' || c == '\n' || c == '\t' || c == '\r') c = ' ';
    return s;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(s);
    while (std::getline(in, cur, sep)) out.push_back(cur);
    if (!s.empty() && s.back() == sep) out.push_back("");
    return out;
}

bool toDouble(const std::string& s, double& v) {
    try {
        size_t used = 0;
        v = std::stod(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}
}  // namespace

bool saveModel(const DecisionHelper& h, const std::string& path, std::string& err) {
    std::ofstream f(path);
    if (!f) { err = "impossible d'écrire " + path; return false; }
    f << "# decision_helper v2\n";
    f << "title=" << clean(h.getTitle()) << "\n";
    for (auto& c : h.getCriteria()) {
        f << "criterion=" << clean(c.name) << "|" << c.weight << "|" << (c.benefit ? "benefit" : "cost") << "|";
        if (c.hasVeto) f << c.veto; else f << "-";
        f << "\n";
    }
    for (auto& o : h.getOptions()) {
        f << "option=" << clean(o.name) << "|";
        for (size_t i = 0; i < o.values.size(); ++i) f << (i ? ";" : "") << o.values[i];
        f << "\n";
    }
    return true;
}

bool loadModel(DecisionHelper& h, const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "fichier introuvable : " + path; return false; }
    DecisionHelper fresh;
    std::string line;
    int ln = 0;
    while (std::getline(f, line)) {
        ++ln;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) { err = "ligne " + std::to_string(ln) + " invalide"; return false; }
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        auto p = split(val, '|');
        if (key == "title") {
            fresh.setTitle(val);
        } else if (key == "criterion") {
            Criterion c;
            if (p.size() < 3 || !toDouble(p[1], c.weight)) { err = "critère invalide ligne " + std::to_string(ln); return false; }
            c.name = p[0];
            c.benefit = p[2] != "cost";
            if (p.size() > 3 && p[3] != "-") {
                if (!toDouble(p[3], c.veto)) { err = "veto invalide ligne " + std::to_string(ln); return false; }
                c.hasVeto = true;
            }
            fresh.addCriterion(c);
        } else if (key == "option") {
            std::vector<double> vals;
            if (p.size() < 2) { err = "option invalide ligne " + std::to_string(ln); return false; }
            for (auto& s : split(p[1], ';')) {
                double v;
                if (!toDouble(s, v)) { err = "note invalide ligne " + std::to_string(ln); return false; }
                vals.push_back(v);
            }
            if (vals.size() != fresh.getCriteria().size()) {
                err = "ligne " + std::to_string(ln) + " : nombre de notes incorrect";
                return false;
            }
            fresh.addOption(p[0], vals);
        } else {
            err = "clé inconnue « " + key + " » ligne " + std::to_string(ln);
            return false;
        }
    }
    h = fresh;
    return true;
}

bool appendHistory(const std::string& path, const HistoryRecord& r) {
    std::ofstream f(path, std::ios::app);
    if (!f) return false;
    f << clean(r.date) << "\t" << clean(r.title) << "\t" << clean(r.option) << "\t" << r.satisfaction << "\t";
    bool first = true;
    for (auto& kv : r.ratings) {
        f << (first ? "" : ";") << clean(kv.first) << "|" << kv.second;
        first = false;
    }
    f << "\n";
    return true;
}

std::vector<HistoryRecord> loadHistory(const std::string& path) {
    std::vector<HistoryRecord> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        auto t = split(line, '\t');
        if (t.size() < 5) continue;
        HistoryRecord r;
        r.date = t[0]; r.title = t[1]; r.option = t[2];
        if (!toDouble(t[3], r.satisfaction)) continue;
        for (auto& kv : split(t[4], ';')) {
            auto p = split(kv, '|');
            double v;
            if (p.size() == 2 && toDouble(p[1], v)) r.ratings[p[0]] = v;
        }
        out.push_back(r);
    }
    return out;
}

HistoryRecord makeRecord(const DecisionHelper& h, size_t option, double satisfaction) {
    HistoryRecord r;
    char buf[16];
    std::time_t t = std::time(nullptr);
    std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&t));
    r.date = buf;
    r.title = h.getTitle();
    r.option = h.getOptions().at(option).name;
    r.satisfaction = satisfaction;
    for (size_t c = 0; c < h.getCriteria().size(); ++c) r.ratings[h.getCriteria()[c].name] = h.normalizedValue(option, c);
    return r;
}

LearnedWeights learnWeights(const DecisionHelper& h, const std::vector<HistoryRecord>& history) {
    LearnedWeights res;
    const auto& cr = h.getCriteria();
    const size_t k = cr.size();
    std::vector<const HistoryRecord*> use;
    for (auto& r : history) {
        bool all = true;
        for (auto& c : cr) all = all && r.ratings.count(c.name);
        if (all) use.push_back(&r);
    }
    res.samples = use.size();
    if (k == 0 || use.size() < 4) {
        res.note = "il faut au moins 4 décisions passées couvrant les mêmes critères (" + std::to_string(use.size()) + " trouvées)";
        return res;
    }
    const size_t n = use.size();
    std::vector<std::vector<double>> X(n, std::vector<double>(k));
    std::vector<double> y(n);
    for (size_t i = 0; i < n; ++i) {
        y[i] = use[i]->satisfaction;
        for (size_t j = 0; j < k; ++j) X[i][j] = use[i]->ratings.at(cr[j].name);
    }
    double ym = 0;
    for (double v : y) ym += v;
    ym /= n;
    for (double& v : y) v -= ym;
    res.correlation.assign(k, 0.0);
    for (size_t j = 0; j < k; ++j) {  // centrer / réduire les colonnes
        double m = 0, s = 0;
        for (size_t i = 0; i < n; ++i) m += X[i][j];
        m /= n;
        for (size_t i = 0; i < n; ++i) s += (X[i][j] - m) * (X[i][j] - m);
        s = std::sqrt(s / n);
        for (size_t i = 0; i < n; ++i) X[i][j] = s > 1e-9 ? (X[i][j] - m) / s : 0.0;
        double cov = 0, sy = 0;
        for (size_t i = 0; i < n; ++i) { cov += X[i][j] * y[i]; sy += y[i] * y[i]; }
        res.correlation[j] = (s > 1e-9 && sy > 1e-12) ? cov / (n * std::sqrt(sy / n)) : 0.0;
    }
    // (XᵀX + λnI) β = Xᵀy
    const double lambda = 0.3 * n;
    std::vector<std::vector<double>> A(k, std::vector<double>(k + 1, 0.0));
    for (size_t a = 0; a < k; ++a) {
        for (size_t b = 0; b < k; ++b)
            for (size_t i = 0; i < n; ++i) A[a][b] += X[i][a] * X[i][b];
        A[a][a] += lambda;
        for (size_t i = 0; i < n; ++i) A[a][k] += X[i][a] * y[i];
    }
    for (size_t col = 0; col < k; ++col) {
        size_t piv = col;
        for (size_t r = col + 1; r < k; ++r)
            if (std::fabs(A[r][col]) > std::fabs(A[piv][col])) piv = r;
        std::swap(A[col], A[piv]);
        for (size_t r = 0; r < k; ++r) {
            if (r == col) continue;
            double f = A[r][col] / A[col][col];
            for (size_t c2 = col; c2 <= k; ++c2) A[r][c2] -= f * A[col][c2];
        }
    }
    std::vector<double> beta(k), w(k);
    double sum = 0;
    for (size_t j = 0; j < k; ++j) {
        beta[j] = A[j][k] / A[j][j];
        w[j] = std::max(0.0, beta[j]);
        sum += w[j];
    }
    if (sum <= 1e-12) {
        res.note = "aucune relation exploitable entre les notes passées et votre satisfaction";
        return res;
    }
    for (double& v : w) v /= sum;
    res.weights = w;
    res.ok = true;
    res.note = "estimé sur " + std::to_string(n) + " décisions";
    return res;
}
