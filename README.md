# decision_helper
programme en C++ qui aide à la prise de décision en fonction de plusieurs paramètres pondérés.

L'idée est de fournir des critères avec des poids associés et de calculer une note finale. En fonction de cette note, le programme prendra une décision ("y aller" ou "ne pas y aller").

Étapes
Définir les critères et leurs poids.
Collecter les valeurs des critères.
Calculer la note pondérée.
Prendre une décision en fonction de cette note.

Explications
Classes et Structures:

DecisionHelper: Cette classe gère les critères, les poids, les valeurs et calcule le score pondéré. Elle prend également la décision en fonction du seuil fourni.
Main Function:

Initialise les critères et les poids.
Demande à l'utilisateur de fournir les valeurs des critères.
Calcule le score pondéré.
Prend une décision en fonction du seuil fourni par l'utilisateur.
Compilation et Exécution
Assurez-vous que tous les fichiers sont dans le même répertoire, puis compilez et exécutez le programme avec :

g++ -o decision_helper main.cpp DecisionHelper.cpp
./decision_helper

Ce programme est une base simplifiée pour la prise de décision pondérée. Vous pouvez l'étendre pour inclure davantage de critères, ajuster les poids dynamiquement, ou améliorer l'interface utilisateur pour une meilleure expérience.
