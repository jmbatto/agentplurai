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

Exemple complet :

```yaml
meta:
  schema: 1
  createdBy: plugin/liteideaf
container: cline-project
image: cline-project:go1.26.7
shared:
  container: /work/project
track:
  kind: git
  branch: agent
  refuseIfDirty: true
engines:
  - name: qwen-local
    kind: model
    pending: false
    model: qwen2.5
    access: http
    endpoint: http://127.0.0.1:11434
    thinking_progress: false
    rendering_markdown: true
  - name: codestral-local
    kind: model
    pending: false
    model: codestral
    access: http
    endpoint: http://127.0.0.1:11435
    thinking_progress: false
    rendering_markdown: true
  - name: cline-project
    kind: agent
    pending: false
    image: cline-project:go1.26.7
    container: cline-project
    access: docker
    shared:
      container: /work/project
    thinking_progress: false
    rendering_markdown: true
projects:
  - path: "C:/chemin/vers/le/projet"
    engine: cline-project
    active: true
```

Champs :

| Champ | Où | Rôle |
|---|---|---|
| `container` | racine et moteur agent | Nom Docker du companion. |
| `image` | racine et moteur agent | Image du companion. |
| `shared.container` | racine et moteur agent | Montage dans le conteneur. Toujours `/work/project`. |
| `engines` | liste | Un `kind: agent` (Cline) et autant de `kind: model` (Ollama) que voulu. |
| `name` | moteur | Identifiant. C'est lui qui apparaît dans IA par défaut. |
| `kind` | moteur | `agent` pour Cline, `model` pour Ollama. |
| `endpoint` | modèle | URL du serveur Ollama, sans `/api`. |
| `model` | modèle | Nom du modèle tel que `ollama list` le donne. |
| `thinking_progress` | moteur | `false` masque la réflexion. |
| `rendering_markdown` | moteur | `true` rend la réponse en Markdown. |
| `projects` | liste | Désigne le moteur actif pour un dossier. |

## Plusieurs Ollama

Un bloc `kind: model` par serveur. Chacun a son `name`, son `endpoint` et son `model`. Le plugin ne lance pas Ollama : le serveur doit déjà tourner.

À saisir pour chaque Ollama :

1. `name` : un identifiant unique, par exemple `qwen-local`.
2. `endpoint` : l'URL, port compris. Deux Ollama sur la même machine ont deux ports (`11434`, `11435`).
3. `model` : le nom exact du modèle (`ollama list`).
4. `pending: false` une fois que Santé répond `ok`.

Exemple de deux serveurs :

```yaml
  - name: qwen-local
    kind: model
    pending: false
    model: qwen2.5
    access: http
    endpoint: http://127.0.0.1:11434
    thinking_progress: false
    rendering_markdown: true
  - name: deepseek-gpu
    kind: model
    pending: false
    model: deepseek-coder-v2
    access: http
    endpoint: http://192.168.1.20:11434
    thinking_progress: false
    rendering_markdown: true
```

Ensuite : Paramétrage → IA par défaut, et choisir le moteur. Prompt enverra à celui-là. Santé vérifie chaque `endpoint`.

Un seul Cline. Les Ollama ne voient pas le dépôt : seul le texte choisi (sélection, fichier, projet) part dans la requête.

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
