# Plugin AgentPlurAI (`liteideaf`)

Menu LiteIDE **AgentPlurAI**, id `plugin/liteideaf`.
Version lue dans Santé (`LITEIDEAF_VERSION` dans `liteideafplugin.h`).

Un Cline par projet Go, dans un conteneur. Plusieurs Ollama en parallèle.
La clé Z.ai reste dans le volume `cline-config` du conteneur, jamais dans le plugin.

## Menu

| Entrée | Rôle |
|---|---|
| Portée | Montre ce que LiteIDE voit : dossier ouvert, fichier courant, sélection. |
| Prompt | Ouvre la saisie. Radio : sélection, fichier courant, ou projet. Envoie au moteur actif. Attente non modale avec compteur. |
| Historique | Lit `.agentplurai/prompts.log` dans le volume du projet. |
| Journal | Lit `.agentplurai/actions.log`. |
| Recharger | Rouvre dans LiteIDE les fichiers que l'agent a écrits (lignes `write` du journal). Refusé si un tampon est sale. |

## Paramétrage

| Entrée | Rôle |
|---|---|
| IA par défaut | Choisit le moteur utilisé par Prompt. Mémorisé dans le plugin. |
| Aligner le projet | Vérifie le conteneur nommé dans le YAML. S'il ne tourne pas, ou s'il ne monte pas le projet actif sur `/work/project`, le détruit et le recrée. Ne change que le montage du projet. La clé reste dans le volume `cline-config`. |
| System prompt | Édite le prompt système global et le prompt local du moteur actif. |
| Masquage réflexion | Coche. Cochée : `thinking_progress: false`, le raisonnement n'est pas affiché. Décochée : il l'est. Le champ est écrit dans le YAML à la création. |
| Santé | Version du plugin, état du conteneur Cline, joignabilité des Ollama. |
| Quota Cline | Lit la clé déjà stockée dans le conteneur et interroge `api.z.ai`. Affiche le restant, le consommé et la fenêtre (5 h, hebdo, mensuel) quand l'API les donne. |

## Contrat

`agentplurai.yaml`, cherché à plusieurs endroits (réglage, dossier du fichier, Open Folder, projet, cwd, stockage LiteIDE, home). Créé s'il manque.

Champs utiles :

- `container` : nom Docker du companion.
- `image` : image du companion.
- `engines` : un `kind: agent` (Cline) et des `kind: model` (Ollama).
- `shared.container` : point de montage dans le conteneur, `/work/project`.
- `thinking_progress` : affiche le raisonnement.
- `rendering_markdown` : rendu Markdown de la réponse.

## Companion

Le plugin lance le conteneur lui-même :

```text
docker run -d --name <container>
  -v <projet>:/work/project
  -v cline-config:/root/.cline
  -w /work/project <image> sleep infinity
```

`cline-config` est un volume nommé. Il survit aux recréations. Une authentification suffit.

Le projet actif est le seul dossier monté. Un chemin hors de ce dossier est invisible pour l'agent.
