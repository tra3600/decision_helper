#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

#include "DecisionHelper.h"
#include "Report.h"
#include "Storage.h"

namespace {

struct Quit {};

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string s;
    if (!std::getline(std::cin, s)) throw Quit();
    return trim(s);
}

// Accepte « 7 », « 7,5 » et « 1/3 ».
bool parseNumber(std::string s, double& v) {
    for (auto& c : s)
        if (c == ',') c = '.';
    size_t slash = s.find('/');
    try {
        size_t used = 0;
        if (slash != std::string::npos) {
            double a = std::stod(s.substr(0, slash)), b = std::stod(s.substr(slash + 1));
            if (b == 0) return false;
            v = a / b;
            return true;
        }
        v = std::stod(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

double readNumber(const std::string& prompt, double lo, double hi, const double* def = nullptr) {
    for (;;) {
        std::string s = readLine(prompt);
        double v;
        if (s.empty() && def) return *def;
        if (parseNumber(s, v) && v >= lo && v <= hi) return v;
        std::cout << "  ⚠ Entrez un nombre entre " << lo << " et " << hi << ".\n";
    }
}

bool askYesNo(const std::string& prompt, bool def) {
    std::string s = readLine(prompt + (def ? " [O/n] " : " [o/N] "));
    if (s.empty()) return def;
    return s[0] == 'o' || s[0] == 'O' || s[0] == 'y' || s[0] == 'Y';
}

void listCriteria(const DecisionHelper& h) {
    if (h.getCriteria().empty()) { std::cout << "  (aucun critère)\n"; return; }
    std::cout << report::weights(h);
}

void listOptions(const DecisionHelper& h) {
    const auto& cr = h.getCriteria();
    for (size_t i = 0; i < h.getOptions().size(); ++i) {
        std::cout << "  " << i + 1 << ". " << h.getOptions()[i].name << "  [";
        for (size_t c = 0; c < cr.size(); ++c) std::cout << (c ? " | " : "") << cr[c].name << "=" << h.getOptions()[i].values[c];
        std::cout << "]\n";
    }
    if (h.getOptions().empty()) std::cout << "  (aucune option)\n";
}

void addCriterion(DecisionHelper& h) {
    Criterion c;
    c.name = readLine("Nom du critère : ");
    if (c.name.empty()) return;
    c.benefit = !askYesNo("Est-ce un COÛT (plus c'est bas, mieux c'est : prix, durée, risque…) ?", false);
    double def = 5;
    c.weight = readNumber("Importance de 0 à 10 [5] : ", 0, 10, &def);
    if (askYesNo("Définir un seuil éliminatoire (veto) ?", false)) {
        c.hasVeto = true;
        c.veto = readNumber(c.benefit ? "Note minimale acceptable (0-10) : " : "Note maximale acceptable (0-10) : ", 0, 10);
    }
    h.addCriterion(c);
    std::cout << "  ✓ Critère ajouté" << (h.getOptions().empty() ? "" : " (les options existantes reçoivent la note neutre 5 : ajustez-les)") << ".\n";
}

void addOption(DecisionHelper& h) {
    if (h.getCriteria().empty()) { std::cout << "  Définissez d'abord des critères.\n"; return; }
    std::string name = readLine("Nom de l'option : ");
    if (name.empty()) return;
    std::vector<double> v;
    std::cout << "  Notez de 0 à 10 (0 = pire, 10 = meilleur ; pour un coût, 10 = très coûteux).\n";
    for (auto& c : h.getCriteria()) {
        v.push_back(readNumber("   " + c.name + (c.benefit ? "" : " (coût)") + " : ", 0, 10));
    }
    h.addOption(name, v);
    std::cout << "  ✓ Option ajoutée.\n";
}

void editValue(DecisionHelper& h) {
    if (h.getOptions().empty()) return;
    listOptions(h);
    size_t o = static_cast<size_t>(readNumber("Option à modifier (numéro) : ", 1, h.getOptions().size())) - 1;
    for (size_t c = 0; c < h.getCriteria().size(); ++c)
        std::cout << "   " << c + 1 << ". " << h.getCriteria()[c].name << " = " << h.getOptions()[o].values[c] << "\n";
    size_t c = static_cast<size_t>(readNumber("Critère (numéro) : ", 1, h.getCriteria().size())) - 1;
    h.setValue(o, c, readNumber("Nouvelle note (0-10) : ", 0, 10));
}

void manualWeights(DecisionHelper& h) {
    for (size_t i = 0; i < h.getCriteria().size(); ++i) {
        double def = h.getCriteria()[i].weight;
        h.setWeight(i, readNumber("Importance de « " + h.getCriteria()[i].name + " » 0-10 [" + std::to_string(static_cast<int>(def)) + "] : ", 0, 10, &def));
    }
}

void ahpWeights(DecisionHelper& h) {
    const size_t n = h.getCriteria().size();
    if (n < 2) { std::cout << "  Il faut au moins 2 critères.\n"; return; }
    std::cout << "  Comparaisons par paires (méthode AHP). Échelle : 1 = aussi important, 3 = modérément plus,\n"
                 "  5 = nettement plus, 7 = très nettement plus, 9 = extrêmement plus. Si le SECOND est plus\n"
                 "  important, saisissez une fraction (1/3, 1/5…).\n";
    std::vector<std::vector<double>> m(n, std::vector<double>(n, 1.0));
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j) {
            double v = readNumber("  « " + h.getCriteria()[i].name + " » vs « " + h.getCriteria()[j].name + " » : ", 1.0 / 9, 9);
            m[i][j] = v;
            m[j][i] = 1.0 / v;
        }
    AhpResult r = DecisionHelper::ahp(m);
    std::cout << "  Poids déduits :\n";
    for (size_t i = 0; i < n; ++i)
        std::cout << "   " << h.getCriteria()[i].name << " : " << static_cast<int>(r.weights[i] * 100 + 0.5) << " %\n";
    std::cout << "  Ratio de cohérence : " << r.cr << (r.consistent ? " (cohérent)\n" : " (> 0.10 : vos comparaisons se contredisent, revoyez-les)\n");
    if (askYesNo("Appliquer ces poids ?", r.consistent))
        for (size_t i = 0; i < n; ++i) h.setWeight(i, r.weights[i] * 100);
}

void criteriaMenu(DecisionHelper& h) {
    for (;;) {
        std::cout << "\n-- Critères --\n";
        listCriteria(h);
        std::string c = readLine("[a] ajouter  [s] supprimer  [m] poids manuels  [p] poids par paires (AHP)  [r] retour : ");
        if (c == "a") addCriterion(h);
        else if (c == "s" && !h.getCriteria().empty()) {
            h.removeCriterion(static_cast<size_t>(readNumber("Numéro du critère : ", 1, h.getCriteria().size())) - 1);
        } else if (c == "m") manualWeights(h);
        else if (c == "p") ahpWeights(h);
        else if (c == "r" || c.empty()) return;
    }
}

void optionsMenu(DecisionHelper& h) {
    for (;;) {
        std::cout << "\n-- Options --\n";
        listOptions(h);
        std::string c = readLine("[a] ajouter  [s] supprimer  [e] modifier une note  [r] retour : ");
        if (c == "a") addOption(h);
        else if (c == "s" && !h.getOptions().empty()) {
            h.removeOption(static_cast<size_t>(readNumber("Numéro de l'option : ", 1, h.getOptions().size())) - 1);
        } else if (c == "e") editValue(h);
        else if (c == "r" || c.empty()) return;
    }
}

bool ready(const DecisionHelper& h) {
    if (h.getCriteria().empty() || h.getOptions().empty()) {
        std::cout << "  Définissez au moins un critère et une option (ou tapez 'd' pour la démo).\n";
        return false;
    }
    return true;
}

void goNoGo(DecisionHelper& h) {
    if (!ready(h)) return;
    listOptions(h);
    size_t o = h.getOptions().size() == 1 ? 0 : static_cast<size_t>(readNumber("Option à évaluer (numéro) : ", 1, h.getOptions().size())) - 1;
    double def = 6.0;
    double thr = readNumber("Seuil de décision sur 10 [6] : ", 0, 10, &def);
    double score = h.scoreOf(o, h.normalizedWeights());
    double p = h.probabilityAbove(o, thr);
    std::cout << "  Note pondérée : " << score << "/10 — seuil " << thr << "\n"
              << "  Probabilité de dépasser le seuil malgré l'incertitude : " << static_cast<int>(p * 100 + 0.5) << " %\n"
              << "  Décision : " << (score >= thr ? "y aller" : "ne pas y aller")
              << (p > 0.35 && p < 0.65 ? "  (⚠ cas limite : l'incertitude pèse autant que l'écart)\n" : "\n");
    if (!h.vetoReason(o).empty()) std::cout << "  ✗ Veto : " << h.vetoReason(o) << "\n";
}

void coinFlip(DecisionHelper& h) {
    if (!ready(h)) return;
    auto rk = h.rank();
    std::vector<size_t> a;
    for (auto& r : rk) if (!r.eliminated) a.push_back(r.index);
    if (a.size() < 2) { std::cout << "  Il faut deux options admissibles.\n"; return; }
    std::cout << "  Test pile-ou-face : la pièce va choisir entre « " << h.getOptions()[a[0]].name << " » et « "
              << h.getOptions()[a[1]].name << " ».\n";
    std::mt19937 gen(std::random_device{}());
    size_t pick = a[gen() % 2], other = pick == a[0] ? a[1] : a[0];
    std::cout << "  → La pièce désigne « " << h.getOptions()[pick] .name << " ».\n";
    std::string s = readLine("  Sentez-vous un soulagement (s) ou une déception (d) ? ");
    if (!s.empty() && (s[0] == 'd' || s[0] == 'D'))
        std::cout << "  Votre intuition préfère « " << h.getOptions()[other].name << " » : c'est une information que les chiffres ne voient pas.\n";
    else
        std::cout << "  Votre intuition est en accord avec la pièce : « " << h.getOptions()[pick].name << " » vous convient émotionnellement.\n";
}

void learningMenu(DecisionHelper& h, const std::string& histPath) {
    for (;;) {
        auto hist = loadHistory(histPath);
        std::cout << "\n-- Journal & apprentissage (" << hist.size() << " décision(s) enregistrée(s) dans " << histPath << ") --\n";
        std::string c = readLine("[j] enregistrer une décision prise et sa satisfaction  [a] voir mes préférences réelles  [r] retour : ");
        if (c == "j") {
            if (!ready(h)) continue;
            listOptions(h);
            size_t o = static_cast<size_t>(readNumber("Option finalement choisie (numéro) : ", 1, h.getOptions().size())) - 1;
            double sat = readNumber("Satisfaction a posteriori (0-10) : ", 0, 10);
            appendHistory(histPath, makeRecord(h, o, sat)) ? void(std::cout << "  ✓ Enregistré.\n") : void(std::cout << "  ⚠ Écriture impossible.\n");
        } else if (c == "a") {
            if (h.getCriteria().empty()) continue;
            auto lw = learnWeights(h, hist);
            if (!lw.ok) { std::cout << "  Pas encore assez de données : " << lw.note << ".\n"; continue; }
            auto w = h.normalizedWeights();
            std::cout << "  Préférences déclarées vs révélées par vos satisfactions (" << lw.note << ") :\n";
            for (size_t i = 0; i < w.size(); ++i)
                std::cout << "   " << h.getCriteria()[i].name << " : déclaré " << static_cast<int>(w[i] * 100 + 0.5) << " % → réel "
                          << static_cast<int>(lw.weights[i] * 100 + 0.5) << " %  (corrélation " << lw.correlation[i] << ")\n";
            if (askYesNo("Mélanger 50/50 vos poids déclarés et appris ?", false))
                for (size_t i = 0; i < w.size(); ++i) h.setWeight(i, (w[i] + lw.weights[i]) * 50.0);
        } else if (c == "r" || c.empty()) return;
    }
}

DecisionHelper demo() {
    DecisionHelper h;
    h.setTitle("Quel logement louer ?");
    Criterion loyer{"Loyer", 8, false, true, 8};
    Criterion surface{"Surface", 6, true, false, 0};
    Criterion trajet{"Temps de trajet", 7, false, false, 0};
    Criterion lumiere{"Luminosité", 4, true, false, 0};
    Criterion quartier{"Ambiance quartier", 5, true, false, 0};
    for (auto& c : {loyer, surface, trajet, lumiere, quartier}) h.addCriterion(c);
    h.addOption("Studio centre-ville", {7, 3, 1, 6, 9});
    h.addOption("T2 proche métro", {6, 6, 3, 7, 7});
    h.addOption("T3 en périphérie", {5, 9, 8, 8, 4});
    h.addOption("Loft de luxe", {10, 9, 2, 9, 9});
    return h;
}

void analyse(const DecisionHelper& h, unsigned seed) {
    if (!ready(h)) return;
    std::cout << report::full(h, h.monteCarlo(5000, 0.35, 0.8, seed));
}

int classic() {
    DecisionHelper h({"coût", "temps", "bénéfice"}, {0.4, 0.3, 0.3});
    std::vector<double> values(3);
    std::cout << "Entrez les valeurs (0-10) pour les critères suivants:\n";
    for (size_t i = 0; i < values.size(); ++i) {
        std::string names[] = {"coût", "temps", "bénéfice"};
        values[i] = readNumber("  " + names[i] + " : ", 0, 10);
    }
    h.setCriteriaValues(values);
    h.calculateScore();
    double thr = readNumber("Entrez le seuil de décision : ", 0, 10);
    std::cout << "La décision est: " << h.makeDecision(thr) << std::endl;
    return 0;
}

void usage() {
    std::cout << "Decision Helper — aide à la décision multicritère\n"
                 "  ./decision_helper               menu interactif\n"
                 "  ./decision_helper --demo        analyse complète d'un exemple\n"
                 "  ./decision_helper --load F      charge un problème (puis menu)\n"
                 "  ./decision_helper --analyze F   analyse non interactive d'un fichier\n"
                 "  ./decision_helper --report OUT  avec --analyze/--demo : exporte un rapport Markdown\n"
                 "  ./decision_helper --classic     mode simple d'origine (un seul choix, un seuil)\n"
                 "  Options : --seed N  --history FICHIER\n";
}

}  // namespace

int main(int argc, char** argv) {
    DecisionHelper h;
    std::string histPath = "decision_history.tsv", loadPath, reportPath;
    bool doDemo = false, analyzeOnly = false;
    unsigned seed = 42;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; };
        if (a == "--demo") doDemo = true;
        else if (a == "--classic") { try { return classic(); } catch (Quit&) { return 0; } }
        else if (a == "--load") next(loadPath);
        else if (a == "--analyze") { next(loadPath); analyzeOnly = true; }
        else if (a == "--report") next(reportPath);
        else if (a == "--history") next(histPath);
        else if (a == "--seed") { std::string s; next(s); seed = static_cast<unsigned>(std::strtoul(s.c_str(), nullptr, 10)); }
        else { usage(); return a == "--help" || a == "-h" ? 0 : 1; }
    }

    if (!loadPath.empty()) {
        std::string err;
        if (!loadModel(h, loadPath, err)) { std::cerr << "Erreur : " << err << "\n"; return 1; }
    }
    if (doDemo) { h = demo(); analyzeOnly = true; }

    if (analyzeOnly) {
        if (h.getCriteria().empty() || h.getOptions().empty()) { std::cerr << "Problème vide.\n"; return 1; }
        auto mc = h.monteCarlo(5000, 0.35, 0.8, seed);
        std::cout << report::full(h, mc);
        if (!reportPath.empty()) {
            std::ofstream f(reportPath);
            if (!f) { std::cerr << "Impossible d'écrire " << reportPath << "\n"; return 1; }
            f << report::full(h, mc, true);
            std::cout << "\nRapport Markdown écrit dans " << reportPath << "\n";
        }
        return 0;
    }

    try {
        std::cout << "═══ Decision Helper ═══\n";
        if (h.getTitle().empty()) h.setTitle(readLine("Quelle décision devez-vous prendre ? "));
        for (;;) {
            std::cout << "\n« " << h.getTitle() << " » — " << h.getCriteria().size() << " critère(s), " << h.getOptions().size() << " option(s)\n"
                      << " 1 Critères & poids     2 Options & notes      3 Analyse complète\n"
                      << " 4 Go / No-Go (seuil)   5 Pile ou face         6 Journal & apprentissage\n"
                      << " 7 Sauvegarder          8 Charger              9 Exporter rapport Markdown\n"
                      << " d Démo                 0 Quitter\n";
            std::string c = readLine("> ");
            std::string err;
            if (c == "1") criteriaMenu(h);
            else if (c == "2") optionsMenu(h);
            else if (c == "3") analyse(h, seed);
            else if (c == "4") goNoGo(h);
            else if (c == "5") coinFlip(h);
            else if (c == "6") learningMenu(h, histPath);
            else if (c == "7") {
                std::string p = readLine("Fichier [decision.dh] : ");
                if (p.empty()) p = "decision.dh";
                std::cout << (saveModel(h, p, err) ? "  ✓ Sauvegardé.\n" : "  ⚠ " + err + "\n");
            } else if (c == "8") {
                std::string p = readLine("Fichier : ");
                std::cout << (loadModel(h, p, err) ? "  ✓ Chargé.\n" : "  ⚠ " + err + "\n");
            } else if (c == "9") {
                if (!ready(h)) continue;
                std::string p = readLine("Fichier [rapport.md] : ");
                if (p.empty()) p = "rapport.md";
                std::ofstream f(p);
                if (f) { f << report::full(h, h.monteCarlo(5000, 0.35, 0.8, seed), true); std::cout << "  ✓ Rapport écrit.\n"; }
                else std::cout << "  ⚠ Écriture impossible.\n";
            } else if (c == "d") h = demo();
            else if (c == "0" || c == "q") break;
        }
    } catch (Quit&) {
        std::cout << "\n";
    }
    return 0;
}
