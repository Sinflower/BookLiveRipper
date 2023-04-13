#include "bookdownloadandexport.h"
#include "ui_bookdownloadandexport.h"

#include "decoder.h"

#include <QKeyEvent>
#include <QFile>
#include <QDebug>
#include <QtConcurrent>
#include <QMenu>
#include <QTextEdit>

#define DEFAUT_MODE

#define UNICODE_CHARACTER_REGEX        "\\u([0-9a-f]{4})"
#define ENCODED_CHARACTER_REGEX        "&\\$([0-9a-z]+)\\;"
#define BROKEN_UNICODE_CHARACTER_REGEX "(&#x[0-9a-f]{4})(?!;)"
#define T_PARAM_START_REGEX            "(<t-param[^>]*>\n?)"
#define T_PARAM_END_REGEX              "(</t-param[^>]*>\n?)"

#define T_IMG_REGEX                    "(<t-img [^>]*>\n?)"
#define T_IMG_SRC_REGEX                "src=\\\"([^\\\"]*)\\\""

#define T_CODE_IMG_REGEX               "(<t-code src=[^>]*>\n?)"

#define T_FONT_REMOVE_REGEX            "<t-font [^>]*></t-font>"
#define T_FONT_REGEX                   "<t-font (t-class=\"([^\"]*)\")?\\s?(xsize=\"([^\"]*)\")?[^>]*>"
#define T_FONT_CLOSE_TAG               "</t-font>"

#define HEADER_REGEX                   "<a href=\"([^\"]*)\">([^<]*)</a>"
#define T_YOKO_START                   "<t-yoko>"
#define T_YOKO_END                     "</t-yoko>"

#define HEADER_START                   "<div>"
#define HEADER_END                     "</div>"
#define HEADER_H1_MODE                 "<h1[^>]*>([^<]*)<\\/h1>"

#define HREF_TARGET_QSTRING            "<a name=\"%1\"></a>"
#define HTML_LINE_BREAK                "<div><br></div>"

#define DIV_REGEX                      "(<div [^>]*>)"
#define UNICODE_SPACE                  "&#x3000;"
#define NON_BREAK_SPACE                "&nbsp;"

#define EXPORT_IMG_TAG_CLEAN_REGEX     "(<img src=\\\"([^\\\"]*)\\\" [^>]*>\n?)"
#define EXPORT_TYPE_PROPERTY           "ExportType"

#define STATUS_MESSAGE(msg) \
{ \
	emit updateStatus(msg); \
	qDebug("[%s]: %s", __func__, msg); \
	}

#define SET_INFINIT_PROGRESS updateProgress(0, 0, 0);

#define INC_PROGRESS(PROGRESS) updateProgress(-1, -1, PROGRESS->value() + 1);
#define ADD_2_PROGRESS(PROGRESS, VALUE) updateProgress(-1, -1, PROGRESS->value() + VALUE);

// TODO: Half of this looks like shit I normally scream at people for ...
// make it look less like shit some moron wrote

BookDownloadAndExport::BookDownloadAndExport(QWidget *parent) :
	QDialog(parent),
	m_pUi(new Ui::BookDownloadAndExport),
	m_configDone(false),
	m_pDecoder(nullptr),
	m_book(""),
	m_decodedBookHtml(""),
	m_pWatcher(new QFutureWatcher<void>()),
	m_pExportMenu(new QMenu("Export")),
	m_pTxtSplit(new QAction(tr("txt - Split"))),
	m_pTxt(new QAction(tr("txt"))),
	m_pHtml(new QAction(tr("HTML"))),
	m_pDocx(new QAction(tr("Docx")))
{
	m_pUi->setupUi(this);
	connect(this, SIGNAL(updateStatus(QString)), this, SLOT(updateStatusMsg(QString)), Qt::UniqueConnection);

	m_pTxtSplit->setProperty(EXPORT_TYPE_PROPERTY, TXT_SPLIT);
	m_pTxt->setProperty(EXPORT_TYPE_PROPERTY, TXT);
	m_pHtml->setProperty(EXPORT_TYPE_PROPERTY, HTML);
	m_pDocx->setProperty(EXPORT_TYPE_PROPERTY, DOCX);
	m_pDocx->setEnabled(false); // TODO: Enable when docx export is implemented

	m_pExportMenu->addAction(m_pTxtSplit);
	m_pExportMenu->addAction(m_pTxt);
	m_pExportMenu->addAction(m_pHtml);
	m_pExportMenu->addAction(m_pDocx);

	connect(m_pTxtSplit, SIGNAL(triggered()), this, SLOT(exportBook()));
	connect(m_pTxt, SIGNAL(triggered()), this, SLOT(exportBook()));
	connect(m_pHtml, SIGNAL(triggered()), this, SLOT(exportBook()));
	connect(m_pDocx, SIGNAL(triggered()), this, SLOT(exportBook()));

	m_pUi->pushButton_Export->setEnabled(true);
	m_pUi->pushButton_Export->setMenu(m_pExportMenu);
}

BookDownloadAndExport::~BookDownloadAndExport()
{
	delete m_pUi;
	delete m_pWatcher;
	delete m_pExportMenu;
	delete m_pTxt;
	delete m_pHtml;
	delete m_pDocx;
}

void BookDownloadAndExport::SetCookies(QList<QNetworkCookie> cookies)
{
	m_cookies = cookies;
}

void BookDownloadAndExport::Load(QString bookID)
{
	if(!m_pWatcher->isRunning())
	{
		resetAll();

		m_pWatcher->setFuture(QtConcurrent::run(this, &BookDownloadAndExport::load, bookID));
	}
}

// Disable dialog close when escape is pressed
void BookDownloadAndExport::keyPressEvent(QKeyEvent *pEvent)
{
	if(pEvent->key() != Qt::Key_Escape)
		QDialog::keyPressEvent(pEvent);
}


void BookDownloadAndExport::on_pushButton_Done_clicked()
{
	accept();
}

void BookDownloadAndExport::on_pushButton_Cancel_clicked()
{
	// TODO: Do stop stuff

	reject();
}

void BookDownloadAndExport::updateStatusMsg(QString msg)
{
	m_pUi->label_Status->setText(msg);
}

void BookDownloadAndExport::exportBook()
{
	if(m_decodedBookHtml.isEmpty())
	{
		STATUS_MESSAGE("No decoded book found stopping export.");
		return;
	}

	QObject *pSender = sender();
	if(!pSender)
		return;

	int exportType = pSender->property(EXPORT_TYPE_PROPERTY).toInt();

	QDir dir = QDir::current();
	QString pathStr = QString("%1%2/%3").arg(m_bookID).arg(m_trial ? " (trial)" : "").arg(getExportString(exportType));

	if(!m_images.isEmpty())
	{
		QFileInfo d(m_images.values().first());
		QString imgPath = QString("%1/%2").arg(pathStr).arg(d.dir().path());

		if(!dir.exists(imgPath))
			dir.mkpath(imgPath);
	}

	dir.mkpath(pathStr);
	QDir::setCurrent(QString("%1/%2").arg(dir.path()).arg(pathStr));


	// Copy images
	foreach(QString img, m_images)
		QFile::copy(QString("%1/%2").arg(dir.path()).arg(img), img);

	switch(exportType)
	{
		case TXT_SPLIT:
		{
			STATUS_MESSAGE("Exporting book as txt split by chapters ...");

			QTextEdit te;
			QString htmlBook = m_decodedBookHtml;

			// Replace image tags with the image name
			QRegExp rx(EXPORT_IMG_TAG_CLEAN_REGEX);
			while(rx.indexIn(htmlBook) != -1)
				htmlBook.replace(rx.cap(1), QString("[%1]").arg(rx.cap(2)));

			QString plainText = "";
			QString subStr;
			QString chapName = "";
			int startPrevChap = -1;
			int chapCnt = 1;
			int posEnd = 0;
			int posStart = 0;

			foreach(QString link, m_knownHeaderLinks)
			{
#ifdef DEFAUT_MODE
				posEnd = htmlBook.indexOf("</div>", htmlBook.indexOf(QString(HREF_TARGET_QSTRING).arg(link)));
				posStart = htmlBook.lastIndexOf("<div>", posEnd);
#else
				posEnd = htmlBook.indexOf("</h1>", htmlBook.indexOf(QString(HREF_TARGET_QSTRING).arg(link)));
				posStart = htmlBook.lastIndexOf("<h1", posEnd);
#endif

				// TODO: Method or stuff
				if(startPrevChap != -1)
				{
					subStr = htmlBook.mid(startPrevChap, posStart - startPrevChap);
					te.setHtml(subStr);

					plainText = te.toPlainText();
					// Due to the slightly broken html code Qt double counts linebreaks
					plainText.replace("\n\n", "\n");
					// Replace normal spaces with ideographic space
					plainText.replace(" ", "　");

					QFile f(QString("%1 - %2.txt").arg(chapCnt).arg(chapName));
					if(f.open(QIODevice::WriteOnly | QIODevice::Text))
					{
						QTextStream out(&f);
						out.setCodec("UTF-8");
						out << plainText;
						f.close();
						chapCnt++;
					}
					else
						qDebug("[%s]: Unable to open output file: ", __func__, qPrintable(f.fileName()));
				}

				startPrevChap = posStart;
				te.setHtml(m_headerLinkValueHash.value(link));
				chapName = te.toPlainText();
				chapName.replace("?", "？");
			}

			if(startPrevChap != -1)
			{
				subStr = htmlBook.mid(startPrevChap, htmlBook.length() - startPrevChap);
				QTextEdit te;
				te.setHtml(subStr);

				plainText = te.toPlainText();
				// Due to the slightly broken html code Qt double counts linebreaks
				plainText.replace("\n\n", "\n");
				// Replace normal spaces with ideographic space
				plainText.replace(" ", "　");

				QFile f(QString("%1 - %2.txt").arg(chapCnt).arg(chapName));
				if(!f.open(QIODevice::WriteOnly | QIODevice::Text))
					qDebug("[%s]: Unable to open output file: ", __func__, qPrintable(f.fileName()));

				QTextStream out(&f);
				out.setCodec("UTF-8");
				out << plainText;
				f.close();
			}

			STATUS_MESSAGE("Exporting book as txt split by chapters ... done");
			break;
		}
		case TXT:
		{
			STATUS_MESSAGE("Exporting book as txt ...");

			QTextEdit te;
			QString htmlBook = m_decodedBookHtml;

			// Replace image tags with the image name
			QRegExp rx(EXPORT_IMG_TAG_CLEAN_REGEX);
			while(rx.indexIn(htmlBook) != -1)
				htmlBook.replace(rx.cap(1), rx.cap(2));

			te.setHtml(htmlBook);

			// TODO: Option for split by chapter?
			QString plainText = te.toPlainText();
			// Due to the slightly broken html code Qt double counts linebreaks
			plainText.replace("\n\n", "\n");
			// Replace normal spaces with ideographic space
			plainText.replace(" ", "　");

			QFile f("book.txt");
			if(!f.open(QIODevice::WriteOnly | QIODevice::Text))
			{
				qDebug("[%s]: Unable to open output file: ", __func__, qPrintable(f.fileName()));
				STATUS_MESSAGE("Exporting book as txt ... failed");
				break;
			}

			QTextStream out(&f);
			out.setCodec("UTF-8");
			out << plainText;
			f.close();

			STATUS_MESSAGE("Exporting book as txt ... done");
			break;
		}
		case HTML:
		{
			STATUS_MESSAGE("Exporting book as html ...");

			QFile f;

			f.setFileName("book.html");
			if(!f.open(QIODevice::WriteOnly | QIODevice::Text))
			{
				qDebug("[%s]: Unable to open output file: ", __func__, qPrintable(f.fileName()));
				STATUS_MESSAGE("Exporting book as html ... failed");
				break;
			}

			QTextStream out(&f);
			out.setCodec("UTF-8");
			out << m_decodedBookHtml;
			f.close();

			STATUS_MESSAGE("Exporting book as html ... done");
			break;
		}
		case DOCX:
			STATUS_MESSAGE("Exporting book as docx ...");
            // TODO: Add word stuff
			//       Also first check whether or not word is available
			STATUS_MESSAGE("Exporting book as docx ... done");
			break;
		default:
			STATUS_MESSAGE("Error: Unknown export type");
			break;
	}

	QDir::setCurrent(dir.path());
}

/*
 * Private
 */

void BookDownloadAndExport::load(QString bookID)
{
	if(m_cookies.isEmpty())
	{
		qDebug("[%s]: No login cookies set.", __func__);
		return;
	}

	SET_INFINIT_PROGRESS;

	loadConfig(bookID);
	loadBook();

	QString content = m_book;

	// --------------- Fixing broken characters --------
	STATUS_MESSAGE("Fixing broken unicode characters...");

	QRegExp rx(BROKEN_UNICODE_CHARACTER_REGEX);
	updateProgress(0, content.count(rx), 0);

	int pos = 0;
	while((pos = rx.indexIn(content, pos)) != -1)
	{
		QString fixed = rx.cap(1) + ";";

		ADD_2_PROGRESS(m_pUi->progressBar_download, content.count(rx.cap(1)));
		content.replace(rx.cap(1), fixed);

		pos += rx.matchedLength();
	}
	// --------------- Fixing broken characters --------

	// --------------- Decode encoded characters -------
	STATUS_MESSAGE("Decoding encoded characters...");

	rx = QRegExp(ENCODED_CHARACTER_REGEX);
	updateProgress(0, content.count(rx), 0);
	pos = 0;

	while(rx.indexIn(content) != -1)
	{
		QString decStr = m_pDecoder->DecodeCharacter(rx.cap(1));

		if(!decStr.isEmpty())
		{
			ADD_2_PROGRESS(m_pUi->progressBar_download, content.count(rx.cap(0)));
			content.replace(rx.cap(0), decStr);
		}
	}
	// --------------- Decode encoded characters -------

	// --------------- Cleanup code --------------------
	STATUS_MESSAGE("Cleaning up code...");

	rx = QRegExp(T_PARAM_START_REGEX);
	updateProgress(0, content.count(rx) + content.count(QRegExp(T_PARAM_END_REGEX)) + content.count(QRegExp(DIV_REGEX)), 0);

	while(rx.indexIn(content) != -1)
	{
		ADD_2_PROGRESS(m_pUi->progressBar_download, content.count(rx.cap(1)));
		content.replace(rx.cap(1), "");
	}

	rx = QRegExp(T_PARAM_END_REGEX);

	while(rx.indexIn(content) != -1)
	{
		ADD_2_PROGRESS(m_pUi->progressBar_download, content.count(rx.cap(1)));
		content.replace(rx.cap(1), "");
	}

	rx = QRegExp(DIV_REGEX);

	while(rx.indexIn(content) != -1)
	{
		ADD_2_PROGRESS(m_pUi->progressBar_download, content.count(rx.cap(1)));
		content.replace(rx.cap(1), "<div>");
	}
	// --------------- Cleanup code --------------------

	// --------------- Descramble normal images --------
	STATUS_MESSAGE("Downloading and descrambling normal images...");
	SET_INFINIT_PROGRESS;

	rx = QRegExp(T_IMG_REGEX);

	pos = 0;
	QSet<QString> images;
	while((pos = rx.indexIn(content, pos)) != -1)
	{
		pos += rx.matchedLength();

		QRegExp srcRx(T_IMG_SRC_REGEX);
		if(srcRx.indexIn(rx.cap(1)) != -1)
			images.insert(srcRx.cap(1));
	}

	updateProgress(0, images.count(), 0);

	foreach(QString i, images)
	{
		loadImage(i);
		INC_PROGRESS(m_pUi->progressBar_download);
		m_images.insert(i);
	}


	// --------------- Descramble normal images --------

	// --------------- Descramble base64 images --------
	STATUS_MESSAGE("Downloading and descrambling base64 images...");
	SET_INFINIT_PROGRESS;

	rx = QRegExp(T_CODE_IMG_REGEX);

	pos = 0;
	images.clear();
	while((pos = rx.indexIn(content, pos)) != -1)
	{
		pos += rx.matchedLength();

		QRegExp srcRx(T_IMG_SRC_REGEX);
		if(srcRx.indexIn(rx.cap(1)) != -1)
			images.insert(srcRx.cap(1));
	}

	updateProgress(0, images.count(), 0);

	foreach(QString i, images)
	{
		loadB64Image(i);
		INC_PROGRESS(m_pUi->progressBar_download);
		m_images.insert(i);
	}
	// --------------- Descramble base64 images --------

	// --------------- Convert image tags --------------
	STATUS_MESSAGE("Converting image tags...");
	SET_INFINIT_PROGRESS;

	// Change from strange custom img tag to normal html img tag
	content.replace("<t-img ", "<img ");
	content.replace("<t-code src=", "<img src=");
	// --------------- Convert image tags --------------

	// --------------- Convert font tags ---------------
	STATUS_MESSAGE("Converting font tags...");
	content.remove(QRegExp(T_FONT_REMOVE_REGEX));
	content.remove(QRegExp(T_FONT_REGEX));
	content.remove(T_FONT_CLOSE_TAG);
	// Does not work at the moment, fucks up spaces and layout
	/*	rx = QRegExp(T_FONT_REGEX);
	while((pos = rx.indexIn(content)) != -1)
	{
		bool bold = isFontBold(rx.cap(2));
		QString fontPercent = rx.cap(4).isEmpty() ? "100%" : rx.cap(4);
		content.replace(pos, rx.cap(0).length(), QString("%1<p style=\"font-size:%2\">").arg((bold ? "<b>" : "<p>")).arg(fontPercent));
		pos = content.indexOf(T_FONT_CLOSE_TAG, pos);
		content.replace(pos, QString(T_FONT_CLOSE_TAG).length(), QString("</p>%1").arg((bold ? "</b>" : "")));
	}*/
	// --------------- Convert font tags ---------------


	// --------------- Detect chapter header -----------
	STATUS_MESSAGE("Detecting chapter header...");
	rx = QRegExp(HEADER_REGEX);
	pos = 0;
	int posEnd = 0;
	int posStart = 0;
	QString subStr;

	content.remove(QRegExp(T_YOKO_START));
	content.remove(QRegExp(T_YOKO_END));

	QString link = "";

	while((pos = rx.indexIn(content, pos)) != -1)
	{
		link = rx.cap(1);
		if(link.startsWith("#"))
			link.remove(0, 1);

		if(!m_knownHeaderLinks.contains(link))
		{
			if(content.contains(QString(HREF_TARGET_QSTRING).arg(link)))
			{
				m_knownHeaderLinks.append(link);
				m_headerLinkValueHash.insert(link, rx.cap(2));
			}
		}

		pos += rx.matchedLength();
	}

#ifdef DEFAUT_MODE
	foreach(link, m_knownHeaderLinks)
	{
		// Search for div close tag starting at the current link target anotation
		posEnd = content.indexOf(HEADER_END, content.indexOf(QString(HREF_TARGET_QSTRING).arg(link)));
		// Search for div start tag starting from the end tag
		posStart = content.lastIndexOf(HEADER_START, posEnd);
		// Create a substring containing the div block with the header
		subStr = content.mid(posStart, (posEnd + QString(HEADER_END).length()) - posStart);
		// Create a copy of the string, the old is kept in its original state to ensure replace works
		QString modStr = subStr;
		// Remove div tags
		modStr.remove(HEADER_START);
		modStr.remove(HEADER_END);
		// Add line breaks and bold tag
		modStr = QString("%1%1<div><b>%2</b></div>").arg(HTML_LINE_BREAK).arg(modStr);
		if(modStr.contains("&#x") && content.count(subStr) == 1 && !subStr.contains("<img"))
			content.replace(subStr, modStr);
	}
#endif


	// --------------- Detect chapter header -----------

	// --------------- Converting spaces ---------------
	STATUS_MESSAGE("Converting spaces...");
	SET_INFINIT_PROGRESS;

	// Because Qt removes leading spaces
	content.replace(UNICODE_SPACE, NON_BREAK_SPACE);
	// --------------- Converting spaces ---------------

	// --------------- Save and display ----------------
#ifdef DEBUG
	QFile f;

	f.setFileName("content_formatted_decoded.html");
	if(!f.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		qDebug("[%s]: Unable to open output file: ", __func__, qPrintable(f.fileName()));
		exit(-1);
	}

	QTextStream out(&f);
	out.setCodec("UTF-8");
	out << content;
	f.close();
#endif

	m_decodedBookHtml = content;

	STATUS_MESSAGE("All done");
	updateProgress(0, 1, 1);

	QMetaObject::invokeMethod(m_pUi->pushButton_Done,
							  "setEnabled",
							  Qt::QueuedConnection,
							  Q_ARG(bool, true));

	QMetaObject::invokeMethod(m_pUi->pushButton_Export,
							  "setEnabled",
							  Qt::QueuedConnection,
							  Q_ARG(bool, true));

	// --------------- Save and display ----------------
}

QString BookDownloadAndExport::genDecodeKey()
{
	QString decodeKey = "";

	int l = 16;
	QString d = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	QString e = m_bookID;
	QString r = "";
	for (int f = 0; f < l; f++)
		r.append(d.at(qrand() % d.length()));

	QString c = e + e;
	QString s = c.mid(0, l);
	QString t = c.mid(c.length() - l, l);
	int x = 0;
	int y = 0;
	int z = 0;

	for(int i = 0; i < r.length(); i++)
	{
		x ^= r.at(i).toLatin1();
		y ^= s.at(i).toLatin1();
		z ^= t.at(i).toLatin1();
		decodeKey.append(r.at(i));
		decodeKey.append(d.at((x + y + z) & 0x3F));
	}

#if 0
	QDate date = QDate::currentDate();
	QTime time = QTime::currentTime();

	QString charset = QString("%1%2ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz").arg(date.toString("yyyyMMdd")).arg(time.toString("HHmmsszzz"));

	for (int i = 0; i < 32; i++)
		decodeKey.append(charset[qrand() % (charset.length() - 1)]);

#endif
	return decodeKey;
}

void BookDownloadAndExport::loadConfig(QString bookID)
{
	STATUS_MESSAGE("Loading book configuration...");

	m_bookID = bookID;
	m_decodeKey = genDecodeKey();
	QByteArray config = Download::Get_sync_raw(QString("https://booklive.jp/bib-api/bibGetCntntInfo?cid=%1&k=%2").arg(m_bookID).arg(m_decodeKey),
											   QNetworkProxy::DefaultProxy, &m_cookies, COOKIE_SET);

	if(config.isEmpty())
	{
		qDebug("[%s]: Error while download the config form the server", __func__);
		exit(-1);
	}

	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(config);

	if(json.isEmpty())
	{
		qDebug("[%s]: Unable to parse config file", __func__);
		exit(-1);
	}

	if(json.isObject())
	{
		if(json.object().contains("items"))
		{
			if(json.object()["items"].isArray())
			{
				if(!json.object()["items"].toArray().isEmpty())
				{
					QJsonObject jObj = json.object()["items"].toArray().at(0).toObject();
					m_stbl    = jObj["stbl"].toString();
					m_ttbl    = jObj["ttbl"].toString();
					m_ptbl    = jObj["ptbl"].toString();
					m_ctbl    = jObj["ctbl"].toString();
					m_pID     = jObj["p"].toString();
					m_cServer = jObj["ContentsServer"].toString();

					m_trial = m_cServer.contains("trial");

					m_pDecoder = new Decoder(m_bookID, m_decodeKey, m_ptbl, m_ctbl, m_stbl, m_ttbl);

					m_configDone = true;
					return;
				}
			}
		}
	}

	qDebug("[%s]: Error while parsing json: %s", __func__, qPrintable(err.errorString()));
	exit(-1);
}

void BookDownloadAndExport::loadBook()
{
	if(!m_configDone)
	{
		qDebug("[%s]: Error configuration not done yet", __func__);
		return;
	}

	STATUS_MESSAGE("Downloading book...");

	QByteArray bookJsonData;

	if(m_trial)
	{
		QString data = Download::Get_sync(QString("%1/content.js").arg(m_cServer));
		data.remove("DataGet_Content(");
		data.chop(1);
		bookJsonData.append(data);
	}
	else
		bookJsonData = Download::Get_sync_raw(QString("%1/sbcGetCntnt.php?cid=%2&p=%3").arg(m_cServer).arg(m_bookID).arg(m_pID));

	if(bookJsonData.isEmpty())
	{
		qDebug("[%s]: An error occured while downloading the book", __func__);
	}

	STATUS_MESSAGE("Converting unicode characters...");

	QRegExp rx(UNICODE_CHARACTER_REGEX);

	updateProgress(0, QString(bookJsonData).count(rx), 0);
	int pos = 0;

	while((pos = rx.indexIn(bookJsonData, pos)) != -1)
	{
		QByteArray arr;
		arr.append(QString("&#x%1;").arg(rx.cap(1)));
		ADD_2_PROGRESS(m_pUi->progressBar_download, QString(bookJsonData).count(rx.cap(1)));
		bookJsonData.replace(rx.cap(0), arr);
		pos += rx.matchedLength();
	}

	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(bookJsonData);

	if(!json.isEmpty())
	{
		if(json.isObject())
		{
			if(json.object().contains("ttx"))
			{
				m_book = json.object()["ttx"].toString();
				STATUS_MESSAGE("Book download successful");
				return;
			}
		}
	}

	qDebug("[%s]: Error while parsing json: %s", __func__, qPrintable(err.errorString()));
	exit(-1);
}

void BookDownloadAndExport::loadImage(QString name)
{
	if(!m_configDone)
	{
		qDebug("[%s]: Error configuration not done yet", __func__);
		return;
	}

	if(QFile::exists(name))
	{
		qDebug("[%s]: File %s already exists, skipping", __func__, qPrintable(name));
		return;
	}

	QByteArray imgData;

	if(m_trial)
	{
		// /M_H.jpg specifies that the image should be maximal resolution and high definition
		imgData = Download::Get_sync_raw(QString("%1/%2/M_H.jpg").arg(m_cServer).arg(name));
	}
	else
		imgData = Download::Get_sync_raw(QString("%1/sbcGetImg.php?cid=%2&src=%3&p=%4").arg(m_cServer).arg(m_bookID).arg(name).arg(m_pID));

	m_pDecoder->DecodeImage(name, imgData, name);
}

void BookDownloadAndExport::loadB64Image(QString name)
{
	if(!m_configDone)
	{
		qDebug("[%s]: Error configuration not done yet", __func__);
		return;
	}

	if(QFile::exists(name))
	{
		qDebug("[%s]: File %s already exists, skipping", __func__, qPrintable(name));
		return;
	}

	QByteArray data;

	if(m_trial)
	{
		// /M_H.jpg specifies that the image should be maximal resolution and high definition
		data = Download::Get_sync_raw(QString("%1/%2/M_H.jpg").arg(m_cServer).arg(name));
	}
	else
		data = Download::Get_sync_raw(QString("%1/sbcGetImgB64.php?cid=%2&src=%3&p=%4").arg(m_cServer).arg(m_bookID).arg(name).arg(m_pID));

	QJsonParseError err;
	QJsonDocument json = QJsonDocument::fromJson(data);

	if(!json.isEmpty())
	{
		if(json.isObject())
		{
			if(json.object().contains("Data"))
			{
				QByteArray imgData;
				imgData.append(json.object()["Data"].toString());
				m_pDecoder->DecodeB64Image(imgData, name);
				return;
			}
		}
	}

	qDebug("[%s]: Error while parsing json: %s", __func__, qPrintable(err.errorString()));
}

void BookDownloadAndExport::updateProgress(int min, int max, int val)
{
	if(min >= 0)
	{
		QMetaObject::invokeMethod(m_pUi->progressBar_download,
								  "setMinimum",
								  Qt::QueuedConnection,
								  Q_ARG(int, min));
	}

	if(max >= 0)
	{
		QMetaObject::invokeMethod(m_pUi->progressBar_download,
								  "setMaximum",
								  Qt::QueuedConnection,
								  Q_ARG(int, max));
	}

	if(val >= 0)
	{
		QMetaObject::invokeMethod(m_pUi->progressBar_download,
								  "setValue",
								  Qt::QueuedConnection,
								  Q_ARG(int, val));
	}
}

void BookDownloadAndExport::resetAll()
{
	m_pUi->pushButton_Done->setEnabled(false);
	m_pUi->pushButton_Export->setEnabled(false);
	m_decodedBookHtml.clear();
	m_images.clear();
	m_ptbl.clear();
	m_ctbl.clear();
	m_stbl.clear();
	m_ttbl.clear();
	m_decodeKey.clear();
	m_bookID.clear();
	m_pID.clear();
	m_cServer.clear();
	m_book.clear();
	m_decodedBookHtml.clear();
	m_knownHeaderLinks.clear();
	m_trial = false;
	m_configDone = false;

}

QString BookDownloadAndExport::getExportString(int type)
{
	switch(type)
	{
		case TXT_SPLIT:
			return "txt split";
		case TXT:
			return "txt";
		case HTML:
			return "html";
		case DOCX:
			return "docx";
		default:
			qDebug("[%s]: Unknown ExportType: %d", __func__, type);
			return "UNKNOWN";
	}
}

// Example input "+163 +120"
// one of the values > 100 should result in bold
bool BookDownloadAndExport::isFontBold(QString str)
{
	str.remove("+");
	QStringList nums = str.split(" ");
	foreach(QString num, nums)
	{
		if(num.toInt() > 100)
			return true;
	}

	return false;
}
