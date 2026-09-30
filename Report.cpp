#include "Report.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace report {
namespace {

std::string fmt(double v, int prec = 1) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(prec) << v;
    return s.str();
}

std::string pct(double v) { return fmt(v * 100.0, 0) + " %"; }

size_t glyphs(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::string pad(const std::string& s, size_t width) {
    size_t g = glyphs(s);
    return g >= width ? s : s + std::string(width - g, ' ');
}

std::string head(const std::string& t, bool md) { return md ? "\n## " + t + "\n\n" : "\n── " + t + " ──\n"; }

std::vector<size_t> aliveByScore(const DecisionHelper& h) {
    std::vector<size_t> a;
    for (auto& r : h.rank())
        if (!r.eliminated) a.push_back(r.index);
    return a;
}

}  // namespace

std::string bar(double value, double max, int width) {
    if (max <= 0) max = 1;
    int filled = static_cast<int>(std::lround(std::min(1.0, std::max(0.0, value / max)) * width));
    std::string s;
    for (int i = 0; i < width; ++i) s += i < filled ? "█" : "░";
    return s;
}

std::string weights(const DecisionHelper& h) {
    std::ostringstream o;
    auto w = h.normalizedWeights();
    auto& cr = h.getCriteria();
    for (size_t i = 0; i < cr.size(); ++i) {
        o << "  " << pad(cr[i].name, 18) << bar(w[i], 1.0, 20) << " " << pad(pct(w[i]), 6)
          << (cr[i].benefit ? " (bénéfice)" : " (coût)");
        if (cr[i].hasVeto) o << " veto " << (cr[i].benefit ? "< " : "> ") << cr[i].veto;
        o << "\n";
    }
    return o.str();
}

std::string ranking(const DecisionHelper& h, const MonteCarloResult& mc, bool md) {
    std::ostringstream o;
    auto rk = h.rank();
    auto& op = h.getOptions();
    if (md) {
        o << "| # | Option | Score /100 | TOPSIS | Chance de gagner | Intervalle 90 % |\n|---|---|---|---|---|---|\n";
    }
    int pos = 1;
    for (auto& r : rk) {
        if (r.eliminated) {
            if (md) o << "| ✗ | " << op[r.index].name << " | - | - | éliminée | " << r.vetoReason << " |\n";
            else o << "  ✗  " << pad(op[r.index].name, 20) << " éliminée : " << r.vetoReason << "\n";
            continue;
        }
        size_t i = r.index;
        std::string ic = "[" + fmt(mc.p05[i], 0) + " – " + fmt(mc.p95[i], 0) + "]";
        if (md) {
            o << "| " << pos << " | " << op[i].name << " | " << fmt(r.score) << " | " << fmt(r.topsis, 2) << " | "
              << pct(mc.winProb[i]) << " | " << ic << " |\n";
        } else {
            o << "  " << pos << ".  " << pad(op[i].name, 20) << bar(r.score, 100, 20) << " " << pad(fmt(r.score), 5)
              << " TOPSIS " << fmt(r.topsis, 2) << "  gagne dans " << pad(pct(mc.winProb[i]), 5) << " des cas  IC90 " << ic
              << "\n";
        }
        ++pos;
    }
    return o.str();
}

std::string verdict(const DecisionHelper& h, const MonteCarloResult& mc) {
    auto rk = h.rank();
    auto& op = h.getOptions();
    if (rk.empty()) return "Aucune option à comparer.\n";
    if (rk[0].eliminated) return "Toutes les options sont éliminées par un veto : assouplissez un critère ou cherchez d'autres options.\n";
    size_t w = rk[0].index;
    std::ostringstream o;
    double p = mc.winProb[w];
    if (rk.size() == 1 || rk[1].eliminated) {
        o << "« " << op[w].name << " » est la seule option admissible.\n";
        return o.str();
    }
    size_t s = rk[1].index;
    o << "Recommandation : « " << op[w].name << " » (" << fmt(rk[0].score) << "/100, " << pct(p)
      << " de chances d'être la meilleure malgré l'incertitude).\n";
    if (p >= 0.80) o << "→ Décision CLAIRE : le résultat résiste aux variations de poids et de notes.\n";
    else if (p >= 0.55)
        o << "→ Avantage net mais FRAGILE face à « " << op[s].name << " » : vérifiez les critères sensibles ci-dessous.\n";
    else
        o << "→ Quasi-égalité avec « " << op[s].name << " » : les chiffres ne tranchent pas. Collectez une information\n"
          << "  décisive ou utilisez le test pile-ou-face (menu) pour sonder votre intuition.\n";
    return o.str();
}

std::string why(const DecisionHelper& h) {
    auto a = aliveByScore(h);
    if (a.size() < 2) return "  (une seule option admissible)\n";
    auto w = h.normalizedWeights();
    auto& cr = h.getCriteria();
    auto& op = h.getOptions();
    size_t win = a[0], sec = a[1];
    struct C { size_t c; double d; };
    std::vector<C> diffs;
    for (size_t c = 0; c < cr.size(); ++c)
        diffs.push_back({c, w[c] * (h.normalizedValue(win, c) - h.normalizedValue(sec, c)) * 10.0});
    std::sort(diffs.begin(), diffs.end(), [](const C& x, const C& y) { return x.d > y.d; });
    std::ostringstream o;
    o << "  Points d'écart de « " << op[win].name << " » sur « " << op[sec].name << " » par critère :\n";
    for (auto& d : diffs)
        o << "   " << (d.d >= 0 ? "+" : "") << pad(fmt(d.d), 6) << pad(cr[d.c].name, 18) << bar(std::fabs(d.d), 30, 12)
          << (d.d >= 0 ? " avantage" : " désavantage") << "\n";
    return o.str();
}

std::string strategies(const DecisionHelper& h) {
    auto a = aliveByScore(h);
    if (a.empty()) return "";
    auto w = h.normalizedWeights();
    auto& op = h.getOptions();
    const size_t m = h.getCriteria().size();
    size_t opt = a[0], pru = a[0], reg = a[0];
    double bestMax = -1, bestMin = -1, bestRegret = 1e9;
    for (size_t i : a) {
        double mx = -1, mn = 11;
        double worstRegret = 0;
        for (size_t c = 0; c < m; ++c) {
            mx = std::max(mx, h.normalizedValue(i, c));
            mn = std::min(mn, h.normalizedValue(i, c));
            double best = 0;
            for (size_t k : a) best = std::max(best, h.normalizedValue(k, c));
            worstRegret = std::max(worstRegret, w[c] * (best - h.normalizedValue(i, c)));
        }
        if (mx > bestMax + 1e-9) { bestMax = mx; opt = i; }
        if (mn > bestMin + 1e-9) { bestMin = mn; pru = i; }
        if (worstRegret < bestRegret - 1e-9) { bestRegret = worstRegret; reg = i; }
    }
    std::ostringstream o;
    o << "  Optimiste   (viser le meilleur atout)      → " << op[opt].name << "\n"
      << "  Prudent     (maximiser le pire critère)    → " << op[pru].name << "\n"
      << "  Équilibré   (somme pondérée)               → " << op[a[0]].name << "\n"
      << "  Sans regret (minimiser le regret maximal)  → " << op[reg].name << "\n";
    if (opt == pru && pru == a[0] && a[0] == reg) o << "  Toutes les philosophies convergent : choix très robuste.\n";
    return o.str();
}

std::string sensitivity(const DecisionHelper& h, bool md) {
    auto sens = h.sensitivity();
    auto& cr = h.getCriteria();
    auto& op = h.getOptions();
    std::ostringstream o;
    auto a = aliveByScore(h);
    if (a.size() < 2) return "  (au moins deux options admissibles sont nécessaires)\n";
    struct Row { double dist; std::string text; };
    std::vector<Row> rows;
    for (size_t j = 0; j < cr.size(); ++j) {
        auto& f = sens.weightFlips[j];
        if (f.exists) {
            rows.push_back({std::fabs(f.to - f.from),
                            "Si le poids de « " + cr[j].name + " » passe de " + pct(f.from) + " à " + pct(f.to) + ", « " +
                                op[f.rival].name + " » devient la meilleure option."});
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) { return x.dist < y.dist; });
    o << (md ? "**Points de bascule des poids** (du plus fragile au plus solide) :\n\n" : "  Points de bascule des poids :\n");
    if (rows.empty()) o << (md ? "- " : "   • ") << "Aucun poids seul ne renverse le classement : très robuste.\n";
    for (auto& r : rows) o << (md ? "- " : "   • ") << r.text << "\n";
    o << (md ? "\n**Notes décisives** :\n\n" : "  Notes décisives :\n");
    std::vector<Row> vr;
    for (size_t j = 0; j < cr.size(); ++j) {
        auto& f = sens.valueFlips[j];
        if (f.exists)
            vr.push_back({f.delta, "Un écart cumulé de " + fmt(f.delta) + " pt sur « " + cr[j].name + " » (gagnant en baisse ou « " +
                                       op[f.rival].name + " » en hausse) suffit à renverser le résultat."});
    }
    std::sort(vr.begin(), vr.end(), [](const Row& x, const Row& y) { return x.dist < y.dist; });
    if (vr.empty()) o << (md ? "- " : "   • ") << "Aucune note isolée ne peut renverser le classement.\n";
    for (size_t i = 0; i < vr.size() && i < 3; ++i) o << (md ? "- " : "   • ") << vr[i].text << "\n";
    return o.str();
}

std::string devilsAdvocate(const DecisionHelper& h) {
    auto a = aliveByScore(h);
    if (a.size() < 2) return "  Pas d'adversaire : une seule option admissible.\n";
    auto w = h.normalizedWeights();
    auto& cr = h.getCriteria();
    auto& op = h.getOptions();
    size_t win = a[0], sec = a[1];
    std::ostringstream o;
    o << "  Plaidoyer pour « " << op[sec].name << " » contre « " << op[win].name << " » :\n";
    struct C { size_t c; double d; };
    std::vector<C> v;
    for (size_t c = 0; c < cr.size(); ++c) v.push_back({c, w[c] * (h.normalizedValue(sec, c) - h.normalizedValue(win, c))});
    std::sort(v.begin(), v.end(), [](const C& x, const C& y) { return x.d > y.d; });
    int shown = 0;
    for (auto& x : v) {
        if (x.d <= 1e-9 || shown >= 3) break;
        o << "   • Sur « " << cr[x.c].name << " », cette option fait mieux (" << fmt(h.normalizedValue(sec, x.c)) << " contre "
          << fmt(h.normalizedValue(win, x.c)) << ") et ce critère pèse " << pct(w[x.c]) << ".\n";
        ++shown;
    }
    if (!shown) o << "   • Cette option ne bat « " << op[win].name << " » sur aucun critère : le gagnant domine.\n";
    auto sens = h.sensitivity();
    double bestDelta = 1e9;
    size_t bj = 0;
    for (size_t j = 0; j < cr.size(); ++j)
        if (sens.valueFlips[j].exists && sens.valueFlips[j].rival == sec && sens.valueFlips[j].delta < bestDelta) {
            bestDelta = sens.valueFlips[j].delta;
            bj = j;
        }
    if (bestDelta < 1e8)
        o << "   • Elle passerait devant si l'écart sur « " << cr[bj].name << " » se réduisait de " << fmt(bestDelta)
          << " point(s) : êtes-vous sûr de vos notes sur ce critère ?\n";
    return o.str();
}

std::string preMortem(const DecisionHelper& h) {
    auto a = aliveByScore(h);
    if (a.empty()) return "";
    auto w = h.normalizedWeights();
    auto& cr = h.getCriteria();
    auto& op = h.getOptions();
    size_t win = a[0];
    struct C { size_t c; double risk; };
    std::vector<C> v;
    for (size_t c = 0; c < cr.size(); ++c) v.push_back({c, w[c] * (10.0 - h.normalizedValue(win, c))});
    std::sort(v.begin(), v.end(), [](const C& x, const C& y) { return x.risk > y.risk; });
    std::ostringstream o;
    o << "  Imaginez dans 6 mois que « " << op[win].name << " » se soit révélé être une erreur. Causes probables :\n";
    int n = 0;
    for (auto& x : v) {
        if (n >= 3 || h.normalizedValue(win, x.c) >= 8.0) break;
        o << "   " << ++n << ". « " << cr[x.c].name << " » (note " << fmt(h.normalizedValue(win, x.c)) << "/10, poids "
          << pct(w[x.c]) << ") — que pouvez-vous faire dès maintenant pour limiter ce risque ?\n";
    }
    if (!n) o << "   Aucun point faible notable : le principal risque est une erreur d'estimation de vos notes.\n";
    return o.str();
}

std::string full(const DecisionHelper& h, const MonteCarloResult& mc, bool md) {
    std::ostringstream o;
    o << (md ? "# " : "═══ ") << (h.getTitle().empty() ? "Analyse de décision" : h.getTitle()) << (md ? "\n" : " ═══\n");
    o << head("Critères et poids", md);
    if (md) {
        auto w = h.normalizedWeights();
        o << "| Critère | Poids | Type | Veto |\n|---|---|---|---|\n";
        for (size_t i = 0; i < h.getCriteria().size(); ++i) {
            auto& c = h.getCriteria()[i];
            o << "| " << c.name << " | " << pct(w[i]) << " | " << (c.benefit ? "bénéfice" : "coût") << " | "
              << (c.hasVeto ? fmt(c.veto) : "-") << " |\n";
        }
    } else {
        o << weights(h);
    }
    o << head("Classement (incertitude simulée : " + std::to_string(mc.runs) + " scénarios)", md) << ranking(h, mc, md);
    o << head("Verdict", md) << verdict(h, mc);
    if (md) o << "\n```\n" << why(h) << "```\n";
    else o << head("Pourquoi ce classement ?", md) << why(h);
    o << head("Ce que choisiraient différentes philosophies", md);
    if (md) o << "\n```\n" << strategies(h) << "```\n"; else o << strategies(h);
    o << head("Sensibilité", md) << sensitivity(h, md);
    o << head("Avocat du diable", md);
    if (md) o << "\n```\n" << devilsAdvocate(h) << "```\n"; else o << devilsAdvocate(h);
    o << head("Pré-mortem", md);
    if (md) o << "\n```\n" << preMortem(h) << "```\n"; else o << preMortem(h);
    return o.str();
}

}  // namespace report
