#ifndef LITEIDEAFPLUGIN_H
#define LITEIDEAFPLUGIN_H

#include "liteapi/liteapi.h"

class QDialog;
class QLabel;
class QTimer;
class QNetworkReply;
class QProcess;

class LiteIdeAfPlugin : public LiteApi::IPlugin
{
    Q_OBJECT
public:
    LiteIdeAfPlugin();
    virtual bool load(LiteApi::IApplication *app);

private slots:
    void health();
    void showScope();
    void sendPrompt();
    void showHistory();
    void editSystemPrompt();
    void showJournal();
    void showQuota();
    void reloadWrites();
    void setDefaultAI();
    void onOllamaFinished();
    void onClineFinished(int code, int status);

private:
    QString m_defaultEngine;
    QDialog *m_loadingDialog;
    QLabel *m_loadingLabel;
    QTimer *m_elapsedTimer;
    int m_elapsedSeconds;
    QString m_pendingEngine;
    QString m_pendingBlock;
    QNetworkReply *m_pendingReply;
    QProcess *m_pendingProcess;

    void showLoadingDialog(const QString &engine);
    void hideLoadingDialog();
    void updateElapsed();
    QString contractPath() const;
    QString yamlText() const;
    bool dryRun() const;
    QString activeCline() const;
    QString engineBlock(const QString &name) const;
    QString blockValue(const QString &block, const QString &key) const;
    QString globalPrompt() const;
    QString localPrompt(const QString &engine) const;
    QString hostRoot() const;
    QString containerRoot() const;
    QString scopeText() const;
    bool editorDirty() const;
    bool gitDirty(const QString &root) const;
    QString run(const QString &program, const QStringList &args, int timeoutMs, int *code = nullptr) const;
    void info(const QString &text) const;
    void warn(const QString &text) const;
    void showAnswer(const QString &engine, const QString &block, const QString &response, const QString &thinking) const;

    LiteApi::IApplication *m_app;
};

class PluginFactory : public LiteApi::PluginFactoryT<LiteIdeAfPlugin>
{
    Q_OBJECT
    Q_INTERFACES(LiteApi::IPluginFactory)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "liteidex.LiteIdeAfPlugin")
#endif
public:
    PluginFactory() {
        m_info->setId("plugin/liteideaf");
        m_info->setName("LiteIDE AgentPlurAI");
        m_info->setAuthor("JM Batto");
        m_info->setVer("0.1");
        m_info->setInfo("Multiple AI assistant");
        m_info->appendDepend("plugin/liteeditor");
    }
};

#endif
