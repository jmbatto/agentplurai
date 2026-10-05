#include "liteideafplugin.h"

#include "liteeditorapi/liteeditorapi.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

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
    QString think;
    QString body;
    bool inTool = false;
    const QStringList rows = clean.split('\n');
    for (const QString &row : rows) {
        const QString t = row.trimmed();
        const bool marker = t.contains("[thinking]") || t.contains("[run_commands]")
            || t.contains("[run_command]") || t.contains("[editor]") || t.startsWith("⎿");
        if (marker) {
            inTool = true;
            think += row + "\n";
            continue;
        }
        if (inTool && t.isEmpty()) {
            think += "\n";
            continue;
        }
        if (inTool && (t.startsWith("Analyse") || t.startsWith("#") || t.startsWith("**") || t.startsWith("| ") || t.startsWith("```")))
            inTool = false;
        if (inTool)
            think += row + "\n";
        else if (!(t.startsWith("```") && t.endsWith("```") && t.count("```") == 2))
            body += row + "\n";
        else
            body += t.mid(3, t.size() - 6) + "\n";
    }
    *thinking = think.trimmed();
    *response = body.trimmed();
    if (response->isEmpty())
        *response = clean.trimmed();
}

} // namespace

LiteIdeAfPlugin::LiteIdeAfPlugin()
    : m_defaultEngine()
    , m_loadingDialog(nullptr)
    , m_app(nullptr)
{
}

void LiteIdeAfPlugin::showLoadingDialog(const QString &engine)
{
    if (m_loadingDialog)
        return;
    m_loadingDialog = new QDialog(m_app->mainWindow());
    m_loadingDialog->setWindowTitle("AgentPlurAI");
    m_loadingDialog->setWindowModality(Qt::WindowModal);
    auto *layout = new QVBoxLayout(m_loadingDialog);
    auto *label = new QLabel("Envoi du prompt au moteur : " + engine + "\nVeuillez patienter...", m_loadingDialog);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    m_loadingDialog->resize(360, 100);
    m_loadingDialog->open();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void LiteIdeAfPlugin::hideLoadingDialog()
{
    if (!m_loadingDialog)
        return;
    m_loadingDialog->close();
    m_loadingDialog->deleteLater();
    m_loadingDialog = nullptr;
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

QString LiteIdeAfPlugin::contractPath() const
{
    if (!m_app || !m_app->settings())
        return "agentplurai.yaml";
    return m_app->settings()->value("agentplurai/contract", "agentplurai.yaml").toString();
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
    return scalar(at >= 0 ? text.mid(at) : text, "engine");
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
    return "/work/mpi";
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
    addParam("System prompt", &LiteIdeAfPlugin::editSystemPrompt);
    addParam("Santé", &LiteIdeAfPlugin::health);
    addParam("Quota Cline", &LiteIdeAfPlugin::showQuota);
    return true;
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
    if (yamlText().isEmpty()) {
        warn("YAML introuvable : " + contractPath());
        return;
    }
    QStringList lines;
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

    const bool model = blockValue(block, "kind") == "model" || !blockValue(block, "endpoint").isEmpty();
    if (model) {
        showLoadingDialog(engine);
        const QString ep = blockValue(block, "endpoint");
        QJsonObject msg;
        msg["model"] = blockValue(block, "model").isEmpty() ? "qwen2.5" : blockValue(block, "model");
        msg["stream"] = false;
        msg["prompt"] = body;
        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl(ep + "/api/generate"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = nam.post(req, QJsonDocument(msg).toJson());
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        const QByteArray raw = reply->readAll();
        const bool netOk = reply->error() == QNetworkReply::NoError;
        const QString netErr = reply->errorString();
        reply->deleteLater();
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
        showAnswer(engine, block, responseText, thinkingText);
        return;
    }

    const bool refuseIfDirty = scalar(yamlText(), "refuseIfDirty") != "false";
    if (refuseIfDirty && (editorDirty() || gitDirty(hostRoot()))) {
        warn("Refusé : tampon LiteIDE sale ou git dirty sur " + hostRoot());
        return;
    }
    showLoadingDialog(engine);
    const QString container = activeCline();
    const QString inBox = "/tmp/agentplurai-prompt.txt";
    const QString logDir = containerRoot() + "/.agentplurai";
    run("docker", {"exec", container, "mkdir", "-p", logDir}, 8000);
    QProcess p;
    p.start("docker", {"exec", "-i", container, "tee", inBox});
    p.waitForStarted(5000);
    p.write(body.toUtf8());
    p.closeWriteChannel();
    p.waitForFinished(8000);
    const QString script = "prompt=$(cat " + inBox + "); exec cline \"$prompt\"";
    const QString out = run("docker", {"exec", "-w", containerRoot(), container, "bash", "-lc", script}, 120000);
    const QString log = logDir + "/prompts.log";
    const QString line = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + " " + engine + "\n" + task + "\n";
    QProcess logp;
    logp.start("docker", {"exec", "-i", container, "tee", "-a", log});
    logp.waitForStarted(5000);
    logp.write(line.toUtf8());
    logp.closeWriteChannel();
    logp.waitForFinished(8000);
    hideLoadingDialog();
    QString response;
    QString thinking;
    splitCline(out, &response, &thinking);
    showAnswer(engine, block, response.isEmpty() ? "Cline sans sortie" : response, thinking);
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
        const QString path = scalar(block, "container") + "/.agentplurai/prompts.log";
        out = run("docker", {"exec", engine, "cat", path}, 8000);
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
        const QString path = scalar(block, "container") + "/.agentplurai/actions.log";
        out = run("docker", {"exec", engine, "cat", path}, 8000);
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
    info("Révision proposée, non fusionnée automatiquement.\nEnregistre le bloc à la main dans " + contractPath() + "\n\n" + next);
}

void LiteIdeAfPlugin::showQuota()
{
    if (dryRun()) {
        info("dryRun : quota non interrogé.");
        return;
    }
    const QString out = run("docker", {"exec", activeCline(), "cline", "auth"}, 15000);
    info("Sortie locale de Cline (pas la console Z.ai) :\n" + out);
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
    const QString journal = run("docker", {"exec", activeCline(), "cat", containerRoot() + "/.agentplurai/actions.log"}, 8000);
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
