# Plugin `liteideaf`

Squelette `IPlugin` compilé **hors** `plugins.pro` officiel, contre
`/src/liteide/liteidex/src/api`.

Dans le conteneur `plugin-dev` :

```bash
build-plugin.sh
```

DLL visée : `/dist/lib/liteide/plugins/liteideaf.dll`  
Puis relancer `dist\bin\liteide.exe` sur l’hôte Windows.

Cline : travailler uniquement dans `/work/plugin-src`. Ne pas committer de clé Z.ai.
Ne pas appeler `api.z.ai` depuis le plugin (contrat SFD).
