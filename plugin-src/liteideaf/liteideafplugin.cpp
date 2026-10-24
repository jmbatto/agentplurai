#include "liteideafplugin.h"

#include "liteeditorapi/liteeditorapi.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString scalar(const QString &text, const QString &key)
{
    const QRegularExpression re("^\\s*" + key + ":\\s*(.*)$", QRegularExpression::MultilineOption);
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return QString();
    QString v = m.captured(1).trimmed();
    if (v.startsWith('"') && v.endsWith('"'))
        v = v.mid(1, v.size() - 2);
    return v;
}

QString folded(const QString &text, const QString &key)
{
    const int at = text.indexOf(QRegularExpression("^\\s*" + key + ":\\s*\\|\\s*$", QRegularExpression::MultilineOption));
    if (at < 0)
        return scalar(text, key);
    int line = text.indexOf('\n', at);
    if (line < 0)
        return QString();
    QString out;
    const QStringList rows = text.mid(line + 1).split('\n');
    for (const QString &row : rows) {
        if (row.startsWith("  "))
            out += row.mid(2) + "\n";
        else if (!row.trimmed().isEmpty())
            break;
    }
    return out.trimmed();
}

QString stripAnsi(QString text)
{
    text.remove(QRegularExpression("\x1b\\[[0-9;?]*[A-Za-z]"));
    text.remove(QRegularExpression("\x1b\\][^\x07]*\x07"));
    text.remove(QRegularExpression("\\[[0-9;]*m"));
    return text;
}

void splitCline(const QString &raw, QString *response, QString *thinking)
{
    QString clean = stripAnsi(raw);
    clean.replace("\r\n", "\n");
    const QStringList rows = clean.split('\n');
    int lastMarker = -1;
    for (int i = 0; i < rows.size(); ++i) {
        const QString t = rows.at(i).trimmed();
        if (t.contains("[thinking]") || t.contains("[run_commands]")
            || t.contains("[run_command]") || t.contains("[editor]") || t.startsWith("⎿"))
            lastMarker = i;
    }
    if (lastMarker < 0) {
        *thinking = QString();
        *response = clean.trimmed();
        return;
    }
    int splitAt = lastMarker + 1;
    while (splitAt < rows.size() && rows.at(splitAt).trimmed().isEmpty())
        ++splitAt;
    QString think;
    for (int i = 0; i <= lastMarker; ++i)
        think += rows.at(i) + "\n";
    QString body;
    for (int i = splitAt; i < rows.size(); ++i)
        body += rows.at(i) + "\n";
    *thinking = think.trimmed();
    *response = body.trimmed();
    if (response->isEmpty())
        *response = clean.trimmed();
}

} // namespace

LiteIdeAfPlugin::LiteIdeAfPlugin()
    : m_defaultEngine()
    , m_loadingDialog(nullptr)
    , m_loadingLabel(nullptr)
    , m_elapsedTimer(nullptr)
    , m_elapsedSeconds(0)
    , m_pendingReply(nullptr)
    , m_pendingProcess(nullptr)
    , m_hideThinkingAction(nullptr)
    , m_app(nullptr)
{
}

void LiteIdeAfPlugin::showLoadingDialog(const QString &engine)
{
    if (m_loadingDialog)
        hideLoadingDialog();
    m_elapsedSeconds = 0;
    m_loadingDialog = new QDialog(m_app->mainWindow());
    m_loadingDialog->setWindowTitle("AgentPlurAI — en cours");
    m_loadingDialog->setWindowModality(Qt::NonModal);
    m_loadingDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    auto *layout = new QVBoxLayout(m_loadingDialog);
    m_loadingLabel = new QLabel(m_loadingDialog);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setText("Moteur : " + engine + "\nÉcoulé : 0 s");
    layout->addWidget(m_loadingLabel);
    m_loadingDialog->resize(320, 90);
    m_loadingDialog->show();
    m_loadingDialog->raise();
    if (!m_elapsedTimer) {
        m_elapsedTimer = new QTimer(this);
        connect(m_elapsedTimer, &QTimer::timeout, this, &LiteIdeAfPlugin::updateElapsed);
    }
    m_elapsedTimer->start(1000);
}

void LiteIdeAfPlugin::updateElapsed()
{
    if (!m_loadingLabel)
        return;
    ++m_elapsedSeconds;
    const QString engine = m_pendingEngine.isEmpty() ? QString("…") : m_pendingEngine;
    m_loadingLabel->setText("Moteur : " + engine + "\nÉcoulé : " + QString::number(m_elapsedSeconds) + " s");
}

void LiteIdeAfPlugin::hideLoadingDialog()
{
    if (m_elapsedTimer)
        m_elapsedTimer->stop();
    if (m_loadingDialog) {
        m_loadingDialog->close();
        m_loadingDialog->deleteLater();
        m_loadingDialog = nullptr;
        m_loadingLabel = nullptr;
    }
}

QString LiteIdeAfPlugin::contractPath() const
{
    const QString configured = m_app && m_app->settings()
        ? m_app->settings()->value("agentplurai/contract").toString()
        : QString();
    if (!configured.isEmpty() && QFileInfo(configured).isFile())
        return QFileInfo(configured).absoluteFilePath();

    QStringList dirs;
    if (m_app && m_app->editorManager() && m_app->editorManager()->currentEditor())
        dirs << QFileInfo(m_app->editorManager()->currentEditor()->filePath()).absolutePath();
    if (m_app && m_app->fileManager())
        dirs << m_app->fileManager()->folderList();
    if (m_app && m_app->projectManager() && m_app->projectManager()->currentProject())
        dirs << QFileInfo(m_app->projectManager()->currentProject()->filePath()).absolutePath();
    dirs << QDir::currentPath();
    if (m_app)
        dirs << m_app->storagePath();
    dirs << QDir::homePath();

    const QStringList names = {"agentplurai.yaml", "agentplurai.yml"};
    for (const QString &dir : dirs) {
        if (dir.isEmpty())
            continue;
        for (const QString &name : names) {
            const QString candidate = QDir(dir).filePath(name);
            if (QFileInfo(candidate).isFile())
                return QFileInfo(candidate).absoluteFilePath();
        }
    }
    const QString fallbackDir = dirs.isEmpty() || dirs.first().isEmpty()
        ? QDir::currentPath() : dirs.first();
    return QDir(fallbackDir).filePath("agentplurai.yaml");
}

QString LiteIdeAfPlugin::storeDir() const
{
#ifdef Q_OS_WIN
    const QString base = qEnvironmentVariable("APPDATA");
    return QDir(base.isEmpty() ? QDir::homePath() : base).filePath("liteide/agentplurai");
#else
    return QDir::home().filePath(".config/liteide/agentplurai");
#endif
}

QString LiteIdeAfPlugin::logPath() const
{
    const QString forced = qEnvironmentVariable("AGENTPLURAI_LOG");
    if (!forced.isEmpty()) {
        QFile probe(forced);
        if (probe.open(QIODevice::WriteOnly | QIODevice::Append))
            return forced;
    }
#ifndef Q_OS_WIN
    QFile sys("/var/log/liteide/agentplurai.log");
    if (sys.open(QIODevice::WriteOnly | QIODevice::Append))
        return sys.fileName();
#endif
    QDir().mkpath(storeDir());
    return QDir(storeDir()).filePath("mounts.log");
}

void LiteIdeAfPlugin::logEvent(const QString &line)
{
    const QString path = logPath();
    QFileInfo info(path);
    if (info.exists() && info.size() > 10 * 1024 * 1024)
        QFile::rename(path, path + ".1");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    const QString stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    f.write((stamp + "  " + line + "\n").toUtf8());
}

QByteArray LiteIdeAfPlugin::sealKey()
{
    QDir().mkpath(storeDir());
    const QString path = QDir(storeDir()).filePath("seal.key");
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly))
        return f.readAll();
    QByteArray key(32, 0);
    for (int i = 0; i < key.size(); ++i)
        key[i] = char(QRandomGenerator::global()->bounded(256));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(key);
        f.close();
#ifndef Q_OS_WIN
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
    }
    return key;
}

QString LiteIdeAfPlugin::frozenCanonical() const
{
    const QString text = yamlText();
    QStringList parts;
    parts << "host=" + QFileInfo(currentProjectRoot()).canonicalFilePath();
    const QRegularExpression names("^\\s*- name:\\s*(\\S+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = names.globalMatch(text);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        const QString block = engineBlock(name);
        parts << "engine=" + name
              << "kind=" + blockValue(block, "kind")
              << "endpoint=" + blockValue(block, "endpoint")
              << "image=" + blockValue(block, "image")
              << "container=" + blockValue(block, "container");
    }
    parts << "mount=" + containerRoot();
    parts.sort();
    return parts.join("\n");
}

QString LiteIdeAfPlugin::hmacOf(const QString &canonical)
{
    return QString::fromLatin1(
        QMessageAuthenticationCode::hash(canonical.toUtf8(), sealKey(), QCryptographicHash::Sha256).toHex());
}

QString LiteIdeAfPlugin::snapshotPath() const
{
    return QDir(storeDir()).filePath("frozen.snap");
}

bool LiteIdeAfPlugin::ensureContract()
{
    const QString root = currentProjectRoot();
    if (root.isEmpty()) {
        warn("Aucun projet actif. Aucun envoi.");
        logEvent("reject  reason=no-project");
        return false;
    }
    const QString path = QDir(root).filePath("agentplurai.yaml");
    if (QFileInfo(path).isFile())
        return true;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString body =
        "meta:\n"
        "  schema: 1\n"
        "  created: " + now + "\n"
        "  createdBy: plugin/liteideaf\n"
        "  generator: 0.1.0\n"
        "  comment: |\n"
        "    Created by the plugin. Ollama is prefilled on the usual port.\n"
        "    The plugin clears pending after verification.\n"
        "container: cline-project\n"
        "image: cline-project:go1.26.7\n"
        "shared:\n"
        "  container: /work/project\n"
        "track:\n"
        "  kind: git\n"
        "  branch: agent\n"
        "  refuseIfDirty: true\n"
        "engines:\n"
        "  - name: ollama-local\n"
        "    kind: model\n"
        "    pending: true\n"
        "    model: \"\"\n"
        "    access: http\n"
        "    endpoint: http://127.0.0.1:11434\n"
        "    thinking_progress: false\n"
        "    rendering_markdown: true\n"
        "  - name: cline-project\n"
        "    kind: agent\n"
        "    pending: true\n"
        "    image: cline-project:go1.26.7\n"
        "    container: cline-project\n"
        "    access: docker\n"
        "    shared:\n"
        "      container: /work/project\n"
        "    thinking_progress: false\n"
        "    rendering_markdown: true\n"
        "projects:\n"
        "  - path: \"" + QDir::fromNativeSeparators(root) + "\"\n"
        "    engine: cline-project\n"
        "    active: true\n";
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(body.toUtf8()) < 0 || !out.commit()) {
        warn("Contrat illisible. Aucun envoi.");
        logEvent("reject  reason=write-failed  host=" + root);
        return false;
    }
    logEvent("init  host=" + root);
    logEvent("log-path  path=" + logPath());
    info("Contrat créé : agentplurai.yaml. Vérification des moteurs.");
    return true;
}

bool LiteIdeAfPlugin::verifyEngines()
{
    const QString text = yamlText();
    const QRegularExpression names("^\\s*- name:\\s*(\\S+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = names.globalMatch(text);
    bool ok = true;
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        const QString block = engineBlock(name);
        if (blockValue(block, "pending") == "false")
            continue;
        const QString kind = blockValue(block, "kind");
        bool ready = false;
        if (kind == "model") {
            const QString ep = blockValue(block, "endpoint");
            int code = 1;
            run("curl", {"-sf", "--max-time", "2", ep + "/api/tags"}, 4000, &code);
            ready = code == 0;
            if (!ready) {
                warn("Moteur " + name + " injoignable (" + ep + "). Aucun envoi vers celui-ci.");
                logEvent("pending  scope=engine:" + name + "  field=endpoint");
                ok = false;
            }
        } else if (kind == "agent") {
            const QString image = blockValue(block, "image");
            int code = 1;
            run("docker", {"image", "inspect", image}, 8000, &code);
            ready = code == 0;
            if (!ready) {
                warn("Moteur " + name + " injoignable (" + image + "). Aucun envoi vers celui-ci.");
                logEvent("pending  scope=engine:" + name + "  field=image");
                ok = false;
            }
        }
        if (ready) {
            clearPending(name);
            info("Moteur " + name + " prêt.");
            logEvent("ready  scope=engine:" + name);
        }
    }
    return ok;
}

bool LiteIdeAfPlugin::clearPending(const QString &name)
{
    const QString path = contractPath();
    QString text = yamlText();
    const QString needle = "- name: " + name;
    const int at = text.indexOf(needle);
    if (at < 0)
        return false;
    const int next = text.indexOf("\n  - name:", at + needle.size());
    const int end = next < 0 ? text.size() : next;
    QString block = text.mid(at, end - at);
    if (!block.contains("pending:"))
        block += "    pending: false\n";
    else
        block.replace(QRegularExpression("pending:\\s*true"), "pending: false");
    text.replace(at, end - at, block);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit())
        return false;
    return true;
}

bool LiteIdeAfPlugin::ensureGitBranch(const QString &root)
{
    if (root.isEmpty())
        return false;
    int code = 1;
    run("git", {"-C", root, "rev-parse", "--is-inside-work-tree"}, 8000, &code);
    if (code != 0) {
        run("git", {"-C", root, "init"}, 8000, &code);
        if (code != 0)
            return false;
    }
    const QString branch = scalar(yamlText(), "branch").isEmpty() ? "agent" : scalar(yamlText(), "branch");
    run("git", {"-C", root, "checkout", "-B", branch}, 8000, &code);
    if (code == 0)
        logEvent("git  branch=" + branch + "  host=" + root);
    return code == 0;
}

bool LiteIdeAfPlugin::mountsMatch(const QString &mounted, const QString &want) const
{
    auto norm = [](QString s) {
        s = QDir::fromNativeSeparators(s).toLower();
        if (s.startsWith("/host_mnt/"))
            s = s.mid(10);
        while (s.startsWith("//"))
            s = s.mid(1);
        if (s.size() > 2 && s.at(0) == '/' && s.at(2) == '/')
            s = s.mid(1);
        if (s.endsWith('/'))
            s.chop(1);
        return s;
    };
    const QString a = norm(mounted);
    const QString b = norm(QFileInfo(want).canonicalFilePath().isEmpty() ? want : QFileInfo(want).canonicalFilePath());
    return !a.isEmpty() && !b.isEmpty() && (a == b || a.endsWith(b) || b.endsWith(a));
}

bool LiteIdeAfPlugin::ensureContainer(const QString &host)
{
    const QString engine = activeCline();
    const QString block = engineBlock(engine);
    const QString image = blockValue(block, "image");
    if (image.isEmpty()) {
        warn("Champ figé manquant : image. Aucun envoi.");
        logEvent("reject  reason=no-image  host=" + host);
        return false;
    }
    const QString name = blockValue(block, "container");
    if (name.isEmpty()) {
        warn("Champ figé manquant : container. Aucun envoi.");
        logEvent("reject  reason=no-container  host=" + host);
        return false;
    }
    if (host.isEmpty()) {
        warn("Aucun projet à monter pour " + name);
        return false;
    }
    const QString mount = containerRoot().isEmpty() ? "/work/project" : containerRoot();

    int existsCode = 1;
    run("docker", {"inspect", name}, 8000, &existsCode);
    const bool exists = existsCode == 0;

    bool running = false;
    bool sameMount = false;
    QString network, hostname, user, workdir;
    QStringList extraEnv, extraVolumes, labels, aliases, exposes;
    QString shm, restart;
    QString healthCmd, healthInterval, healthTimeout, healthRetries, healthStart;

    auto field = [this, &name](const QString &tmpl) -> QString {
        int code = 1;
        const QString v = run("docker", {"inspect", "-f", tmpl, name}, 8000, &code).trimmed();
        if (code != 0 || v.isEmpty() || v == "<no value>" || v == "[]" || v == "map[]" || v == "<nil>")
            return {};
        return v;
    };

    if (exists) {
        const QString state = field("{{.State.Running}}");
        running = state == "true";
        const QString mounted = field("{{range .Mounts}}{{if eq .Destination \"/work/project\"}}{{.Source}}{{end}}{{end}}");
        sameMount = mountsMatch(mounted, host);

        network = field("{{range $k, $v := .NetworkSettings.Networks}}{{$k}}{{end}}");
        hostname = field("{{.Config.Hostname}}");
        user = field("{{.Config.User}}");
        workdir = field("{{.Config.WorkingDir}}");
        shm = field("{{.HostConfig.ShmSize}}");
        restart = field("{{.HostConfig.RestartPolicy.Name}}");

        const QString envRaw = field("{{range .Config.Env}}{{.}}\\n{{end}}");
        for (const QString &line : envRaw.split('\n', Qt::SkipEmptyParts)) {
            const int eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            const QString key = line.left(eq);
            if (key.isEmpty() || key.contains(' ') || key.contains('='))
                continue;
            extraEnv << line;
        }

        // Délimiteur '|' : un chemin Windows (C:/...) ne casse plus le split.
        const QString mountsRaw = field("{{range .Mounts}}{{if ne .Destination \"/work/project\"}}{{if .Name}}{{.Name}}{{else}}{{.Source}}{{end}}|{{.Destination}}\\n{{end}}{{end}}");
        for (const QString &line : mountsRaw.split('\n', Qt::SkipEmptyParts)) {
            const int bar = line.indexOf('|');
            if (bar <= 0)
                continue;
            const QString src = line.left(bar);
            const QString dst = line.mid(bar + 1);
            if (src.isEmpty() || dst.isEmpty() || !dst.startsWith('/'))
                continue;
            if (dst == "/home/mpiuser/.cline" || dst == "/root/.cline" || dst.endsWith("/.cline"))
                continue;
            // Bind Windows (C:/...) : Docker le coupe sur ':'. On ne le propage pas.
            if (src.size() >= 2 && src.at(1) == ':')
                continue;
            extraVolumes << src + ":" + dst;
        }
        const QString labelsRaw = field("{{range $k, $v := .Config.Labels}}{{$k}}={{$v}}\\n{{end}}");
        for (const QString &line : labelsRaw.split('\n', Qt::SkipEmptyParts))
            if (line.contains('='))
                labels << line;
        const QString aliasRaw = field("{{range .NetworkSettings.Networks}}{{range .Aliases}}{{.}}\\n{{end}}{{end}}");
        for (const QString &line : aliasRaw.split('\n', Qt::SkipEmptyParts))
            if (!line.isEmpty() && line != name)
                aliases << line;
        const QString exposeRaw = field("{{range $p, $v := .Config.ExposedPorts}}{{$p}}\\n{{end}}");
        for (const QString &line : exposeRaw.split('\n', Qt::SkipEmptyParts)) {
            const QString port = line.split('/').first();
            if (!port.isEmpty())
                exposes << port;
        }
        if (!field("{{.Config.Healthcheck}}").isEmpty()) {
            healthCmd = field("{{range .Config.Healthcheck.Test}}{{.}} {{end}}");
            healthInterval = field("{{.Config.Healthcheck.Interval}}");
            healthTimeout = field("{{.Config.Healthcheck.Timeout}}");
            healthRetries = field("{{.Config.Healthcheck.Retries}}");
            healthStart = field("{{.Config.Healthcheck.StartPeriod}}");
        }
    }

    if (exists && running && sameMount) {
        logEvent("ok  container=" + name + "  host=" + host + "  mount=" + mount);
        return true;
    }

    if (exists) {
        const QString why = running ? "mauvais montage" : "arrêté";
        run("docker", {"rm", "-f", name}, 20000);
        logEvent("rm  container=" + name + "  reason=" + why);
    }

    QStringList args = {"run", "-d", "--name", name};
    if (!network.isEmpty() && network != "bridge")
        args << "--network" << network;
    if (!hostname.isEmpty())
        args << "--hostname" << hostname;
    if (!user.isEmpty())
        args << "--user" << user;
    if (!restart.isEmpty() && restart != "no")
        args << "--restart" << restart;
    if (!shm.isEmpty() && shm != "0" && shm != "67108864")
        args << "--shm-size" << shm;
    for (const QString &a : aliases)
        args << "--network-alias" << a;
    for (const QString &l : labels)
        args << "--label" << l;
    for (const QString &p : exposes)
        args << "--expose" << p;
    if (!healthCmd.isEmpty() && healthCmd != "NONE") {
        args << "--health-cmd" << healthCmd;
        if (!healthInterval.isEmpty() && healthInterval != "0")
            args << "--health-interval" << healthInterval;
        if (!healthTimeout.isEmpty() && healthTimeout != "0")
            args << "--health-timeout" << healthTimeout;
        if (!healthRetries.isEmpty() && healthRetries != "0")
            args << "--health-retries" << healthRetries;
        if (!healthStart.isEmpty() && healthStart != "0")
            args << "--health-start-period" << healthStart;
    }
    for (const QString &e : extraEnv)
        args << "-e" << e;
    args << "-v" << QDir::fromNativeSeparators(host) + ":" + mount;
    args << "-v" << "cline-config:/home/mpiuser/.cline";
    for (const QString &v : extraVolumes)
        args << "-v" << v;
    args << "-w" << (workdir.isEmpty() ? mount : workdir);
    args << image << "sleep" << "infinity";

    int code = 1;
    const QString out = run("docker", args, 60000, &code);
    if (code != 0) {
        warn("Recréation échouée pour " + name + " :\n" + out);
        logEvent("reject  reason=docker-run  container=" + name);
        return false;
    }

    int cfgCode = 1;
    run("docker", {"exec", name, "bash", "--noprofile", "--norc", "-c",
        "test -s /home/mpiuser/.cline/data/settings/providers.json || test -s /home/mpiuser/.cline/data/secrets.json"},
        8000, &cfgCode);
    const QString cfg = cfgCode == 0 ? "présente" : "absente";
    info("Conteneur recréé : " + name
        + "\nMontage : " + host + " → " + mount
        + "\nRéseau : " + (network.isEmpty() ? "bridge" : network)
        + "\nClé Z.ai : " + cfg);
    logEvent("ok  container=" + name + "  host=" + host + "  mount=" + mount
        + "  net=" + (network.isEmpty() ? "bridge" : network) + "  cline=" + cfg);
    return true;
}

void LiteIdeAfPlugin::sealSnapshot()
{
    QDir().mkpath(storeDir());
    QSaveFile snap(snapshotPath());
    if (!snap.open(QIODevice::WriteOnly))
        return;
    snap.write(frozenCanonical().toUtf8());
    snap.commit();
    writeOk();
}

void LiteIdeAfPlugin::writeOk()
{
    const QString canonical = frozenCanonical();
    const QByteArray sha = QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex();
    QStringList engines;
    const QRegularExpression names("^\\s*- name:\\s*(\\S+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = names.globalMatch(yamlText());
    while (it.hasNext())
        engines << it.next().captured(1);
    logEvent("ok  sha256:" + QString::fromLatin1(sha.left(12))
        + "  hmac:" + hmacOf(canonical).left(12)
        + "  engines=" + engines.join(",")
        + "  host=" + currentProjectRoot());
}

bool LiteIdeAfPlugin::preValidate()
{
    if (yamlText().isEmpty()) {
        warn("Contrat illisible. Aucun envoi.");
        logEvent("reject  reason=unreadable  host=" + currentProjectRoot());
        return false;
    }
    verifyEngines();
    QDir().mkpath(storeDir());
    if (!QFileInfo(snapshotPath()).isFile()) {
        QSaveFile snap(snapshotPath());
        if (snap.open(QIODevice::WriteOnly)) {
            snap.write(frozenCanonical().toUtf8());
            snap.commit();
        }
        logEvent("ok  hmac:" + hmacOf(frozenCanonical()).left(12) + "  host=" + currentProjectRoot());
    }
    writeOk();
    return true;
}

bool LiteIdeAfPlugin::postValidate()
{
    const QString canonical = frozenCanonical();
    const QString mac = hmacOf(canonical);
    QFile snap(snapshotPath());
    if (!snap.open(QIODevice::ReadOnly)) {
        warn("Pas d'état de référence. Realignez.");
        logEvent("reject  reason=no-snapshot");
        return false;
    }
    const QString previous = QString::fromUtf8(snap.readAll());
    if (previous == canonical)
        return true;
    const QStringList now = canonical.split('\n');
    const QStringList was = previous.split('\n');
    for (const QString &line : now) {
        if (!was.contains(line)) {
            const QString field = line.section('=', 0, 0);
            QString scope = "companion";
            if (field == "endpoint" || field == "image" || field == "engine")
                scope = "engine:" + line.section('=', 1, 1).section('\n', 0, 0);
            warn("Contrat altéré : " + line + ". Aucun envoi. Alignez le projet pour resceller.");
            logEvent("corrupted  scope=" + scope + "  field=" + field + "  reason=hmac-mismatch");
            return false;
        }
    }
    return true;
}

QString LiteIdeAfPlugin::yamlText() const
{
    QFile f(contractPath());
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

bool LiteIdeAfPlugin::dryRun() const
{
    return scalar(yamlText(), "dryRun") == "true";
}

QString LiteIdeAfPlugin::activeCline() const
{
    const QString text = yamlText();
    const int at = text.indexOf("projects:");
    const QString named = scalar(at >= 0 ? text.mid(at) : text, "engine");
    if (!named.isEmpty())
        return named;
    const QRegularExpression re("^\\s*- name:\\s*(\\S+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (blockValue(engineBlock(name), "kind") == "agent")
            return name;
    }
    return QString();
}

QString LiteIdeAfPlugin::engineBlock(const QString &name) const
{
    const QString text = yamlText();
    const QString needle = "- name: " + name;
    const int at = text.indexOf(needle);
    if (at < 0)
        return QString();
    const int next = text.indexOf("\n  - name:", at + needle.size());
    return next < 0 ? text.mid(at) : text.mid(at, next - at);
}

QString LiteIdeAfPlugin::blockValue(const QString &block, const QString &key) const
{
    return scalar(block, key);
}

QString LiteIdeAfPlugin::globalPrompt() const
{
    return folded(yamlText(), "systemPrompt");
}

QString LiteIdeAfPlugin::localPrompt(const QString &engine) const
{
    return folded(engineBlock(engine), "systemPrompt");
}

QString LiteIdeAfPlugin::hostRoot() const
{
    return scalar(engineBlock(activeCline()), "host");
}

QString LiteIdeAfPlugin::containerRoot() const
{
    const QString block = engineBlock(activeCline());
    const QRegularExpression re("^\\s*container:\\s*(/\\S+)", QRegularExpression::MultilineOption);
    const QRegularExpressionMatch m = re.match(block);
    if (m.hasMatch())
        return m.captured(1);
    return "/work/project";
}

QString LiteIdeAfPlugin::scopeText() const
{
    QStringList lines;
    if (m_app->fileManager()) {
        const QStringList folders = m_app->fileManager()->folderList();
        if (!folders.isEmpty())
            lines << "repo: " + folders.first();
    }
    LiteApi::IEditor *ed = m_app->editorManager() ? m_app->editorManager()->currentEditor() : nullptr;
    if (ed)
        lines << "current: " + ed->filePath();
    LiteApi::ILiteEditor *lite = LiteApi::getLiteEditor(ed);
    if (lite) {
        const QString sel = lite->textCursor().selectedText();
        if (!sel.isEmpty())
            lines << "selection:\n" + sel.left(2000);
    }
    if (lines.isEmpty())
        lines << "portée vide";
    return lines.join("\n");
}

bool LiteIdeAfPlugin::editorDirty() const
{
    if (!m_app->editorManager())
        return false;
    const QList<LiteApi::IEditor *> eds = m_app->editorManager()->editorList();
    for (LiteApi::IEditor *ed : eds) {
        if (ed && ed->isModified())
            return true;
    }
    return false;
}

bool LiteIdeAfPlugin::gitDirty(const QString &root) const
{
    if (root.isEmpty())
        return false;
    int code = 1;
    const QString out = run("git", {"-C", root, "status", "--porcelain"}, 8000, &code);
    return code != 0 || !out.trimmed().isEmpty();
}

QString LiteIdeAfPlugin::run(const QString &program, const QStringList &args, int timeoutMs, int *code) const
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(timeoutMs)) {
        if (code)
            *code = -1;
        return QString::fromUtf8(p.readAllStandardError());
    }
    if (code)
        *code = p.exitCode();
    return QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError());
}

void LiteIdeAfPlugin::showAnswer(const QString &engine, const QString &block, const QString &response, const QString &thinking) const
{
    const bool showThinking = blockValue(block, "thinking_progress") == "true";
    const bool renderMarkdown = blockValue(block, "rendering_markdown") == "true";
    QString md = response;
    if (showThinking && !thinking.trimmed().isEmpty())
        md = thinking + "\n\n---\n\n" + response;

    QDialog dialog(m_app->mainWindow());
    dialog.setWindowTitle("AgentPlurAI - Réponse " + engine);
    dialog.resize(800, 600);
    auto *layout = new QVBoxLayout(&dialog);
    auto *textEdit = new QTextEdit(&dialog);
    textEdit->setReadOnly(true);
    if (renderMarkdown)
        textEdit->setMarkdown(md);
    else
        textEdit->setPlainText(md);
    layout->addWidget(textEdit);
    const int chars = md.size();
    auto *sizeLabel = new QLabel(
        QString("Reçu : %1 caractères%2")
            .arg(QLocale(QLocale::French).toString(chars))
            .arg(showThinking ? "" : " (réflexion masquée)"),
        &dialog);
    layout->addWidget(sizeLabel);
    dialog.exec();
}

void LiteIdeAfPlugin::info(const QString &text) const
{
    QMessageBox::information(m_app->mainWindow(), "AgentPlurAI", text.left(6000));
}

void LiteIdeAfPlugin::warn(const QString &text) const
{
    QMessageBox::warning(m_app->mainWindow(), "AgentPlurAI", text.left(6000));
}

bool LiteIdeAfPlugin::load(LiteApi::IApplication *app)
{
    if (!app)
        return false;
    m_app = app;
    QMenu *menu = app->actionManager()->insertMenu("menu/liteideaf", "AgentPlurAI");
    if (!menu)
        return true;

    auto add = [this, menu](const QString &label, void (LiteIdeAfPlugin::*slot)()) {
        QAction *act = menu->addAction(label);
        connect(act, &QAction::triggered, this, slot);
    };
    add("Portée", &LiteIdeAfPlugin::showScope);
    add("Prompt", &LiteIdeAfPlugin::sendPrompt);
    add("Historique", &LiteIdeAfPlugin::showHistory);
    add("Journal", &LiteIdeAfPlugin::showJournal);
    add("Recharger", &LiteIdeAfPlugin::reloadWrites);

    QMenu *paramMenu = menu->addMenu("Paramétrage");
    auto addParam = [this, paramMenu](const QString &label, void (LiteIdeAfPlugin::*slot)()) {
        QAction *act = paramMenu->addAction(label);
        connect(act, &QAction::triggered, this, slot);
    };
    addParam("IA par défaut", &LiteIdeAfPlugin::setDefaultAI);
    addParam("Aligner le projet", &LiteIdeAfPlugin::alignProject);
    addParam("System prompt", &LiteIdeAfPlugin::editSystemPrompt);
    m_hideThinkingAction = paramMenu->addAction("Masquage réflexion : ok");
    m_hideThinkingAction->setCheckable(true);
    connect(m_hideThinkingAction, &QAction::triggered, this, &LiteIdeAfPlugin::toggleHideThinking);
    connect(paramMenu, &QMenu::aboutToShow, this, [this]() {
        const QString block = engineBlock(activeCline());
        const bool hidden = blockValue(block, "thinking_progress") != "true";
        m_hideThinkingAction->setChecked(hidden);
        m_hideThinkingAction->setText(hidden ? "Masquage réflexion : ok" : "Masquage réflexion : non");
    });
    addParam("Santé", &LiteIdeAfPlugin::health);
    addParam("Quota Cline", &LiteIdeAfPlugin::showQuota);
    return true;
}

QString LiteIdeAfPlugin::currentProjectRoot() const
{
    if (m_app->projectManager()) {
        LiteApi::IProject *project = m_app->projectManager()->currentProject();
        if (project && !project->filePath().isEmpty())
            return QFileInfo(project->filePath()).absolutePath();
    }
    if (m_app->fileManager()) {
        const QStringList folders = m_app->fileManager()->folderList();
        if (!folders.isEmpty())
            return folders.first();
    }
    if (m_app->editorManager()) {
        LiteApi::IEditor *ed = m_app->editorManager()->currentEditor();
        if (ed && !ed->filePath().isEmpty())
            return QFileInfo(ed->filePath()).absolutePath();
    }
    return QString();
}

QString LiteIdeAfPlugin::declaredHost() const
{
    return scalar(engineBlock(activeCline()), "host");
}

bool LiteIdeAfPlugin::updateDeclaredHost(const QString &host)
{
    const QString path = contractPath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        warn("YAML illisible : " + path);
        return false;
    }
    QString text = QString::fromUtf8(f.readAll());
    f.close();
    const QString normalized = QDir::fromNativeSeparators(host);
    const QRegularExpression hostRe("(^|\\n)([ \\t]*)host:[^\\n]*");
    const QRegularExpression pathRe("(^|\\n)([ \\t]*)path:[^\\n]*");
    if (!hostRe.match(text).hasMatch()) {
        text += "\nhost: " + normalized + "\n";
    } else {
        text.replace(hostRe, "\\1\\2host: " + normalized);
    }
    if (pathRe.match(text).hasMatch())
        text.replace(pathRe, "\\1\\2path: " + normalized);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        warn("YAML non écrivable : " + path);
        return false;
    }
    f.write(text.toUtf8());
    f.flush();
    return true;
}

bool LiteIdeAfPlugin::recreateContainer(const QString &host)
{
    const QString engine = activeCline();
    const QString block = engineBlock(engine);
    const QString container = blockValue(block, "container").isEmpty() ? engine : blockValue(block, "container");
    const QString image = blockValue(block, "image");
    const QString composeFile = scalar(yamlText(), "composeFile");
    if (!composeFile.isEmpty()) {
        QProcess p;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PROJECT_DIR", QDir::toNativeSeparators(host));
        p.setProcessEnvironment(env);
        p.setWorkingDirectory(QFileInfo(composeFile).absolutePath());
        p.start("docker", {"compose", "-f", composeFile, "up", "-d", "--force-recreate"});
        if (!p.waitForFinished(120000) || p.exitCode() != 0) {
            warn(QString::fromUtf8(p.readAllStandardError()));
            return false;
        }
        info("Conteneur recréé via compose : " + container + "\nProjet : " + host);
        return true;
    }
    if (image.isEmpty()) {
        warn("Pas de composeFile ni d'image dans le YAML : recréation impossible.");
        return false;
    }
    run("docker", {"stop", container}, 30000);
    run("docker", {"rm", container}, 15000);
    const QString mount = QDir::toNativeSeparators(host) + ":" + containerRoot();
    int code = -1;
    const QString out = run("docker", {"run", "-d", "--name", container,
        "-v", mount, "-w", containerRoot(), image}, 60000, &code);
    if (code != 0) {
        warn(out);
        return false;
    }
    info("Conteneur recréé : " + container + "\nMontage : " + host + " → " + containerRoot());
    return true;
}

void LiteIdeAfPlugin::alignProject()
{
    const QString live = currentProjectRoot();
    if (live.isEmpty()) {
        warn("Aucun projet actif. Alignez depuis un dossier ouvert.");
        return;
    }
    if (!ensureContract())
        return;

    // Toujours valider le conteneur, même si le YAML est déjà bon.
    if (!ensureContainer(live))
        return;

    if (!updateDeclaredHost(live))
        warn("YAML non mis à jour (" + contractPath() + "). Conteneur aligné quand même.");
    sealSnapshot();
    info("Conteneur aligné sur :\n" + live + "\nContrat rescellé.");
}

void LiteIdeAfPlugin::toggleHideThinking()
{
    if (!ensureContract())
        return;
    const QString path = contractPath();
    QString text = yamlText();
    if (text.isEmpty()) {
        warn("YAML illisible : " + path);
        return;
    }
    const bool hide = m_hideThinkingAction->isChecked();
    const QString value = hide ? "false" : "true";
    if (!text.contains("thinking_progress:"))
        text += "\nthinking_progress: " + value + "\n";
    else
        text.replace(QRegularExpression("thinking_progress:\\s*\\S+"), "thinking_progress: " + value);
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit()) {
        warn("YAML non écrivable : " + path);
        return;
    }
    m_hideThinkingAction->setText(hide ? "Masquage réflexion : ok" : "Masquage réflexion : non");
    info(hide ? "Réflexion masquée." : "Réflexion affichée.");
}

void LiteIdeAfPlugin::setDefaultAI()
{
    QStringList engines;
    QRegularExpression re("^\\s*- name:\\s*(\\S+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = re.globalMatch(yamlText());
    while (it.hasNext())
        engines << it.next().captured(1);
    if (engines.isEmpty()) {
        warn("Aucun moteur défini dans le contrat YAML.");
        return;
    }
    bool ok = false;
    const QString current = m_defaultEngine.isEmpty() ? activeCline() : m_defaultEngine;
    int currentIndex = engines.indexOf(current);
    if (currentIndex < 0)
        currentIndex = 0;
    const QString choice = QInputDialog::getItem(
        m_app->mainWindow(), "AgentPlurAI", "Choisir l'IA par défaut :", engines, currentIndex, false, &ok);
    if (ok && !choice.isEmpty()) {
        m_defaultEngine = choice;
        info("L'IA par défaut est désormais fixée sur : " + m_defaultEngine);
    }
}

void LiteIdeAfPlugin::health()
{
    QStringList lines;
    lines << "Plugin : " + QString(LITEIDEAF_VERSION);
    if (yamlText().isEmpty()) {
        lines << "YAML : introuvable (" + contractPath() + ")";
        info(lines.join("\n"));
        return;
    }
    const QString cline = activeCline();
    if (cline.isEmpty())
        lines << "Cline actif : aucun";
    else if (dryRun())
        lines << "Cline " + cline + " : dryRun, aucune socket";
    else {
        int code = -1;
        const QString out = run("docker", {"inspect", "-f", "{{.State.Status}}", cline}, 8000, &code);
        lines << (code == 0 ? "Cline " + cline + " : " + out.trimmed()
                            : "Cline " + cline + " : conteneur absent");
    }
    const QString text = yamlText();
    int from = 0;
    while (true) {
        const int at = text.indexOf("endpoint:", from);
        if (at < 0)
            break;
        const QString ep = scalar(text.mid(at), "endpoint");
        from = at + 9;
        if (ep.isEmpty())
            continue;
        if (dryRun()) {
            lines << "Ollama " + ep + " : dryRun";
            continue;
        }
        QNetworkAccessManager nam;
        QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(ep + "/api/tags")));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        lines << "Ollama " + ep + " : " + (reply->error() == QNetworkReply::NoError ? "ok" : reply->errorString());
        reply->deleteLater();
    }
    info(lines.join("\n"));
}

void LiteIdeAfPlugin::showScope()
{
    info(scopeText());
}

void LiteIdeAfPlugin::sendPrompt()
{
    if (!ensureContract() || !preValidate() || !postValidate())
        return;
    const QString live = currentProjectRoot();
    if (!live.isEmpty() && blockValue(engineBlock(activeCline()), "kind") == "agent") {
        if (!ensureContainer(live))
            return;
        if (!ensureGitBranch(live))
            warn("Branche agent non créée.");
    }
    const QString engine = m_defaultEngine.isEmpty() ? activeCline() : m_defaultEngine;
    if (engine.isEmpty()) {
        warn("Aucun moteur actif. Réglez d'abord l'IA par défaut.");
        return;
    }

    LiteApi::IEditor *ed = m_app->editorManager() ? m_app->editorManager()->currentEditor() : nullptr;
    LiteApi::ILiteEditor *lite = LiteApi::getLiteEditor(ed);
    const QString selection = lite ? lite->textCursor().selectedText() : QString();
    const QString filePath = ed ? ed->filePath() : QString();
    const QString fileBody = (lite && lite->document()) ? lite->document()->toPlainText() : QString();
    QString projectRoot;
    if (m_app->projectManager() && m_app->projectManager()->currentProject())
        projectRoot = QFileInfo(m_app->projectManager()->currentProject()->filePath()).absolutePath();
    if (projectRoot.isEmpty() && m_app->fileManager() && !m_app->fileManager()->folderList().isEmpty())
        projectRoot = m_app->fileManager()->folderList().first();

    QDialog ask(m_app->mainWindow());
    ask.setWindowTitle("AgentPlurAI - Cible : " + engine);
    ask.resize(640, 420);
    auto *layout = new QVBoxLayout(&ask);
    auto *selBtn = new QRadioButton("Sélection courante", &ask);
    auto *fileBtn = new QRadioButton("Fichier courant", &ask);
    auto *projBtn = new QRadioButton("Projet", &ask);
    selBtn->setEnabled(!selection.isEmpty());
    fileBtn->setEnabled(!filePath.isEmpty());
    projBtn->setEnabled(!projectRoot.isEmpty());
    if (selBtn->isEnabled())
        selBtn->setChecked(true);
    else if (fileBtn->isEnabled())
        fileBtn->setChecked(true);
    else
        projBtn->setChecked(true);
    layout->addWidget(selBtn);
    layout->addWidget(fileBtn);
    layout->addWidget(projBtn);
    auto *edit = new QTextEdit(&ask);
    layout->addWidget(edit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &ask);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &ask, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &ask, &QDialog::reject);
    if (ask.exec() != QDialog::Accepted || edit->toPlainText().trimmed().isEmpty())
        return;
    const QString task = edit->toPlainText();

    QString scope;
    if (selBtn->isChecked())
        scope = "selection:\n" + filePath + "\n" + selection;
    else if (fileBtn->isChecked())
        scope = "fichier:\n" + filePath + "\n" + fileBody;
    else
        scope = "projet:\n" + projectRoot;
    const QString body = globalPrompt() + "\n\n" + localPrompt(engine) + "\n\n" + scope + "\n\n" + task;
    if (dryRun()) {
        info("dryRun, non envoyé.\n" + body);
        return;
    }
    const QString block = engineBlock(engine);
    if (block.isEmpty()) {
        warn("Moteur non trouvé dans le YAML : " + engine);
        return;
    }

    m_pendingEngine = engine;
    m_pendingBlock = block;

    const bool model = blockValue(block, "kind") == "model" || !blockValue(block, "endpoint").isEmpty();
    if (model) {
        showLoadingDialog(engine);
        const QString ep = blockValue(block, "endpoint");
        QJsonObject msg;
        msg["model"] = blockValue(block, "model").isEmpty() ? "qwen2.5" : blockValue(block, "model");
        msg["stream"] = false;
        msg["prompt"] = body;
        auto *nam = new QNetworkAccessManager(this);
        QNetworkRequest req(QUrl(ep + "/api/generate"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        m_pendingReply = nam->post(req, QJsonDocument(msg).toJson());
        connect(m_pendingReply, &QNetworkReply::finished, this, &LiteIdeAfPlugin::onOllamaFinished);
        return;
    }

    const bool refuseIfDirty = scalar(yamlText(), "refuseIfDirty") != "false";
    if (refuseIfDirty && (editorDirty() || gitDirty(live))) {
        warn("Arbre de travail sale. Commit ou rangez avant d’envoyer.");
        logEvent("reject  reason=dirty  host=" + live);
        return;
    }
    if (!ensureContainer(live))
        return;
    showLoadingDialog(engine);
    const QString container = blockValue(block, "container");
    if (container.isEmpty()) {
        hideLoadingDialog();
        warn("Champ figé manquant : container. Aucun envoi.");
        return;
    }
    const QString inBox = "/tmp/agentplurai-prompt.txt";
    const QString logDir = containerRoot() + "/.agentplurai";
    run("docker", {"exec", container, "mkdir", "-p", logDir}, 8000);
    QProcess tee;
    tee.start("docker", {"exec", "-i", container, "tee", inBox});
    tee.waitForStarted(5000);
    tee.write(body.toUtf8());
    tee.closeWriteChannel();
    tee.waitForFinished(8000);
    const QString log = logDir + "/prompts.log";
    const QString line = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + " " + engine + "\n" + task + "\n";
    QProcess logp;
    logp.start("docker", {"exec", "-i", container, "tee", "-a", log});
    logp.waitForStarted(5000);
    logp.write(line.toUtf8());
    logp.closeWriteChannel();
    logp.waitForFinished(8000);

    const QString script =
        "export PATH=\"/opt/venv/bin:/usr/local/go/bin:/usr/local/bin:/usr/bin:${PATH}\"; "
        "prompt=$(cat " + inBox + "); "
        "exec cline \"$prompt\" 2>/tmp/agentplurai-stderr.log";
    m_pendingProcess = new QProcess(this);
    connect(m_pendingProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LiteIdeAfPlugin::onClineFinished);
    m_pendingProcess->start("docker", {"exec", "-w", containerRoot(), container, "bash", "--noprofile", "--norc", "-c", script});
}

void LiteIdeAfPlugin::onOllamaFinished()
{
    if (!m_pendingReply)
        return;
    const QByteArray raw = m_pendingReply->readAll();
    const bool netOk = m_pendingReply->error() == QNetworkReply::NoError;
    const QString netErr = m_pendingReply->errorString();
    m_pendingReply->deleteLater();
    m_pendingReply = nullptr;
    hideLoadingDialog();
    if (!netOk) {
        warn(netErr);
        return;
    }
    const QString answer = QString::fromUtf8(raw);
    QJsonParseError parseError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(answer.toUtf8(), &parseError);
    QString responseText = answer;
    QString thinkingText;
    if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
        responseText = jsonDoc.object().value("response").toString();
        thinkingText = jsonDoc.object().value("thinking").toString();
    }
    showAnswer(m_pendingEngine, m_pendingBlock, responseText, thinkingText);
    postValidate();
}

void LiteIdeAfPlugin::onClineFinished(int, int)
{
    if (!m_pendingProcess)
        return;
    const QString out = QString::fromUtf8(m_pendingProcess->readAllStandardOutput());
    const QString err = QString::fromUtf8(m_pendingProcess->readAllStandardError());
    m_pendingProcess->deleteLater();
    m_pendingProcess = nullptr;
    hideLoadingDialog();
    QString response;
    QString thinking;
    splitCline(out, &response, &thinking);
    if (!err.trimmed().isEmpty())
        response += "\n\n---\nstderr:\n" + err.trimmed();
    showAnswer(m_pendingEngine, m_pendingBlock, response.isEmpty() ? "Cline sans sortie" : response, thinking);
    postValidate();
}

void LiteIdeAfPlugin::showHistory()
{
    if (dryRun()) {
        info("dryRun : pas de lecture de l'historique.");
        return;
    }
    const QString engine = m_defaultEngine.isEmpty() ? activeCline() : m_defaultEngine;
    if (engine.isEmpty()) {
        warn("Aucun moteur défini. Réglez d'abord l'IA par défaut.");
        return;
    }
    const QString block = engineBlock(engine);
    if (block.isEmpty()) {
        warn("Moteur non trouvé dans le contrat YAML : " + engine);
        return;
    }
    const bool isModel = blockValue(block, "kind") == "model" || !blockValue(block, "endpoint").isEmpty();
    QString out;
    if (isModel) {
        out = "Historique non disponible pour " + engine + " : appel HTTP sans journal.";
    } else {
        const QString container = blockValue(engineBlock(activeCline()), "container");
        const QString path = containerRoot() + "/.agentplurai/prompts.log";
        out = container.isEmpty()
            ? "Pas de clé container pour " + engine
            : run("docker", {"exec", container, "cat", path}, 8000);
        if (out.trimmed().isEmpty())
            out = "Historique vide ou absent : " + path;
    }
    info(out);
}

void LiteIdeAfPlugin::showJournal()
{
    if (dryRun()) {
        info("dryRun : pas de lecture du journal.");
        return;
    }
    const QString engine = m_defaultEngine.isEmpty() ? activeCline() : m_defaultEngine;
    if (engine.isEmpty()) {
        warn("Aucun moteur défini. Réglez d'abord l'IA par défaut.");
        return;
    }
    const QString block = engineBlock(engine);
    if (block.isEmpty()) {
        warn("Moteur non trouvé dans le contrat YAML : " + engine);
        return;
    }
    const bool isModel = blockValue(block, "kind") == "model" || !blockValue(block, "endpoint").isEmpty();
    QString out;
    if (isModel) {
        out = "Journal non disponible pour " + engine + " : modèle HTTP sans actions.";
    } else {
        const QString container = blockValue(engineBlock(activeCline()), "container");
        const QString path = containerRoot() + "/.agentplurai/actions.log";
        out = container.isEmpty()
            ? "Pas de clé container pour " + engine
            : run("docker", {"exec", container, "cat", path}, 8000);
        if (out.trimmed().isEmpty())
            out = "Journal vide ou absent : " + path;
    }
    info(out);
}

void LiteIdeAfPlugin::editSystemPrompt()
{
    bool ok = false;
    const QString which = QInputDialog::getItem(
        m_app->mainWindow(), "AgentPlurAI", "Niveau", {"global", activeCline()}, 0, false, &ok);
    if (!ok)
        return;
    const QString current = which == "global" ? globalPrompt() : localPrompt(which);
    const QString next = QInputDialog::getMultiLineText(m_app->mainWindow(), "AgentPlurAI", which, current, &ok);
    if (!ok)
        return;
    if (QMessageBox::question(m_app->mainWindow(), "AgentPlurAI", "Écrire ce system prompt dans le YAML ?") != QMessageBox::Yes)
        return;

    const QString path = contractPath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        warn("YAML illisible : " + path);
        return;
    }
    QString text = QString::fromUtf8(f.readAll());
    f.close();
    QString folded = "systemPrompt: |\n";
    for (const QString &row : next.split('\n'))
        folded += "  " + row + "\n";
    const QRegularExpression block("(^|\\n)([ \\t]*)systemPrompt:\\s*\\|[\\s\\S]*?(?=\\n[^ \\t\\n]|\\n*$)");
    if (which == "global") {
        const int at = text.indexOf("systemPrompt:");
        if (at < 0) {
            warn("systemPrompt global absent.");
            return;
        }
        const int end = text.indexOf("\nengines:", at);
        if (end < 0) {
            warn("Bloc engines absent, fusion annulée.");
            return;
        }
        text.replace(at, end - at, folded);
    } else {
        const QString needle = "- name: " + which;
        const int at = text.indexOf(needle);
        if (at < 0) {
            warn("Moteur absent : " + which);
            return;
        }
        const int prompt = text.indexOf("systemPrompt:", at);
        if (prompt < 0) {
            warn("systemPrompt local absent.");
            return;
        }
        const int nextName = text.indexOf("\n  - name:", prompt);
        const int end = nextName < 0 ? text.size() : nextName;
        text.replace(prompt, end - prompt, folded.trimmed() + "\n");
    }
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        warn("YAML non écrivable : " + path);
        return;
    }
    f.write(text.toUtf8());
    f.flush();
    info("System prompt écrit dans " + path);
}

void LiteIdeAfPlugin::showQuota()
{
    if (dryRun()) {
        info("dryRun : quota non interrogé.");
        return;
    }
    const QString container = blockValue(engineBlock(activeCline()), "container");
    if (container.isEmpty()) {
        warn("Pas de clé container pour le moteur actif.");
        return;
    }
    const QString script =
        "set -e; "
        "f=/root/.cline/data/settings/providers.json; "
        "[ -f \"$f\" ] || f=/root/.cline/data/secrets.json; "
        "if [ ! -f \"$f\" ]; then echo 'Aucun fichier de clé dans /root/.cline'; exit 0; fi; "
        "key=$(node -e '"
        "const fs=require(\"fs\");"
        "const j=JSON.parse(fs.readFileSync(process.argv[1],\"utf8\"));"
        "const keys=[];"
        "function walk(o){if(!o||typeof o!==\"object\")return;"
        "for(const [k,v] of Object.entries(o)){"
        "if(typeof v===\"string\"&&/key|token|secret/i.test(k)&&v.length>8)keys.push(v);"
        "else walk(v);}};"
        "walk(j);process.stdout.write(keys[0]||\"\");"
        "' \"$f\"); "
        "if [ -z \"$key\" ]; then echo 'Clé introuvable dans /root/.cline'; exit 0; fi; "
        "curl -fsS -H \"Authorization: Bearer $key\" -H \"Accept: application/json\" "
        "https://api.z.ai/api/monitor/usage/quota/limit "
        "|| curl -fsS -H \"Authorization: $key\" -H \"Accept: application/json\" "
        "https://api.z.ai/api/monitor/usage/quota/limit";
    const QString raw = run("docker", {"exec", container, "bash", "-lc", script}, 20000);
    const QString out = raw;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw.trimmed().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        info("Quota Z.ai (sortie brute) :\n" + raw.left(4000));
        return;
    }
    const QJsonObject data = doc.object().value("data").toObject();
    const QJsonArray limits = data.value("limits").toArray();
    QStringList lines;
    lines << "Quota Z.ai — ce qu'il reste sur le plan";
    if (limits.isEmpty()) {
        lines << raw.left(2000);
        info(lines.join("\n"));
        return;
    }
    auto windowOf = [](const QJsonObject &o) {
        const int unit = o.value("unit").toInt();
        const int number = o.value("number").toInt(1);
        if (unit == 3 && number == 5) return QString("fenêtre glissante de 5 heures");
        if (unit == 6) return QString("fenêtre de %1 jours").arg(number);
        if (unit == 5) return QString("quota mensuel");
        if (unit == 1) return QString("fenêtre de %1 jours").arg(number);
        if (number > 1) return QString("fenêtre de %1 unités").arg(number);
        return QString();
    };
    auto labelOf = [](const QString &type) {
        if (type == "CREDIT_LIMIT") return QString("Crédits");
        if (type == "TOKENS_LIMIT") return QString("Tokens");
        if (type == "TIME_LIMIT") return QString("Outils (recherche, lecteur)");
        return type.isEmpty() ? QString("Quota") : type;
    };
    auto fmt = [](double n) {
        return QLocale(QLocale::French).toString(qint64(n));
    };
    int index = 0;
    for (const QJsonValue &v : limits) {
        const QJsonObject o = v.toObject();
        ++index;
        const QString type = o.value("type").toString();
        const QString window = windowOf(o);
        const double pct = o.value("percentage").toDouble();
        const double total = o.value("usage").toDouble();
        const double left = o.value("remaining").toDouble();
        const double used = o.value("currentValue").toDouble();
        QString title = labelOf(type);
        if (!window.isEmpty())
            title += " — " + window;
        else if (limits.size() > 1)
            title += QString(" — enveloppe %1").arg(index);
        lines << "";
        lines << title;
        lines << QString("  Reste : %1 sur %2").arg(fmt(left), fmt(total));
        lines << QString("  Déjà consommé : %1 (%2 %)").arg(fmt(used)).arg(pct, 0, 'f', 0);
        if (o.contains("nextResetTime")) {
            const qint64 ms = qint64(o.value("nextResetTime").toDouble());
            if (ms > 0) {
                const QDateTime reset = QDateTime::fromMSecsSinceEpoch(ms);
                const qint64 mins = QDateTime::currentDateTime().secsTo(reset) / 60;
                QString when = reset.toString("dd/MM HH:mm");
                if (mins > 0)
                    when += QString(" (dans %1 h %2 min)").arg(mins / 60).arg(mins % 60);
                lines << "  Réinitialisation : " + when;
            }
        }
    }
    info(lines.join("\n"));
}

void LiteIdeAfPlugin::reloadWrites()
{
    if (dryRun()) {
        info("dryRun : rien à recharger.");
        return;
    }
    if (editorDirty()) {
        warn("Tampon sale : rechargement refusé.");
        return;
    }
    const QString container = blockValue(engineBlock(activeCline()), "container");
    if (container.isEmpty()) {
        warn("Pas de clé container pour le moteur actif.");
        return;
    }
    const QString journal = run("docker", {"exec", container, "cat", containerRoot() + "/.agentplurai/actions.log"}, 8000);
    QStringList files;
    const QRegularExpression re("write\\s+(\\S+)");
    QRegularExpressionMatchIterator it = re.globalMatch(journal);
    while (it.hasNext())
        files << it.next().captured(1);
    if (files.isEmpty()) {
        info("Aucune ligne write dans le journal.");
        return;
    }
    if (QMessageBox::question(m_app->mainWindow(), "AgentPlurAI", "Recharger ?\n" + files.join("\n")) != QMessageBox::Yes)
        return;
    const QString host = hostRoot();
    const QString box = containerRoot();
    for (const QString &path : files) {
        QString local = path;
        if (local.startsWith(box))
            local = host + local.mid(box.size());
        if (m_app->fileManager())
            m_app->fileManager()->openEditor(local);
    }
}
