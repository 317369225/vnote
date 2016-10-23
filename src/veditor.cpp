#include <QtWidgets>
#include <QTextBrowser>
#include <QWebChannel>
#include <QWebEngineView>
#include "veditor.h"
#include "vedit.h"
#include "vdocument.h"
#include "vnote.h"
#include "utils/vutils.h"
#include "vpreviewpage.h"
#include "hgmarkdownhighlighter.h"
#include "vconfigmanager.h"
#include "vmarkdownconverter.h"

extern VConfigManager vconfig;

VEditor::VEditor(const QString &path, const QString &name, bool modifiable,
                 QWidget *parent)
    : QStackedWidget(parent), mdConverterType(vconfig.getMdConverterType())
{
    DocType docType = isMarkdown(name) ? DocType::Markdown : DocType::Html;
    QString fileText = VUtils::readFileFromDisk(QDir(path).filePath(name));
    noteFile = new VNoteFile(path, name, fileText, docType, modifiable);

    isEditMode = false;

    setupUI();

    showFileReadMode();
}

VEditor::~VEditor()
{
    if (noteFile) {
        delete noteFile;
    }
}

void VEditor::setupUI()
{
    textEditor = new VEdit(noteFile);
    addWidget(textEditor);

    switch (noteFile->docType) {
    case DocType::Markdown:
        setupMarkdownPreview();
        textBrowser = NULL;
        break;

    case DocType::Html:
        textBrowser = new QTextBrowser();
        addWidget(textBrowser);
        textBrowser->setFont(vconfig.getBaseEditFont());
        webPreviewer = NULL;
        break;
    default:
        qWarning() << "error: unknown doc type" << int(noteFile->docType);
    }
}

bool VEditor::isMarkdown(const QString &name)
{
    const QVector<QString> mdPostfix({"md", "markdown", "mkd"});

    QStringList list = name.split('.', QString::SkipEmptyParts);
    if (list.isEmpty()) {
        return false;
    }
    const QString &postfix = list.last();
    for (int i = 0; i < mdPostfix.size(); ++i) {
        if (postfix == mdPostfix[i]) {
            return true;
        }
    }
    return false;
}

void VEditor::showFileReadMode()
{
    isEditMode = false;
    switch (noteFile->docType) {
    case DocType::Html:
        textBrowser->setHtml(noteFile->content);
        setCurrentWidget(textBrowser);
        break;
    case DocType::Markdown:
        if (mdConverterType == MarkdownConverterType::Marked) {
            document.setText(noteFile->content);
        } else {
            previewByConverter();
        }
        setCurrentWidget(webPreviewer);
        break;
    default:
        qWarning() << "error: unknown doc type" << int(noteFile->docType);
    }
}

void VEditor::previewByConverter()
{
    VMarkdownConverter mdConverter;
    QString content = noteFile->content;
    QString html = mdConverter.generateHtml(content, vconfig.getMarkdownExtensions());
    QRegularExpression tocExp("<p>\\[TOC\\]<\\/p>", QRegularExpression::CaseInsensitiveOption);
    QString toc = mdConverter.generateToc(content, vconfig.getMarkdownExtensions());
    html.replace(tocExp, toc);
    QString completeHtml = VNote::preTemplateHtml + html + VNote::postTemplateHtml;
    webPreviewer->setHtml(completeHtml, QUrl::fromLocalFile(noteFile->path + QDir::separator()));
}

void VEditor::showFileEditMode()
{
    isEditMode = true;
    textEditor->beginEdit();
    setCurrentWidget(textEditor);
    textEditor->setFocus();
}

bool VEditor::requestClose()
{
    readFile();
    return !isEditMode;
}

void VEditor::editFile()
{
    if (isEditMode || !noteFile->modifiable) {
        return;
    }

    showFileEditMode();
}

void VEditor::readFile()
{
    if (!isEditMode) {
        return;
    }

    if (textEditor->isModified()) {
        // Need to save the changes
        QMessageBox msgBox(this);
        msgBox.setText("The note has been modified.");
        msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret) {
        case QMessageBox::Save:
            saveFile();
            // Fall through
        case QMessageBox::Discard:
            textEditor->reloadFile();
            break;
        case QMessageBox::Cancel:
            // Nothing to do if user cancel this action
            return;
        default:
            qWarning() << "error: wrong return value from QMessageBox:" << ret;
            return;
        }
    }
    textEditor->setReadOnly(true);
    showFileReadMode();
}

bool VEditor::saveFile()
{
    if (!isEditMode || !noteFile->modifiable || !textEditor->isModified()) {
        return true;
    }
    textEditor->saveFile();
    bool ret = VUtils::writeFileToDisk(QDir(noteFile->path).filePath(noteFile->name),
                                       noteFile->content);
    if (!ret) {
        QMessageBox msgBox(QMessageBox::Warning, tr("Fail to save to file"),
                           QString("Fail to write to disk when saving a note. Please try it again."),
                           QMessageBox::Ok, this);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        textEditor->setModified(true);
        return false;
    }
    textEditor->setModified(false);
    return true;
}

void VEditor::setupMarkdownPreview()
{
    webPreviewer = new QWebEngineView(this);
    VPreviewPage *page = new VPreviewPage(this);
    webPreviewer->setPage(page);

    if (mdConverterType == MarkdownConverterType::Marked) {
        QWebChannel *channel = new QWebChannel(this);
        channel->registerObject(QStringLiteral("content"), &document);
        page->setWebChannel(channel);
        webPreviewer->setHtml(VNote::templateHtml, QUrl::fromLocalFile(noteFile->path + QDir::separator()));
    }

    addWidget(webPreviewer);
}
