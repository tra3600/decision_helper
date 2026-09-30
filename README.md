# decision_helper

Assistant de décision multicritère en C++17 (aucune dépendance). Il ne se contente plus de calculer une note :
il compare plusieurs options, mesure la **robustesse** du résultat, l'**explique**, et **apprend** de vos décisions passées.

## Fonctionnalités

| Domaine | Ce que fait le programme |
|---|---|
| Modélisation | Plusieurs options, critères *bénéfice* ou *coût*, seuils éliminatoires (**veto**), notes 0-10 |
| Pondération | Saisie manuelle ou **AHP** (comparaisons par paires, vecteur propre, **ratio de cohérence** qui détecte vos contradictions) |
| Classement | Somme pondérée (/100) + **TOPSIS** (proximité à la solution idéale) |
| Incertitude | **Simulation Monte-Carlo** (5000 scénarios) : probabilité que chaque option soit la meilleure, intervalle à 90 % |
| Sensibilité | **Points de bascule** : de combien faut-il changer un poids ou une note pour renverser le résultat |
| Explication | « Pourquoi ce classement ? » critère par critère, verdict en langage clair (décision claire / fragile / quasi-égalité) |
| Regard critique | **Avocat du diable** (plaidoyer pour le second), **pré-mortem** (« imaginez que ça a échoué »), 4 philosophies (optimiste, prudent, équilibré, sans regret) |
| Intuition | Test **pile-ou-face** : la réaction émotionnelle à la pièce révèle votre préférence cachée |
| Go / No-Go | Mode seuil d'origine, enrichi de la probabilité de dépasser le seuil malgré l'incertitude |
| Apprentissage | **Journal** des décisions et de la satisfaction a posteriori ; régression ridge qui révèle vos préférences *réelles* vs *déclarées* |
| Export | Sauvegarde texte éditable à la main, rapport **Markdown** |

## Compilation et utilisation

```
make                     # compile ./decision_helper
make test                # lance les tests automatiques
./decision_helper        # menu interactif
./decision_helper --demo # analyse complète d'un exemple (choix d'un logement)
./decision_helper --analyze probleme.dh --report rapport.md
./decision_helper --classic   # mode simple de la première version
```

Options : `--load FICHIER`, `--seed N` (reproductibilité), `--history FICHIER` (journal, `decision_history.tsv` par défaut).

## Format de fichier

```
title=Quel logement louer ?
criterion=Loyer|8|cost|8          # nom | importance | benefit/cost | veto ("-" si aucun)
criterion=Surface|6|benefit|-
option=Studio centre|7;3          # nom | une note par critère
```

## Comment lire les résultats

- **Score /100** : note pondérée ; pour un critère *coût*, la note est inversée (10 = très coûteux → 0 point).
- **Gagne dans X % des cas** : on fait varier aléatoirement poids (±35 %) et notes (±0,8 pt) pour simuler votre incertitude ; si le gagnant l'emporte rarement, le choix est fragile.
- **Points de bascule** : plus la variation nécessaire est faible, plus il faut vérifier ce critère.
