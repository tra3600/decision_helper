#include <cmath>
#include <cstdio>
#include <iostream>

#include "DecisionHelper.h"
#include "Report.h"
#include "Storage.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; std::cerr << "ECHEC ligne " << __LINE__ << ": " #cond "\n"; } } while (0)

static DecisionHelper sample() {
    DecisionHelper h;
    h.addCriterion({"Prix", 8, false, true, 9});
    h.addCriterion({"Qualité", 6, true, false, 0});
    h.addCriterion({"Délai", 3, false, false, 0});
    h.addOption("A", {3, 6, 5});
    h.addOption("B", {5, 8, 5});
    h.addOption("C", {2, 9, 9});
    h.addOption("D", {10, 10, 0});  // éliminée par le veto sur le prix
    return h;
}

int main() {
    // Compatibilité première version
    {
        DecisionHelper h({"coût", "temps", "bénéfice"}, {0.4, 0.3, 0.3});
        h.setCriteriaValues({5, 5, 5});
        h.calculateScore();
        CHECK(h.makeDecision(5.0) == "y aller");
        CHECK(h.makeDecision(5.1) == "ne pas y aller");
        bool threw = false;
        try { h.setCriteriaValues({1}); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
    // Classement, veto, TOPSIS
    {
        auto h = sample();
        auto rk = h.rank();
        CHECK(rk.back().eliminated && rk.back().index == 3);
        for (size_t i = 0; i + 1 < rk.size(); ++i) if (!rk[i + 1].eliminated) CHECK(rk[i].score >= rk[i + 1].score);
        for (auto& r : rk) CHECK(r.topsis >= 0 && r.topsis <= 1 && r.score >= 0 && r.score <= 100);
        auto w = h.normalizedWeights();
        double s = 0; for (double x : w) s += x;
        CHECK(std::fabs(s - 1) < 1e-9);
    }
    // Monte-Carlo : probabilités cohérentes, déterminisme, éliminée jamais gagnante
    {
        auto h = sample();
        auto a = h.monteCarlo(2000, 0.35, 0.8, 7), b = h.monteCarlo(2000, 0.35, 0.8, 7);
        double tot = 0;
        for (double p : a.winProb) tot += p;
        CHECK(std::fabs(tot - 1) < 1e-9);
        CHECK(a.winProb[3] == 0.0);
        CHECK(a.winProb == b.winProb);
        for (size_t i = 0; i < 3; ++i) CHECK(a.p05[i] <= a.mean[i] && a.mean[i] <= a.p95[i]);
    }
    // Sensibilité : appliquer la bascule annoncée change bien le gagnant
    {
        auto h = sample();
        auto sens = h.sensitivity();
        size_t win = h.rank()[0].index;
        CHECK(sens.winner == win);
        int checked = 0;
        for (size_t j = 0; j < h.getCriteria().size(); ++j) {
            auto& f = sens.weightFlips[j];
            if (!f.exists) continue;
            auto w = h.normalizedWeights();
            double t = f.to, rest = 1.0 - w[j];
            std::vector<double> w2(w.size());
            for (size_t k = 0; k < w.size(); ++k) w2[k] = k == j ? t : w[k] * (1 - t) / rest;
            // à la bascule, le rival égalise le gagnant
            CHECK(std::fabs(h.scoreOf(win, w2) - h.scoreOf(f.rival, w2)) < 1e-9);
            ++checked;
        }
        for (size_t j = 0; j < h.getCriteria().size(); ++j) {
            auto& f = sens.valueFlips[j];
            if (!f.exists) continue;
            double gap = h.scoreOf(win, h.normalizedWeights()) - h.scoreOf(f.rival, h.normalizedWeights());
            CHECK(std::fabs(f.delta * h.normalizedWeights()[j] - gap) < 1e-9);
        }
        CHECK(checked > 0);
    }
    // AHP : matrice parfaitement cohérente → poids exacts, CR ≈ 0 ; incohérente → CR élevé
    {
        auto r = DecisionHelper::ahp({{1, 2, 4}, {0.5, 1, 2}, {0.25, 0.5, 1}});
        CHECK(std::fabs(r.weights[0] - 4.0 / 7) < 1e-6 && std::fabs(r.weights[2] - 1.0 / 7) < 1e-6);
        CHECK(r.cr < 1e-6 && r.consistent);
        auto bad = DecisionHelper::ahp({{1, 9, 1.0 / 9}, {1.0 / 9, 1, 9}, {9, 1.0 / 9, 1}});
        CHECK(!bad.consistent);
    }
    // Persistance aller-retour
    {
        auto h = sample();
        h.setTitle("Test");
        std::string err;
        CHECK(saveModel(h, "/tmp/dh_test.dh", err));
        DecisionHelper g;
        CHECK(loadModel(g, "/tmp/dh_test.dh", err));
        CHECK(g.getTitle() == "Test" && g.getCriteria().size() == 3 && g.getOptions().size() == 4);
        CHECK(g.getCriteria()[0].hasVeto && !g.getCriteria()[0].benefit);
        CHECK(g.rank()[0].index == h.rank()[0].index);
        CHECK(!loadModel(g, "/tmp/inexistant.dh", err));
        std::remove("/tmp/dh_test.dh");
    }
    // Apprentissage : la satisfaction dépend uniquement de « Qualité »
    {
        DecisionHelper h;
        h.addCriterion({"Prix", 9, true, false, 0});
        h.addCriterion({"Qualité", 1, true, false, 0});
        std::vector<HistoryRecord> hist;
        double prix[] = {2, 8, 5, 9, 1, 6, 3, 7}, qual[] = {9, 2, 6, 1, 8, 3, 7, 4};
        for (int i = 0; i < 8; ++i) {
            HistoryRecord r;
            r.satisfaction = qual[i];
            r.ratings = {{"Prix", prix[i]}, {"Qualité", qual[i]}};
            hist.push_back(r);
        }
        auto lw = learnWeights(h, hist);
        CHECK(lw.ok && lw.weights[1] > 0.8);
        CHECK(!learnWeights(h, {hist[0], hist[1]}).ok);
    }
    // Rapports : pas de plantage, contenu attendu
    {
        auto h = sample();
        auto mc = h.monteCarlo(500);
        auto txt = report::full(h, mc), md = report::full(h, mc, true);
        CHECK(txt.find("Recommandation") != std::string::npos);
        CHECK(md.find("## ") != std::string::npos && md.find("| Option") != std::string::npos);
        DecisionHelper empty;
        report::full(empty, empty.monteCarlo(10));
    }
    if (failures) { std::cerr << failures << " échec(s)\n"; return 1; }
    std::cout << "Tous les tests passent.\n";
    return 0;
}
